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
import time
from dataclasses import dataclass
from urllib.parse import urlparse

import re

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

        # ─── Load session cookies (login handled externally) ──
        cookie_file = self._config.get("session_cookies")
        if cookie_file and os.path.exists(cookie_file):
            await self._browser.load_session_cookies(cookie_file)

        # ─── Phase 2: Connect AI agent via MCP ────────────
        self._agent = AIAgent(self._config, self._browser.cdp_endpoint)
        # Pass browser_infra reference → AI can call upload_file() directly
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

            # Check: redirect ra ngoài scope? (ví dụ /redirect?to=https://external.com)
            if not self._is_in_scope(actual_url):
                print(f"[DFS]   ✗ Redirected out of scope → {actual_url[:80]}")
                self._log_crawl(node, "out_of_scope_redirect")
                continue

            title = await self._browser.get_page_title()
            print(f"[DFS]   ✓ {title[:60]}")

            # ─── Check: cần AI không? ──────────────────────
            needs_ai = False
            purpose = "explore"
            has_upload = False

            if self._ai_trigger.get("on_forms"):
                # Regex nhanh — có input/textarea/button/file upload?
                elements = await self._browser.has_interactive_elements()
                if elements["input"] or elements["textarea"] or elements["button"]:
                    needs_ai = True
                    purpose = "form_fill"
                    node.found_forms = True
                    found = [k for k, v in elements.items() if v]
                    print(f"[DFS]   📋 Interactive: {', '.join(found)} → AI sẽ xác nhận")
                has_upload = elements["file_upload"]

            # ─── 🤖 MCP: AI interaction (chỉ khi cần) ─────
            if needs_ai:
                print(f"[DFS]   🤖 AI snapshot + analyze...")
                asset_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "asset")
                ai_result = await self._agent.analyze_and_interact(
                    {
                        "url": actual_url,
                        "purpose": purpose,
                        "has_upload": has_upload,
                        "asset_dir": asset_dir,
                    }
                )
                node.ai_interacted = True
                actions = ai_result.get("actions_taken", 0)
                if actions > 0:
                    print(f"[DFS]   🤖 AI completed: {actions} actions")
                    for r in ai_result.get("results", []):
                        if r.get("executed"):
                            detail = (r.get("value") or r.get("element") or r.get("snapshot", "")[:60] or "")
                            print(f"[DFS]       → {r['executed']}: ref={r.get('ref', '')} {detail}")
                else:
                    print(f"[DFS]   🤖 AI: nothing actionable (false positive)")

                # Page có thể đã thay đổi sau AI interaction
                await asyncio.sleep(1)

            # ─── Extract links → expand DFS ────────────────
            links = await self._browser.extract_links(actual_url)
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
                        parent_url=actual_url,
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
    #  Helpers
    # ═══════════════════════════════════════════════════════

    async def _handle_file_upload(self):
        """
        Upload file test vào input[type=file].
        Chọn file dựa trên accept attribute nếu có, không thì mặc định .txt.
        """
        asset_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "asset")
        if not os.path.isdir(asset_dir):
            print(f"[DFS]   📎 No asset dir → skip upload")
            return

        # Map extension → file test tương ứng
        asset_files = {}
        for f in os.listdir(asset_dir):
            ext = os.path.splitext(f)[1].lower()
            asset_files[ext] = os.path.join(asset_dir, f)

        # Tìm tất cả input[type=file] trên trang
        upload_info = await self._browser.page.evaluate("""
            () => Array.from(document.querySelectorAll('input[type=file]')).map((el, i) => ({
                index: i,
                accept: el.getAttribute('accept') || '',
                id: el.id || '',
                name: el.name || '',
            }))
        """)

        for info in upload_info:
            accept = info.get("accept", "")
            # Chọn file phù hợp với accept attribute
            chosen = asset_files.get(".txt")  # default
            if accept:
                for ext in asset_files:
                    if ext in accept or accept == "*/*":
                        chosen = asset_files[ext]
                        break
                # Nếu accept chứa image → chọn .jpg
                if "image" in accept and ".jpg" in asset_files:
                    chosen = asset_files[".jpg"]
                elif "pdf" in accept and ".pdf" in asset_files:
                    chosen = asset_files[".pdf"]

            if chosen:
                selector = f'input[type=file]:nth-of-type({info["index"] + 1})'
                if info.get("id"):
                    selector = f'input#{ info["id"]}'
                elif info.get("name"):
                    selector = f'input[name="{info["name"]}"]'
                await self._browser.upload_file(selector, chosen)
                print(f"[DFS]   📎 Uploaded {os.path.basename(chosen)} → {selector}")

    def _is_in_scope(self, url: str) -> bool:
        """URL nằm trong scope crawl?"""
        parsed = urlparse(url)

        if parsed.scheme not in ("http", "https"):
            return False

        if self._scope and self._scope not in parsed.netloc:
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

        # Skip static file extensions
        if _SKIP_EXTENSIONS.search(path):
            return False

        # Skip socket.io, webpack, etc.
        if _SKIP_PATTERNS.search(url):
            return False

        # Skip redirect URLs (tránh bị đưa ra ngoài scope)
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
        # GIỮ fragment cho SPA hash-based routing
        if parsed.fragment:
            normalized += f"#{parsed.fragment.rstrip('/')}"
        return normalized

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
        print(f"[REPORT] Duration:        {elapsed:.1f}s")
        print(f"[REPORT] Report:          {report_file}")
        print(f"[REPORT] ═══════════════════════════════════")
