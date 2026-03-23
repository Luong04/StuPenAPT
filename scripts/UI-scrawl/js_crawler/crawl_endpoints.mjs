#!/usr/bin/env node

/**
 * UI-scrawl PlaywrightCrawler
 * ============================
 *
 * Goals:
 * - Crawl same-origin URLs using Crawlee PlaywrightCrawler.
 * - Capture request/response traffic and forward to a Python filter service.
 * - Inject cookies + extra headers (Authorization token) BEFORE any navigation.
 * - Proxy all traffic through Burp (default http://127.0.0.1:8080).
 * - Reveal more UI surface by clicking likely "menu/account/settings" triggers,
 *   then enqueue more links.
 *
 * Note:
 * This file currently does NOT fill forms via playwright-mcp/LMM.
 * That integration requires an architectural decision about browser/CDP sharing.
 */

import { PlaywrightCrawler, log as crawleeLog, ProxyConfiguration } from '@crawlee/playwright';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { createWriteStream, existsSync } from 'node:fs';
import { setTimeout as sleep } from 'node:timers/promises';
import { request as undiciRequest } from 'undici';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const parsed = parseArgs({
  options: {
    url: { type: 'string', short: 'u' },
    maxDepth: { type: 'string', default: '4' },
    maxRequestsPerCrawl: { type: 'string', default: '1000' },
    concurrency: { type: 'string', default: '5' },
    visible: { type: 'boolean', default: false },
    debugFlow: { type: 'boolean', default: false },
    proxyUrl: { type: 'string', default: 'http://127.0.0.1:8080' },
    cookieFile: { type: 'string', default: '../../session_cookies.json' },
    filterService: { type: 'string', default: 'http://127.0.0.1:9000/traffic' },
    outputDir: { type: 'string', default: '../../katana_output' },
    maxRevealClicks: { type: 'string', default: '5' },
  },
  allowPositionals: false,
});

const url = parsed.values.url;
if (!url) {
  console.error('Usage: node crawl_endpoints.mjs --url https://target.tld [--visible] [--debugFlow]');
  process.exit(1);
}

const maxDepthNum = Number(parsed.values.maxDepth);
const maxReqNum = Number(parsed.values.maxRequestsPerCrawl);
const concurrencyNum = Number(parsed.values.concurrency);
const headless = !parsed.values.visible;
const debugFlow = !!parsed.values.debugFlow;
if (debugFlow) {
  crawleeLog.setLevel(crawleeLog.LEVELS.DEBUG);
}

const proxyUrl = parsed.values.proxyUrl;
const filterService = parsed.values.filterService;
const outputDir = parsed.values.outputDir;
const maxRevealClicks = Number(parsed.values.maxRevealClicks);

const cookieFileAbs = resolve(__dirname, parsed.values.cookieFile);
const trafficLogPath = resolve(__dirname, outputDir, 'playwrightcrawler_traffic.jsonl');
const trafficStream = createWriteStream(trafficLogPath, { flags: 'a' });

const origin = new URL(url).origin;

// ────────────────────────────────────────────────────────────────
// Cookie injection (same format as scripts/session_cookies.json)
// ────────────────────────────────────────────────────────────────

async function loadCookiePayloadSafe() {
  try {
    if (!existsSync(cookieFileAbs)) {
      if (debugFlow) crawleeLog.debug(`[cookies] cookieFile not found: ${cookieFileAbs}`);
      return null;
    }
    // Node ESM: dynamic import fs already loaded above, but read file sync for simplicity.
    const { readFileSync } = await import('node:fs');
    const raw = JSON.parse(readFileSync(cookieFileAbs, 'utf-8'));
    if (debugFlow) {
      const cookies = raw?.cookies ? Object.keys(raw.cookies) : [];
      const headers = raw?.headers ? Object.keys(raw.headers) : [];
      crawleeLog.debug(`[cookies] Loaded cookie file. cookies=${cookies.length}, headers=${headers.length}, base_url=${raw?.base_url ?? ''}`);
    }
    return raw;
  } catch (e) {
    crawleeLog.warning(`[cookies] Failed to load cookie file: ${e?.message || e}`);
    return null;
  }
}

function toNodeCookies(session) {
  const parsedBase = session?.base_url ? new URL(session.base_url) : null;
  const domain = parsedBase ? parsedBase.host.split(':')[0] : '';
  const isHttps = parsedBase ? parsedBase.protocol === 'https:' : false;

  const pwCookies = [];
  const rawCookies = session?.cookies && typeof session.cookies === 'object' ? session.cookies : {};
  for (const [name, value] of Object.entries(rawCookies)) {
    if (!value) continue;
    pwCookies.push({
      name,
      value: String(value),
      domain,
      path: '/',
      httpOnly: false,
      secure: isHttps,
      sameSite: 'Lax',
    });
  }

  const cookieString = session?.cookie_string;
  if (cookieString && typeof cookieString === 'string') {
    const existing = new Set(pwCookies.map((c) => c.name));
    for (const part of cookieString.split(';')) {
      const p = part.trim();
      if (!p.includes('=')) continue;
      const [k, ...rest] = p.split('=');
      const name = k.trim();
      const value = rest.join('=').trim();
      if (!name || !value || existing.has(name)) continue;
      pwCookies.push({
        name,
        value,
        domain,
        path: '/',
        httpOnly: false,
        secure: isHttps,
        sameSite: 'Lax',
      });
    }
  }

  return pwCookies;
}

function toExtraHeaders(session) {
  const headers = session?.headers && typeof session.headers === 'object' ? session.headers : {};
  const out = {};
  for (const [k, v] of Object.entries(headers)) {
    if (k.toLowerCase() === 'user-agent') continue;
    if (v === undefined || v === null || v === '') continue;
    out[k] = String(v);
  }
  return out;
}

function isHashRoute(hash) {
  if (!hash || hash === '#') return false;
  const fragment = hash.slice(1);
  // Bắt đầu bằng / → chắc chắn là hash-router (ví dụ: #/, #/login)
  if (fragment.startsWith('/')) return true;
  // Có / ở giữa với ký tự trước và sau → dạng route lồng nhau (ví dụ: #dashboard/view)
  return /^[^?#]+\/[^?#]/.test(fragment);
}

function normalizeUrl(u) {
  try {
    const x = new URL(u);
    // Chỉ xóa hash nếu KHÔNG phải hash-router route
    if (x.hash && !isHashRoute(x.hash)) x.hash = '';
    // Bỏ trailing slash ở pathname (không động vào hash)
    if (x.pathname.endsWith('/') && x.pathname.length > 1) {
      x.pathname = x.pathname.replace(/\/+$/, '');
    }
    return x.toString();
  } catch {
    return u;
  }
}


function isSkippableStatic(u) {
  try {
    const x = new URL(u);
    return /(\.png|\.jpe?g|\.gif|\.svg|\.css|\.js|\.woff2?|\.ttf|\.ico|\.pdf|\.zip|\.rar|\.7z)(\?.*)?$/i.test(x.pathname);
  } catch {
    return false;
  }
}

function shouldKeepUrl(targetUrl) {
  try {
    const u = new URL(targetUrl);
    if (u.origin !== origin) return false;
    if (isSkippableStatic(u)) return false;
    return true;
  } catch {
    return false;
  }
}

// ────────────────────────────────────────────────────────────────
// Traffic + filter forwarding
// ────────────────────────────────────────────────────────────────

// Deduplicate per crawl run (method + url)
const seenRequestKeys = new Set();

function logTraffic(entry) {
  trafficStream.write(JSON.stringify(entry) + '\n');
}

async function sendToFilterService(payload) {
  if (!filterService) return;
  try {
    await undiciRequest(filterService, {
      method: 'POST',
      body: JSON.stringify(payload),
      headers: { 'content-type': 'application/json' },
      throwOnError: false,
    });
  } catch (e) {
    crawleeLog.debug(`[filterService] error: ${e?.message || e}`);
  }
}

// ────────────────────────────────────────────────────────────────
// Reveal strategy (click UI triggers to expose hidden elements)
// ────────────────────────────────────────────────────────────────

const REVEAL_KWS = [
  'menu',
  'sidenav',
  'sidebar',
  'account',
  'profile',
  'user',
  'notification',
  'settings',
];

async function revealUI(page, { maxClicks = 5 } = {}) {
  // We use DOM clicks in page.evaluate to quickly open drawers/modals.
  // This is not as "trustworthy" as real Playwright clicks, but works well for SPA triggers.
  try {
    const clicked = await page.evaluate(({kws, maxClicksInner}) => {
      const candidates = Array.from(document.querySelectorAll(
        'button,[role="button"],[aria-haspopup],.dropdown-toggle,[data-bs-toggle="dropdown"],.navbar-toggler'
      ));

      const isVisible = (el) => {
        const r = el.getBoundingClientRect();
        return r.width > 0 && r.height > 0;
      };

      const kwMatch = (el) => {
        const t = (el.textContent || '').toLowerCase();
        const c = (el.className || '').toLowerCase();
        const a = (el.getAttribute('aria-label') || '').toLowerCase();
        const combo = (t + ' ' + c + ' ' + a);
        return kws.some((kw) => combo.includes(kw));
      };

      const seen = new Set();
      const out = [];
      let count = 0;

      for (const el of candidates) {
        if (count >= maxClicksInner) break;
        if (!isVisible(el)) continue;
        if (el.dataset && el.dataset.uiScrawlClicked === '1') continue;
        if (!kwMatch(el)) continue;

        const label = (el.getAttribute('aria-label') || el.textContent || '').trim().slice(0, 80);
        if (!label || seen.has(label)) continue;
        seen.add(label);

        try {
          el.dataset.uiScrawlClicked = '1';
          el.click();
          out.push(label);
          count += 1;
        } catch {
          // ignore
        }
      }

      return out;
    }, { kws: REVEAL_KWS, maxClicksInner: maxClicks });

    if (debugFlow && clicked && clicked.length) {
      crawleeLog.debug(`[revealUI] clicked triggers: ${clicked.join(' | ')}`);
    }

    // give SPA time to render new panels/buttons
    await sleep(800);
    return clicked || [];
  } catch (e) {
    if (debugFlow) crawleeLog.debug(`[revealUI] failed: ${e?.message || e}`);
    return [];
  }
}

async function extractLinksFromPage(page) {
  // Collect hrefs and common data-* link attributes.
  // We also attempt to extract router/manifest links (best-effort).
  const raw = await page.evaluate(() => {
    const h = new Set();
    const norm = (u) => {
      try {
        const x = new URL(u);
        const fragment = (x.hash || '').slice(1);
        const hashIsRoute = /^[^?#]*\/[^?#]/.test(fragment);
        if (x.hash && !hashIsRoute) x.hash = '';
        if (x.pathname.endsWith('/') && x.pathname.length > 1) {
          x.pathname = x.pathname.replace(/\/+$/, '');
        }
        return x.toString();
      } catch {
        return null;
      }
    };
    
    document.querySelectorAll('a[href]').forEach((a) => {
      const v = a.getAttribute('href');
      if (!v) return;
      if (v.startsWith('javascript:') || v.startsWith('mailto:') || v.startsWith('tel:')) return;
      try { h.add(a.href); } catch {}
    });

    ['data-href', 'data-url', 'data-link'].forEach((attr) => {
      document.querySelectorAll('[' + attr + ']').forEach((el) => {
        const v = el.getAttribute(attr);
        if (!v) return;
        try {
          if (v.startsWith('/')) h.add(location.origin + v);
          else if (v.startsWith('http')) h.add(v);
        } catch {}
      });
    });

    // Next.js manifest pages (best-effort)
    try {
      const nd = window.__NEXT_DATA__;
      if (nd?.buildManifest?.pages) {
        Object.keys(nd.buildManifest.pages).forEach((p) => {
          if (p) h.add(location.origin + p);
        });
      }
    } catch {}

    // React router context (best-effort)
    try {
      const rr = window.__reactRouterContext || window.__routerContext;
      if (rr?.router?.routes) {
        const walk = (rs) => rs && rs.forEach((r) => {
          if (r.path && r.path !== '*') {
            const p = r.path.replace(/^\//, '');
            h.add(location.origin + '/' + p);
          }
          if (r.children) walk(r.children);
        });
        walk(rr.router.routes);
      }
    } catch {}

    return Array.from(h).map(norm).filter(Boolean);
  });

  return raw || [];
}

// ────────────────────────────────────────────────────────────────
// Main crawler
// ────────────────────────────────────────────────────────────────

async function main() {
  const cookieSession = await loadCookiePayloadSafe();
  const injectedContexts = new WeakSet();

  crawleeLog.info(`UI-scrawl starting → ${url}`);
  crawleeLog.info(`Origin scope: ${origin}`);
  crawleeLog.info(`Proxy: ${proxyUrl}`);
  crawleeLog.info(`Headless: ${headless}`);
  crawleeLog.info(`Traffic log: ${trafficLogPath}`);

  const crawler = new PlaywrightCrawler({
    maxConcurrency: concurrencyNum,
    maxRequestsPerCrawl: maxReqNum,
    browserPoolOptions: { useFingerprints: true },
    proxyConfiguration: new ProxyConfiguration({ proxyUrls: [proxyUrl] }),
    launchContext: {
      launchOptions: {
        headless,
        args: [
          '--ignore-certificate-errors',
          '--ignore-ssl-errors',
          '--disable-blink-features=AutomationControlled',
          '--no-sandbox',
        ],
      },
    },
    navigationTimeoutSecs: 60,
    requestHandlerTimeoutSecs: 120,
    preNavigationHooks: [
      async ({ page }) => {
        const ctx = page?.context?.();
        if (!ctx) return;
        if (injectedContexts.has(ctx)) return;

        if (!cookieSession) {
          if (debugFlow) crawleeLog.debug('[cookies] No session cookie loaded, skipping injection');
          injectedContexts.add(ctx);
          return;
        }

        if (debugFlow) {
          crawleeLog.debug('[cookies] Injecting cookies BEFORE navigation...');
        }

        const pwCookies = toNodeCookies(cookieSession);
        const extraHeaders = toExtraHeaders(cookieSession);

        if (pwCookies.length) {
          if (debugFlow) crawleeLog.debug(`[cookies] addCookies: ${pwCookies.map((c) => c.name).slice(0, 10).join(', ')}${pwCookies.length > 10 ? '...' : ''}`);
          await ctx.addCookies(pwCookies);
        }

        const keys = Object.keys(extraHeaders);
        if (keys.length) {
          if (debugFlow) crawleeLog.debug(`[cookies] setExtraHTTPHeaders: ${keys.join(', ')}`);
          ctx.setExtraHTTPHeaders(extraHeaders);
        }

        injectedContexts.add(ctx);
        page.on('request', (req) => {
          const key = `${req.method()} ${req.url()}`;
          if (!seenRequestKeys.has(key) && shouldKeepUrl(req.url())) {
            seenRequestKeys.add(key);
            const entry = {
              type: 'request',
              method: req.method(),
              url: req.url(),
              headers: req.headers(),
              resourceType: req.resourceType(),
              postData: req.postData(),
              ts: Date.now(),
            };
            logTraffic(entry);
            void sendToFilterService(entry);
          }
        });
    // log traffic
        page.on('response', async (res) => {
          const req = res.request();
          const key = `${req.method()} ${req.url()}`;
          if (!shouldKeepUrl(req.url())) return;
          if (!seenRequestKeys.has(key)) return;
          const ct = (res.headers()['content-type'] || '').toLowerCase();
          let bodySnippet = null;
          if (/(json|text|xml|javascript)/.test(ct)) {
            try { bodySnippet = (await res.text()).slice(0, 4096); } catch {}
          }
          logTraffic({ type: 'response', status: res.status(), url: res.url(), headers: res.headers(), contentType: ct, bodySnippet, ts: Date.now() });
        });    page.on('request', (req) => {
          const key = `${req.method()} ${req.url()}`;
          if (!seenRequestKeys.has(key) && shouldKeepUrl(req.url())) {
            seenRequestKeys.add(key);
            const entry = {
              type: 'request',
              method: req.method(),
              url: req.url(),
              headers: req.headers(),
              resourceType: req.resourceType(),
              postData: req.postData(),
              ts: Date.now(),
            };
            logTraffic(entry);
            void sendToFilterService(entry);
          }
        });
    
        page.on('response', async (res) => {
          const req = res.request();
          const key = `${req.method()} ${req.url()}`;
          if (!shouldKeepUrl(req.url())) return;
          if (!seenRequestKeys.has(key)) return;
          const ct = (res.headers()['content-type'] || '').toLowerCase();
          let bodySnippet = null;
          if (/(json|text|xml|javascript)/.test(ct)) {
            try { bodySnippet = (await res.text()).slice(0, 4096); } catch {}
          }
          logTraffic({ type: 'response', status: res.status(), url: res.url(), headers: res.headers(), contentType: ct, bodySnippet, ts: Date.now() });
        });
      },
    ],
    async requestHandler({ page, request, enqueueLinks, enqueueLinksByClickingElements, log }) {
      const pageUrl = request.url;
      const depth = (request.userData?.depth ?? 0) | 0;

      if (!shouldKeepUrl(pageUrl)) {
        if (debugFlow) log.debug(`[skip] out-of-scope/static page: ${pageUrl}`);
        return;
      }

      if (debugFlow) {
        log.debug(`[visit] depth=${depth} url=${pageUrl}`);
      }


      // ── wait for some DOM readiness ───────────────────────
      try {
        await page.waitForLoadState('domcontentloaded', { timeout: 15000 });
      } catch {}

      // Encourage rendering for SPAs by waiting for some interactive element.
      try {
        await page.waitForSelector('a[href], button, input, textarea, form', { timeout: 8000 });
      } catch {}

      // ── Reveal strategy ────────────────────────────────────
      // Click likely triggers to open hidden UI, then extract more links.
      await revealUI(page, { maxClicks: maxRevealClicks });

      // Extract best-effort links after reveal.
      let discoveredLinks = [];
      try {
        discoveredLinks = await extractLinksFromPage(page);
      } catch (e) {
        if (debugFlow) log.debug(`[extractLinks] failed: ${e?.message || e}`);
        discoveredLinks = [];
      }

      // Filter + dedupe + depth guard
      const filtered = [];
      const seen = new Set();
      for (const link of discoveredLinks) {
        if (!link) continue;
        const norm = normalizeUrl(link);
        if (!shouldKeepUrl(norm)) continue;
        if (seen.has(norm)) continue;
        if (depth + 1 > maxDepthNum) continue;
        seen.add(norm);
        filtered.push(norm);
      }

      if (debugFlow) {
        log.debug(`[enqueue] extractedLinks=${filtered.length} depth=${depth + 1}`);
        if (filtered.length) log.debug(`[enqueue] sample=${filtered.slice(0, 5).join(' | ')}`);
      }

      const enqueueRes = await enqueueLinks({
        urls: filtered,
        waitForAllRequestsToBeAdded: true,
        transformRequestFunction: (req) => {
          req.userData = { depth: depth + 1 };
          req.uniqueKey = req.url;  // ← thêm dòng này
          return req;
        },
      });

      if (debugFlow) {
        log.debug(`[enqueue] processed=${enqueueRes.processedRequests.length} unprocessed=${enqueueRes.unprocessedRequests.length}`);
      }

      // Finally: click nav-like elements and intercept navigations.
      // This is last because it may mutate visibility & focus.
      try {
        await Promise.race([
          enqueueLinksByClickingElements({
            selector: 'button,[role="button"],[aria-haspopup],.dropdown-toggle,.navbar-toggler',
            userData: { depth: depth + 1 },
            waitForPageIdleSecs: 1,
            maxWaitForPageIdleSecs: 2,  // giảm xuống
            forefront: false,
            transformRequestFunction: (req) => {
              if (!shouldKeepUrl(req.url)) return null;
              if ((req.userData?.depth ?? depth + 1) > maxDepthNum) return null;
              return req;
            },
          }),
          sleep(12000).then(() => { throw new Error('timeout'); })
        ]);
      } catch (e) {
        if (debugFlow) log.debug(`[enqueueByClick] aborted: ${e?.message}`);
      }
    },
    failedRequestHandler({ request, log }) {
      log.warning(`Request failed many times: ${request.url}`);
    },
  });

  await crawler.run([{ url, userData: { depth: 0 } }]);

  await sleep(500);
  trafficStream.end();
  crawleeLog.info(`Crawler finished. Traffic log: ${trafficLogPath}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});

