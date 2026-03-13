"""
🎭 PLAYWRIGHT PYTHON — Infrastructure Layer
=============================================
"Phần máy móc" — chạy tự động, không cần AI nhìn vào

Sở hữu toàn bộ browser instance. Chạy NỀN liên tục.

Responsibilities:
  ┌────────────────────────────────┬──────────────────────────────────────────┐
  │ API                            │ Lý do dùng Playwright thay vì MCP        │
  ├────────────────────────────────┼──────────────────────────────────────────┤
  │ page.route()                   │ Bắt full headers + body (MCP không có)  │
  │ CDP session                    │ Bắt WebSocket frames (MCP không hỗ trợ) │
  │ context.storage_state()        │ Snapshot để backtrack (logic thuần)      │
  │ context record HAR             │ Export toàn bộ traffic session           │
  │ page.goto() + wait_for_load    │ Điều hướng do DFS engine kiểm soát      │
  │ add_init_script()              │ Inject JS patch fetch/XHR vào mọi page  │
  └────────────────────────────────┴──────────────────────────────────────────┘

CDP Sharing:
  Browser được launch với --remote-debugging-port.
  → Playwright Python kết nối qua internal pipe (full API)
  → Playwright-MCP kết nối qua CDP endpoint (MCP tools)
  → Cả hai thao tác trên CÙNG browser instance.
"""

import asyncio
import json
import os
import re
from urllib.parse import urlparse

from playwright.async_api import (
    async_playwright,
    Browser,
    BrowserContext,
    Page,
    Playwright,
    Route,
)


class BrowserInfra:
    """
    Playwright Python infrastructure — the 'machine' layer.

    Owns the browser. Captures ALL traffic via page.route().
    Exposes CDP endpoint so Playwright-MCP can connect to same browser.
    """

    def __init__(self, config: dict):
        self._config = config
        self._pw: Playwright = None
        self._browser: Browser = None
        self._context: BrowserContext = None
        self._page: Page = None
        self._cdp_session = None
        self._traffic_store = None
        self._cdp_port = config.get("browser", {}).get("cdp_port", 9222)

    # ─── Properties ────────────────────────────────────────

    @property
    def cdp_endpoint(self) -> str:
        """CDP endpoint cho Playwright-MCP kết nối"""
        return f"http://localhost:{self._cdp_port}"

    @property
    def page(self) -> Page:
        return self._page

    @property
    def context(self) -> BrowserContext:
        return self._context

    # ─── Launch ────────────────────────────────────────────

    async def launch(self, traffic_store):
        """
        Khởi động browser với đầy đủ hạ tầng:
          1. Remote debugging port → cho MCP connect
          2. Proxy nếu cần (mitmproxy)
          3. HAR recording
          4. Init script injection
          5. Traffic capture routes
          6. CDP session cho WebSocket
        """
        self._traffic_store = traffic_store
        self._pw = await async_playwright().start()

        browser_cfg = self._config.get("browser", {})

        # Launch args — expose CDP port for Playwright-MCP
        launch_args = [
            f"--remote-debugging-port={self._cdp_port}",
            "--disable-blink-features=AutomationControlled",
            "--no-first-run",
        ]

        proxy = browser_cfg.get("proxy")

        # Hỗ trợ cả hai kiểu config: headless: true/false HOẶC headed: true/false
        if "headed" in browser_cfg:
            headless = not browser_cfg["headed"]
        else:
            headless = browser_cfg.get("headless", True)

        self._browser = await self._pw.chromium.launch(
            headless=headless,
            args=launch_args,
            proxy={"server": proxy} if proxy else None,
        )

        # Context — isolation + interception config
        ctx_opts = {
            "ignore_https_errors": True,
            "service_workers": "block",  # đảm bảo bắt được hết traffic
        }
        ua = browser_cfg.get("user_agent")
        if ua:
            ctx_opts["user_agent"] = ua

        har_path = browser_cfg.get("har_path")
        if har_path:
            os.makedirs(os.path.dirname(har_path) or ".", exist_ok=True)
            ctx_opts["record_har_path"] = har_path
            ctx_opts["record_har_url_filter"] = "**/*"

        self._context = await self._browser.new_context(**ctx_opts)

        # Init script injection — patch fetch/XHR trong mọi page
        init_script = browser_cfg.get("init_script")
        if init_script and os.path.exists(init_script):
            await self._context.add_init_script(path=init_script)

        await self._context.add_init_script(script="window.__DFS_CRAWL__ = true;")

        self._page = await self._context.new_page()

        # Bắt traffic + WebSocket
        await self._setup_route_capture()
        await self._setup_cdp_websocket()

    # ─── Traffic Capture (page.route) ──────────────────────

    async def _setup_route_capture(self):
        """
        page.route("**/*") — intercept MỌI network request.

        Tại sao dùng page.route() thay vì page.on("response")?
          → route.fetch() cho phép đọc response body CHẮC CHẮN
          → MCP browser_network_requests KHÔNG trả về body
          → Đây là lý do PHẢI dùng Playwright Python cho traffic

        Chiến lược:
          - document/xhr/fetch: full capture (headers + body) qua route.fetch()
          - static resources:   metadata only, pass through nhanh
        """
        # Proxy đã theo dõi traffic → chỉ cần pass-through, không ghi file
        # Vẫn giữ route để đếm requests cho reporting
        important_types = {"document", "xhr", "fetch", "websocket", "other"}

        async def capture_handler(route: Route):
            request = route.request
            rtype = request.resource_type

            req_data = {
                "url": request.url,
                "method": request.method,
                "resource_type": rtype,
            }

            # Chỉ lưu metadata in-memory, KHÔNG ghi file (proxy đã lo)
            self._traffic_store.store_http_meta(req_data)
            await route.continue_()

        await self._page.route("**/*", capture_handler)

    # ─── CDP WebSocket Capture ─────────────────────────────

    async def _setup_cdp_websocket(self):
        """
        CDP session — bắt WebSocket frames.
        Playwright-MCP KHÔNG hỗ trợ capture WebSocket content.
        → Phải dùng CDP trực tiếp.
        """
        self._cdp_session = await self._context.new_cdp_session(self._page)
        await self._cdp_session.send("Network.enable")

        self._cdp_session.on(
            "Network.webSocketFrameSent",
            lambda params: self._traffic_store.store_websocket("sent", params),
        )
        self._cdp_session.on(
            "Network.webSocketFrameReceived",
            lambda params: self._traffic_store.store_websocket("received", params),
        )

    # ─── Session Cookies (login handled externally) ────────

    async def load_session_cookies(self, cookie_file: str):
        """
        Load session cookies từ file JSON (do hệ thống login bên ngoài cung cấp).
        Inject cookies vào browser context + localStorage.
        Hỗ trợ format: {"cookies": {"name": "value"}, "base_url": "..."}
        """
        with open(cookie_file) as f:
            session = json.load(f)

        cookies_data = session.get("cookies", {})
        base_url = session.get("base_url", "")
        parsed = urlparse(base_url) if base_url else urlparse(self._page.url)
        domain = parsed.netloc or parsed.hostname or ""

        # Thêm cookies vào browser context
        pw_cookies = []
        for name, value in cookies_data.items():
            pw_cookies.append({
                "name": name,
                "value": value,
                "domain": domain,
                "path": "/",
            })

        if pw_cookies:
            await self._context.add_cookies(pw_cookies)

        # Navigate đến target trước → thoát about:blank → mới có quyền localStorage
        target_url = base_url or self._page.url
        if not target_url or "about:blank" in target_url:
            target_url = f"https://{domain}"
        await self._page.goto(target_url, wait_until="domcontentloaded", timeout=30000)
        await asyncio.sleep(1)

        # Inject vào localStorage (nhiều SPA đọc token từ đây)
        for name, value in cookies_data.items():
            safe_value = value.replace("'", "\\'")
            await self._page.evaluate(
                f"localStorage.setItem('{name}', '{safe_value}')"
            )

        print(f"[BROWSER] 🍪 Loaded {len(pw_cookies)} cookies from {cookie_file}")
        print(f"[BROWSER]    → domain: {domain}")

    # ─── Navigation (controlled by DFS engine) ─────────────

    async def navigate(self, url: str) -> bool:
        """
        Điều hướng browser.
        Hỗ trợ cả navigation thường và SPA hash routing.
        """
        current = self._page.url
        parsed_new = urlparse(url)
        parsed_cur = urlparse(current)

        same_origin = (
            parsed_new.scheme == parsed_cur.scheme
            and parsed_new.netloc == parsed_cur.netloc
            and parsed_new.path == parsed_cur.path
        )

        if same_origin and parsed_new.fragment:
            # SPA hash navigation: chỉ thay đổi hash, không reload page
            await self._page.evaluate(f"window.location.hash = '{parsed_new.fragment}'")
            await asyncio.sleep(2)  # Chờ Angular/React render component mới
            return True
        else:
            # Full navigation
            resp = await self._page.goto(
                url, wait_until="domcontentloaded", timeout=30000
            )
            await asyncio.sleep(2)
            return resp is not None and resp.status < 400

    async def current_url(self) -> str:
        return self._page.url

    async def get_page_title(self) -> str:
        return await self._page.title()

    async def get_html(self) -> str:
        return await self._page.content()

    # ─── State Management (backtracking) ───────────────────

    async def save_state(self) -> dict:
        """
        context.storage_state() — snapshot cookies + localStorage.
        Dùng cho DFS backtracking: lưu state trước khi đi sâu,
        restore nếu cần quay lại.
        """
        return await self._context.storage_state()

    async def restore_state(self, state: dict):
        """Khôi phục browser state từ snapshot"""
        await self._context.clear_cookies()
        if state.get("cookies"):
            await self._context.add_cookies(state["cookies"])

    # ─── Page Analysis (JS evaluation) ─────────────────────

    async def extract_links(self, base_url: str) -> list[str]:
        """
        Extract tất cả link navigable từ page hiện tại.

        Hỗ trợ SPA (Angular/React/Vue):
          - <a href> thông thường
          - <a routerLink> (Angular)
          - <a [href]> (Angular binding)
          - <button routerLink>
          - Hash-based routing (#/path)
          - onclick navigation
        """
        raw_links = await self._page.evaluate(
            """(baseUrl) => {
            const links = new Set();
            const origin = new URL(baseUrl).origin;

            // <a href="...">
            document.querySelectorAll('a[href]').forEach(a => {
                const href = a.href;
                if (href && !href.startsWith('javascript:') &&
                    !href.startsWith('mailto:') && !href.startsWith('#') &&
                    !href.startsWith('tel:') && !href.startsWith('data:')) {
                    links.add(href);
                }
            });

            // SPA routerLink (Angular) — <a routerLink="/login">
            document.querySelectorAll('[routerLink], [routerlink]').forEach(el => {
                const rl = el.getAttribute('routerLink') || el.getAttribute('routerlink') || '';
                if (rl && rl !== '/') {
                    // Hash-based SPA: origin + /#/ + path
                    if (window.location.hash !== undefined && window.location.href.includes('#/')) {
                        links.add(origin + '/#' + (rl.startsWith('/') ? rl : '/' + rl));
                    } else {
                        links.add(new URL(rl, baseUrl).href);
                    }
                }
            });

            // Clickable elements with navigation data attributes
            document.querySelectorAll('[data-href], [data-url], [data-link]').forEach(el => {
                const val = el.getAttribute('data-href') || el.getAttribute('data-url') || el.getAttribute('data-link') || '';
                if (val) {
                    try { links.add(new URL(val, baseUrl).href); } catch(e) {}
                }
            });

            // onclick navigation patterns
            document.querySelectorAll('[onclick]').forEach(el => {
                const onclick = el.getAttribute('onclick') || '';
                const match = onclick.match(
                    /(?:location\\.href|window\\.open|location\\.assign|router\\.navigate)\\s*[=(\\[]\\s*['"]([^'"]+)['"]/
                );
                if (match) {
                    try { links.add(new URL(match[1], baseUrl).href); }
                    catch(e) {}
                }
            });

            // <form action="..."> (chỉ nếu action khác page hiện tại)
            document.querySelectorAll('form[action]').forEach(f => {
                const action = f.getAttribute('action');
                if (action && action !== '' && action !== '#') {
                    try { links.add(new URL(action, baseUrl).href); } catch(e) {}
                }
            });

            return [...links];
        }""",
            base_url,
        )
        return raw_links

    async def has_interactive_elements(self) -> dict:
        """
        Detect nhanh: trang có gì để tương tác?
        Keyword đơn giản — nhìn phát biết.
        Trả dict cho biết loại nào có, AI snapshot sẽ xác nhận chi tiết.
        """
        html = await self._page.content()

        return {
            # Có chỗ điền (input text, textarea, contenteditable)
            "input": bool(re.search(
                r'<input\b(?![^>]*type\s*=\s*["\']?'
                r'(?:hidden|submit|button|image|reset))',
                html, re.IGNORECASE,
            )),
            "textarea": bool(re.search(
                r'<textarea|contenteditable', html, re.IGNORECASE,
            )),
            # Có nút bấm (button, submit, role=button, clickable)
            "button": bool(re.search(
                r'<button|type\s*=\s*["\']?submit|role\s*=\s*["\']?button',
                html, re.IGNORECASE,
            )),
            # Có file upload
            "file_upload": bool(re.search(
                r'type\s*=\s*["\']?file', html, re.IGNORECASE,
            )),
        }

    async def upload_file(self, selector: str, file_path: str):
        """
        Upload file vào input[type=file].
        Dùng Playwright set_input_files — hoạt động với mọi framework.
        """
        locator = self._page.locator(selector)
        await locator.set_input_files(file_path)
        print(f"[BROWSER] 📎 Uploaded: {os.path.basename(file_path)}")

    # ─── Cleanup ───────────────────────────────────────────

    async def close(self):
        """Tắt browser và tất cả connections"""
        if self._cdp_session:
            await self._cdp_session.detach()
        if self._context:
            await self._context.close()
        if self._browser:
            await self._browser.close()
        if self._pw:
            await self._pw.stop()
