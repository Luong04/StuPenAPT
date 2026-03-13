"""
🔄 DFS Crawl Engine — Orchestrator
====================================
Kết nối hai layer chạy song song:

  🎭 BrowserInfra  (Playwright Python) — LUÔN chạy nền, bắt mọi traffic
  🤖 AIAgent       (Playwright-MCP)    — CHỈ gọi khi cần AI tương tác UI

  📦 TrafficStore  — lưu trữ mọi request/response
  🔍 NucleiRunner  — scan security trên endpoints đã bắt

DFS Algorithm:
  ┌──────────────────────────────────────────────────────┐
  │  1. Push start URL vào stack                         │
  │  2. Pop URL → 🎭 navigate (auto-capture traffic)    │
  │  3. Check: page có forms/login?                      │
  │     → Có: gọi 🤖 snapshot + analyze + interact      │
  │     → Không: skip AI, tiếp tục                       │
  │  4. Extract links từ page                            │
  │  5. Push unvisited in-scope links vào stack (DFS)    │
  │  6. Periodic: 🔍 Nuclei scan captured endpoints    │
  │  7. Repeat cho đến khi stack rỗng hoặc đạt limit    │
  └──────────────────────────────────────────────────────┘

Key insight: Khi 🤖 MCP thực thi browser_click() hoặc browser_fill_form(),
             🎭 Playwright page.route() vẫn chạy nền → tự động bắt mọi
             request/response mà AI tạo ra. KHÔNG bỏ sót traffic nào.
"""

import asyncio
import json
import os
import re
import time
from dataclasses import dataclass
from urllib.parse import urlparse

from .ai_agent import AIAgent
from .browser_infra import BrowserInfra
from .nuclei_runner import NucleiRunner
from .traffic_store import TrafficStore

# Static/non-page extensions — skip trong DFS (không navigate vào)
_SKIP_EXTENSIONS = re.compile(
    r'\.(js|css|json|xml|woff2?|ttf|eot|otf|ico|png|jpe?g|gif|svg|webp|avif|'
    r'mp3|mp4|webm|ogg|wav|flac|avi|mkv|mov|pdf|zip|tar|gz|rar|7z|bz2|'
    r'map|wasm|bin|dat|dll|exe|msi|deb|rpm|apk|dmg|iso)'
    r'(\?.*)?$',
    re.IGNORECASE,
)

# URL patterns to always skip
_SKIP_PATTERNS = re.compile(
    r'socket\.io[/\?]|/ws[/\?]?$|__webpack|hot-update|favicon',
    re.IGNORECASE,
)

# Hosts thường chứa dữ liệu nhạy cảm bị leak — cho phép ghé thăm nhanh
_SENSITIVE_HOSTS = re.compile(
    r"s3\.amazonaws\.com|blob\.core\.windows\.net|storage\.googleapis\.com"
    r"|digitaloceanspaces\.com|backblazeb2\.com|r2\.cloudflarestorage\.com",
    re.IGNORECASE,
)

# Extensions/paths thường chứa secrets
_SENSITIVE_PATHS = re.compile(
    r"\.(json|xml|env|key|pem|p12|yaml|yml|config|conf|bak|sql|log|csv)(\?|$)",
    re.IGNORECASE,
)


@dataclass
class CrawlNode:
    """Một node trong DFS tree"""

    url: str
    depth: int
    parent_url: str = ""
    found_forms: bool = False
    ai_interacted: bool = False
    links_extracted: int = 0


class DFSEngine:
    """
    DFS Web Crawler + Security Auditor.

    Playwright chạy liên tục bắt traffic.
    MCP chỉ gọi khi cần AI tương tác.
    """

    def __init__(self, config: dict):
        self._config = config
        self._target = config["target"]

        crawl_cfg = config.get("crawl", {})
        self._max_depth = crawl_cfg.get("max_depth", 10)
        self._max_pages = crawl_cfg.get("max_pages", 500)
        self._scope = crawl_cfg.get("scope_pattern", "")
        self._exclude = crawl_cfg.get("exclude_patterns", [])
        self._ai_trigger = crawl_cfg.get("ai_trigger", {})

        # DFS state
        self._stack: list[CrawlNode] = []
        self._visited: set[str] = set()
        self._crawl_log: list[dict] = []
        self._sensitive_findings: list[dict] = []

        # Sensitive external URLs phát sinh trong lúc crawl/AI interaction
        # _setup_scope_guard() đánh dấu vào đây → xử lý sau mỗi trang
        self._pending_sensitive_urls: set[str] = set()

        # Bốn thành phần
        self._browser = BrowserInfra(config)
        self._traffic = TrafficStore(config)
        self._nuclei = NucleiRunner(config)
        self._agent: AIAgent = None

        # Output
        self._output_dir = config.get("output", {}).get("dir", "./output")
        os.makedirs(self._output_dir, exist_ok=True)

    # ═══════════════════════════════════════════════════════
    #  Main Execution
    # ═══════════════════════════════════════════════════════

    async def run(self):
        """
        Entry point. Thực thi toàn bộ flow:
          Phase 1: Launch 🎭 Playwright (hạ tầng)
          Phase 2: Connect 🤖 MCP (AI eyes)
          Phase 3: DFS crawl loop
          Phase 4: Final 🔍 Nuclei scan
          Phase 5: Generate report
        """
        start_time = time.time()
        print(f"[DFS] ═══ Starting DFS Crawl + Audit ═══")
        print(f"[DFS] Target: {self._target}")
        print(f"[DFS] Max depth: {self._max_depth}, Max pages: {self._max_pages}")

        # ─── Phase 1: Launch browser infrastructure ────────
        await self._browser.launch(self._traffic)
        print(f"[DFS] 🎭 Browser launched (CDP: {self._browser.cdp_endpoint})")
        print(f"[DFS]    → page.route() capturing ALL traffic")
        print(f"[DFS]    → CDP session capturing WebSocket frames")

        # Scope guard: chặn navigation ra ngoài target domain ở tầng network
        # Ngoại lệ: sensitive external URLs (S3, blob storage, .env, .key, ...)
        # được phép đi qua nhưng sẽ bị scan ngay sau đó
        await self._setup_scope_guard()
        target_host = urlparse(self._target).netloc.lower().split(":")[0]
        print(f"[DFS] 🛡 Scope guard active → only {target_host} and subdomains allowed")
        print(f"[DFS]    → Sensitive external URLs (S3/blob/storage) allowed but will be scanned")

        # ─── Load session cookies (login handled externally) ──
        cookie_file = self._config.get("session_cookies")
        if cookie_file and os.path.exists(cookie_file):
            await self._browser.load_session_cookies(cookie_file)

        # ─── Phase 2: Connect AI agent via MCP ────────────
        self._agent = AIAgent(self._config, self._browser.cdp_endpoint)
        self._config["_browser_infra"] = self._browser
        await self._agent.connect()
        print(f"[DFS] 🤖 AI agent connected via MCP")
        print(f"[DFS]    → Connected to SAME browser via CDP endpoint")

        # ─── Phase 3: DFS Crawl Loop ──────────────────────
        self._stack.append(CrawlNode(url=self._target, depth=0))
        pages_crawled = 0

        while self._stack and pages_crawled < self._max_pages:
            node = self._stack.pop()

            # Dedup + scope check
            normalized = self._normalize_url(node.url)
            if normalized in self._visited:
                continue
            if not self._is_in_scope(node.url):
                continue
            if node.depth > self._max_depth:
                continue

            self._visited.add(normalized)
            pages_crawled += 1

            print(
                f"\n[DFS] [{pages_crawled}/{self._max_pages}] "
                f"depth={node.depth} → {node.url[:100]}"
            )

            # ─── 🎭 Playwright: Navigate (traffic auto-captured) ──
            success = await self._browser.navigate(node.url)
            if not success:
                print(f"[DFS]   ✗ Navigation failed")
                self._log_crawl(node, "nav_failed")
                continue

            actual_url = await self._browser.current_url()

            # Scope guard chặn ở tầng network nhưng server-side redirect
            # có thể vượt qua (301/302 trả về trước khi route handler kịp chặn)
            # → check lại sau navigate, kéo browser về nếu bị lạc
            if not self._is_in_scope(actual_url):
                print(f"[DFS]   ✗ Redirected out of scope → {actual_url[:80]}")
                await self._browser.navigate(self._target)
                self._log_crawl(node, "out_of_scope_redirect")
                continue

            title = await self._browser.get_page_title()
            print(f"[DFS]   ✓ {title[:60]}")

            # ─── 🤖 AI analyzing page ──────────────────────
            has_upload = False
            if self._ai_trigger.get("on_forms"):
                elements = await self._browser.has_interactive_elements()
                has_upload = elements.get("file_upload", False)

            print(f"[DFS]   🤖 AI analyzing page...")
            asset_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "asset")
            ai_result = await self._agent.analyze_and_interact(
                {
                    "url": actual_url,
                    "title": title,
                    "purpose": "full_analysis",
                    "has_upload": has_upload,
                    "asset_dir": asset_dir,
                }
            )
            node.ai_interacted = True

            # Log kết quả AI
            actions = ai_result.get("actions_taken", 0)
            sensitive = ai_result.get("sensitive_data", [])
            if sensitive:
                print(f"[DFS]   🔐 Sensitive data found: {len(sensitive)} items")
                self._log_sensitive(actual_url, sensitive)
                for s in sensitive[:5]:
                    print(f"[DFS]       → {s}")
            if actions > 0:
                print(f"[DFS]   🤖 AI completed: {actions} actions")
                for r in ai_result.get("results", []):
                    if r.get("executed"):
                        detail = (r.get("value") or r.get("element") or r.get("snapshot", "")[:60] or "")
                        print(f"[DFS]       → {r['executed']}: ref={r.get('ref', '')} {detail}")
            else:
                print(f"[DFS]   🤖 AI: no actionable elements")

            # Page có thể đã thay đổi sau AI interaction
            await asyncio.sleep(1)

            # ─── Kiểm tra URL sau AI interaction ──────────
            # AI có thể đã click nhầm link → navigate ra ngoài scope
            post_ai_url = await self._browser.current_url()
            if not self._is_in_scope(post_ai_url):
                print(f"[DFS]   ✗ AI navigated out of scope → {post_ai_url[:80]}")
                await self._browser.navigate(actual_url)
                await asyncio.sleep(1)

            # ─── Xử lý sensitive external URLs phát sinh ──
            # Scope guard đã đánh dấu vào _pending_sensitive_urls
            # → ghé thăm nhanh, scan, rồi quay về
            if self._pending_sensitive_urls:
                await self._process_sensitive_externals(actual_url)

            # ─── Extract links → expand DFS ────────────────
            current_url = await self._browser.current_url()
            links = await self._browser.extract_links(current_url)
            in_scope_links = [
                lnk
                for lnk in links
                if self._is_in_scope(lnk)
                and self._is_navigable(lnk)
                and self._normalize_url(lnk) not in self._visited
            ]
            node.links_extracted = len(in_scope_links)

            # Push reversed → DFS: last pushed = first explored
            for link in reversed(in_scope_links):
                self._stack.append(
                    CrawlNode(
                        url=link,
                        depth=node.depth + 1,
                        parent_url=current_url,
                    )
                )

            self._log_crawl(node, "success")
            print(
                f"[DFS]   🔗 {len(in_scope_links)} new links "
                f"(stack: {len(self._stack)}, "
                f"traffic: {self._traffic.total_requests} reqs)"
            )

            # ─── Periodic Nuclei scan ──────────────────────
            if pages_crawled % 20 == 0 and self._nuclei._enabled:
                await self._run_nuclei_scan()

        # ─── Phase 4: Final Nuclei scan ───────────────────
        elapsed = time.time() - start_time
        print(
            f"\n[DFS] ═══ Crawl complete ═══"
            f"\n[DFS] Pages: {pages_crawled} | "
            f"Requests: {self._traffic.total_requests} | "
            f"Time: {elapsed:.1f}s"
        )
        await self._run_nuclei_scan()

        # ─── Phase 5: Report ──────────────────────────────
        self._generate_report(pages_crawled, elapsed)

        # Cleanup
        await self._agent.disconnect()
        await self._browser.close()
        print(f"[DFS] ✓ Done. Results in {self._output_dir}/")

    # ═══════════════════════════════════════════════════════
    #  Scope Guard
    # ═══════════════════════════════════════════════════════

    async def _setup_scope_guard(self):
        """
        Chặn MỌI request ra ngoài target domain ở tầng network.
        - document, xhr, fetch → CHẶN nếu ngoài scope
        - stylesheet, image, font, script → cho qua (CDN assets)
        - popup / tab mới → đóng ngay

        Ngoại lệ: sensitive external URLs (S3, blob, .env, .key...) được phép
        đi qua nhưng bị đánh dấu vào _pending_sensitive_urls để scan sau.
        """
        target_host = urlparse(self._target).netloc.lower().split(":")[0]

        ALLOWED_EXTERNAL_RESOURCES = {
            "stylesheet", "image", "font", "media",
            "manifest", "other", "script",
        }

        def _is_in_scope_host(url: str) -> bool:
            host = urlparse(url).netloc.lower().split(":")[0]
            return host == target_host or host.endswith("." + target_host)

        def _is_sensitive_external(url: str) -> bool:
            return bool(_SENSITIVE_HOSTS.search(url) or _SENSITIVE_PATHS.search(url))

        async def _handle_route(route, request):
            url = request.url
            res_type = request.resource_type

            # In-scope → luôn cho qua
            if _is_in_scope_host(url):
                await route.continue_()
                return

            # Ngoài scope nhưng là sensitive external → đánh dấu scan, cho qua
            if _is_sensitive_external(url):
                print(f"[GUARD] ⚠ Sensitive external → {url[:80]}")
                self._pending_sensitive_urls.add(url)
                await route.continue_()
                return

            # Ngoài scope: CDN assets (css, image, font, script) → cho qua
            if res_type in ALLOWED_EXTERNAL_RESOURCES:
                await route.continue_()
                return

            # Ngoài scope: document, xhr, fetch, websocket → CHẶN
            print(f"[GUARD] 🚫 Blocked {res_type} → {url[:80]}")
            await route.abort()

        await self._browser.page.route("**/*", _handle_route)

        # Chặn popup / tab mới
        self._browser.page.context.on("page", self._handle_new_page)

    async def _handle_new_page(self, page):
        """Khi browser mở tab/popup mới → đóng ngay, thêm vào stack nếu in-scope"""
        await page.wait_for_load_state("commit")
        url = page.url

        if self._is_in_scope(url):
            print(f"[GUARD] 📑 New tab in-scope → closing, added to stack: {url[:80]}")
            normalized = self._normalize_url(url)
            if normalized not in self._visited:
                self._stack.append(CrawlNode(url=url, depth=0, parent_url="popup"))
        else:
            if bool(_SENSITIVE_HOSTS.search(url) or _SENSITIVE_PATHS.search(url)):
                print(f"[GUARD] ⚠ New tab sensitive → {url[:80]}")
                self._pending_sensitive_urls.add(url)
            else:
                print(f"[GUARD] 🚫 New tab out-of-scope → closing: {url[:80]}")

        await page.close()
        
    async def _process_sensitive_externals(self, return_url: str):
        """
        Ghé thăm nhanh các sensitive external URLs được scope guard đánh dấu.
        Snapshot → regex scan → log findings → quay về return_url.
        Không dùng LLM để giữ nhanh.
        """
        pending = list(self._pending_sensitive_urls)
        self._pending_sensitive_urls.clear()

        for ext_url in pending:
            print(f"[DFS]   🔐 Scanning sensitive external: {ext_url[:80]}")
            try:
                await self._browser.navigate(ext_url)
                await asyncio.sleep(1)
                snap = await self._agent.snapshot()
                findings = self._scan_for_sensitive(snap, ext_url)
                if findings:
                    print(f"[DFS]   🔐 Found {len(findings)} items at {ext_url[:60]}")
                    for f in findings[:3]:
                        print(f"[DFS]       → {f[:100]}")
                    self._log_sensitive(ext_url, findings)
                else:
                    print(f"[DFS]   🔐 Nothing sensitive found at {ext_url[:60]}")
            except Exception as e:
                print(f"[DFS]   ✗ Failed to scan {ext_url[:60]}: {e}")

        # Quay về trang đang crawl
        await self._browser.navigate(return_url)
        await asyncio.sleep(1)

    def _scan_for_sensitive(self, text: str, url: str) -> list[str]:
        """
        Scan nhanh bằng regex — không cần LLM cho external URLs.
        Tìm các pattern phổ biến: AWS keys, private keys, JWT, connection strings...
        """
        findings = []
        patterns = {
            "AWS Key":        r"AKIA[0-9A-Z]{16}",
            "Private Key":    r"-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----",
            "API Key":        r"(?i)(api[_-]?key|token|secret)[\"']?\s*[:=]\s*[\"']?[\w\-]{16,}",
            "Email":          r"[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}",
            "Internal IP":    r"\b(10\.|172\.(1[6-9]|2\d|3[01])\.|192\.168\.)\d+\.\d+\b",
            "JWT":            r"eyJ[a-zA-Z0-9_\-]+\.eyJ[a-zA-Z0-9_\-]+\.[a-zA-Z0-9_\-]+",
            "Connection Str": r"(?i)(mongodb|mysql|postgres|redis):\/\/[^\s\"']+",
        }
        for label, pattern in patterns.items():
            matches = re.findall(pattern, text)
            for m in matches:
                val = m if isinstance(m, str) else m[0]
                findings.append(f"[{label}] {val[:100]} (src: {url[:60]})")
        return findings

    # ═══════════════════════════════════════════════════════
    #  Scope / URL Helpers
    # ═══════════════════════════════════════════════════════

    def _is_in_scope(self, url: str) -> bool:
        """
        URL có nằm trong scope crawl không?
        Luôn enforce domain từ target — không để scope_pattern bỏ trống
        dẫn đến crawl ra ngoài.
        """
        parsed = urlparse(url)

        if parsed.scheme not in ("http", "https"):
            return False

        netloc = parsed.netloc.lower().split(":")[0]  # bỏ port
        target_host = urlparse(self._target).netloc.lower().split(":")[0]

        # Exact match hoặc subdomain hợp lệ (api.example.com, app.example.com)
        # Chặn: notexample.com, evil-example.com
        if netloc != target_host and not netloc.endswith("." + target_host):
            return False

        # scope_pattern bổ sung (e.g. chỉ crawl /api/...) nếu được config
        if self._scope and not re.search(self._scope, url, re.IGNORECASE):
            return False

        url_lower = url.lower()
        for pattern in self._exclude:
            if pattern.lower() in url_lower:
                return False

        return True

    def _is_navigable(self, url: str) -> bool:
        """
        URL có phải trang web thật không?
        Bỏ qua static files (.js, .css, .png, ...) và các pattern vô nghĩa.
        """
        parsed = urlparse(url)
        path = parsed.path

        if _SKIP_EXTENSIONS.search(path):
            return False

        if _SKIP_PATTERNS.search(url):
            return False

        if '/redirect?' in url.lower():
            return False

        return True

    def _normalize_url(self, url: str) -> str:
        """
        Normalize URL cho dedup.
        GIỮ fragment (#) cho SPA hash routing (/#/login != /#/register).
        Bỏ trailing slash và standardize.
        """
        parsed = urlparse(url)
        normalized = f"{parsed.scheme}://{parsed.netloc}{parsed.path.rstrip('/')}"
        if parsed.query:
            normalized += f"?{parsed.query}"
        if parsed.fragment:
            normalized += f"#{parsed.fragment.rstrip('/')}"
        return normalized

    # ═══════════════════════════════════════════════════════
    #  Logging
    # ═══════════════════════════════════════════════════════

    def _log_crawl(self, node: CrawlNode, status: str):
        """Ghi log crawl activity"""
        entry = {
            "timestamp": time.time(),
            "url": node.url,
            "depth": node.depth,
            "parent": node.parent_url,
            "status": status,
            "forms": node.found_forms,
            "ai_interacted": node.ai_interacted,
            "links": node.links_extracted,
        }
        self._crawl_log.append(entry)

        log_file = os.path.join(
            self._output_dir,
            self._config.get("output", {}).get("crawl_log", "crawl.jsonl"),
        )
        with open(log_file, "a") as f:
            f.write(json.dumps(entry, default=str) + "\n")

    def _log_sensitive(self, url: str, items: list):
        """Lưu sensitive data findings"""
        for item in items:
            entry = {"url": url, "finding": item, "timestamp": time.time()}
            self._sensitive_findings.append(entry)

        sensitive_file = os.path.join(self._output_dir, "sensitive_data.jsonl")
        with open(sensitive_file, "a") as f:
            for item in items:
                f.write(
                    json.dumps(
                        {"url": url, "finding": item, "timestamp": time.time()},
                        default=str,
                    )
                    + "\n"
                )

    # ═══════════════════════════════════════════════════════
    #  Nuclei
    # ═══════════════════════════════════════════════════════

    async def _run_nuclei_scan(self):
        """Export captured URLs → chạy Nuclei"""
        urls_file = self._traffic.export_urls_file()
        url_count = len(self._traffic.get_unique_urls())

        if url_count == 0:
            return

        print(f"\n[NUCLEI] 🔍 Scanning {url_count} unique URLs...")
        results = await self._nuclei.scan_urls(urls_file)

        if results:
            print(f"[NUCLEI] ⚠ Found {len(results)} issues:")
            for r in results:
                sev = r.severity.upper()
                print(f"  [{sev}] {r.template_id} → {r.url[:80]}")
        else:
            print(f"[NUCLEI] ✓ No issues found")

    # ═══════════════════════════════════════════════════════
    #  Report
    # ═══════════════════════════════════════════════════════

    def _generate_report(self, pages_crawled: int, elapsed: float):
        """Generate final JSON report"""
        report = {
            "target": self._target,
            "timestamp": time.time(),
            "duration_seconds": round(elapsed, 1),
            "summary": {
                "pages_crawled": pages_crawled,
                "total_requests": self._traffic.total_requests,
                "websocket_frames": self._traffic.total_ws_frames,
                "unique_endpoints": len(self._traffic.get_unique_urls()),
                "parameterized_endpoints": len(
                    self._traffic.get_endpoints_with_params()
                ),
                "vulnerabilities_found": len(self._nuclei.all_results),
                "sensitive_findings": len(self._sensitive_findings),
            },
            "vulnerabilities": [
                {
                    "template": r.template_id,
                    "severity": r.severity,
                    "url": r.url,
                    "description": r.description,
                }
                for r in self._nuclei.all_results
            ],
            "sensitive_findings": self._sensitive_findings,
            "crawl_log": self._crawl_log,
        }

        report_file = os.path.join(
            self._output_dir,
            self._config.get("output", {}).get("report", "report.json"),
        )
        with open(report_file, "w") as f:
            json.dump(report, f, indent=2, default=str)

        # Export supplementary data
        self._traffic.export_raw_requests()
        self._traffic.export_parameterized()
        self._nuclei.export_results()

        print(f"\n[REPORT] ═══════════════════════════════════")
        print(f"[REPORT] Target:          {self._target}")
        print(f"[REPORT] Pages crawled:   {pages_crawled}")
        print(f"[REPORT] Requests:        {self._traffic.total_requests}")
        print(f"[REPORT] WS frames:       {self._traffic.total_ws_frames}")
        print(f"[REPORT] Vulnerabilities: {len(self._nuclei.all_results)}")
        print(f"[REPORT] Sensitive finds: {len(self._sensitive_findings)}")
        print(f"[REPORT] Duration:        {elapsed:.1f}s")
        print(f"[REPORT] Report:          {report_file}")
        print(f"[REPORT] ═══════════════════════════════════")