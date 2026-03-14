"""
🤖 PLAYWRIGHT-MCP + LLM — AI Eyes Layer
=========================================
"Đôi mắt của AI" — chỉ gọi khi cần nhìn UI và tương tác

Module này CHỈ được kích hoạt khi DFS engine gặp page cần tương tác
thông minh (forms, login, navigation phức tạp).

Responsibilities:
  ┌──────────────────────────┬───────────────────────────────────────────┐
  │ MCP Tool                  │ Lý do dùng MCP thay vì Playwright trực  │
  ├──────────────────────────┼───────────────────────────────────────────┤
  │ browser_snapshot()        │ AI cần "nhìn" cấu trúc trang để quyết  │
  │ browser_fill_form()       │ AI biết điền gì từ snapshot context      │
  │ browser_click()           │ AI chỉ đúng element cần click            │
  │ browser_wait_for()        │ Đợi server response sau khi submit       │
  │ browser_handle_dialog()   │ AI quyết định accept/dismiss popup       │
  └──────────────────────────┴───────────────────────────────────────────┘

Architecture:
  1. MCP server connects to CÙNG browser via CDP endpoint
  2. LLM nhận page snapshot → quyết định actions
  3. Actions thực thi qua MCP tools
  4. Playwright Python (chạy nền) tự động bắt mọi traffic phát sinh
"""

import asyncio
import json
import os
from contextlib import AsyncExitStack

from mcp import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client


class AIAgent:
    """
    Playwright-MCP agent — đôi mắt và bàn tay của AI.

    Kết nối đến CÙNG browser mà BrowserInfra sở hữu,
    qua CDP endpoint. Dùng MCP tools cho UI interaction,
    LLM API cho decision making.

    Playwright Python vẫn chạy nền bắt traffic trong lúc
    AI agent thao tác → không bỏ sót request nào.
    """

    def __init__(self, config: dict, cdp_endpoint: str):
        self._config = config
        self._cdp_endpoint = cdp_endpoint
        self._session: ClientSession = None
        self._exit_stack = AsyncExitStack()
        self._llm_client = None
        self._llm_model = "gpt-4o"
        self._llm_max_tokens = 4096
        self._llm_temperature = 0.1
        self._connected = False

    # ─── Connection Lifecycle ──────────────────────────────

    async def connect(self):
        """
        Khởi động Playwright-MCP server + kết nối qua stdio MCP protocol.

        MCP server connect đến CÙNG browser qua --cdp-endpoint.
        → browser_snapshot() nhìn CÙNG page mà Playwright Python đang capture.
        → browser_click() trigger navigation → page.route() tự động bắt traffic.
        """
        server_params = StdioServerParameters(
            command="npx",
            args=[
                "@playwright/mcp@latest",
                "--cdp-endpoint",
                self._cdp_endpoint,
                "--headless",
                "--block-service-workers",
                "--ignore-https-errors",
            ],
        )

        transport = await self._exit_stack.enter_async_context(
            stdio_client(server_params)
        )
        read_stream, write_stream = transport
        self._session = await self._exit_stack.enter_async_context(
            ClientSession(read_stream, write_stream)
        )
        await self._session.initialize()
        self._connected = True

        # Khởi tạo LLM client
        self._init_llm()

    def _init_llm(self):
        """
        Setup LLM client — dùng GitHub Models API.

        GitHub Models là API tương thích OpenAI, dùng GitHub PAT (Personal Access Token)
        thay vì OpenAI API key. Hỗ trợ GPT-4o, GPT-4o-mini, Claude, và nhiều model khác.

        Endpoint: https://models.inference.ai.azure.com
        Auth: GITHUB_TOKEN environment variable (Personal Access Token)
        """
        import openai

        llm_cfg = self._config.get("llm", {})
        token_env = llm_cfg.get("api_key_env", "GITHUB_TOKEN")
        api_key = os.environ.get(token_env)

        base_url = llm_cfg.get("base_url", "https://models.inference.ai.azure.com")

        self._llm_client = openai.AsyncOpenAI(
            api_key=api_key,
            base_url=base_url,
        )
        self._llm_model = llm_cfg.get("model", "gpt-4o")
        self._llm_max_tokens = llm_cfg.get("max_tokens", 4096)
        self._llm_temperature = llm_cfg.get("temperature", 0.1)

    # ═══════════════════════════════════════════════════════
    #  MCP Tool Wrappers
    #  Mỗi method map 1:1 với một Playwright-MCP tool
    # ═══════════════════════════════════════════════════════

    async def snapshot(self) -> str:
        """
        browser_snapshot() — AI đọc cấu trúc trang.

        Trả về accessibility tree dạng text.
        AI dùng để: nhận diện form fields, buttons, links, errors.
        """
        result = await self._session.call_tool("browser_snapshot", arguments={})
        return result.content[0].text if result.content else ""

    async def fill_form(self, fields: list[dict]):
        """
        browser_fill_form() — AI điền nhiều form fields cùng lúc.

        fields: [
            {"ref": "R12", "element": "email input", "value": "test@test.com"},
            {"ref": "R15", "element": "password input", "value": "Pass123!"},
        ]
        """
        return await self._session.call_tool(
            "browser_fill_form", arguments={"fields": fields}
        )

    async def click(self, ref: str, element: str = ""):
        """
        browser_click() — AI click element theo ref từ snapshot.

        AI đọc snapshot → biết ref nào là submit button,
        ref nào là navigation link → click chính xác.
        """
        args = {"ref": ref}
        if element:
            args["element"] = element
        return await self._session.call_tool("browser_click", arguments=args)

    async def type_text(self, ref: str, text: str, submit: bool = False):
        """
        browser_type() — AI nhập text vào element.

        submit=True → press Enter sau khi nhập (cho search forms).
        """
        return await self._session.call_tool(
            "browser_type",
            arguments={"ref": ref, "text": text, "submit": submit},
        )

    async def wait_for(self, text: str = None, time_seconds: float = None):
        """
        browser_wait_for() — Chờ sau khi submit.

        Đợi text xuất hiện (e.g. "Success") hoặc đợi N giây.
        """
        args = {}
        if text:
            args["text"] = text
        if time_seconds:
            args["time"] = time_seconds
        return await self._session.call_tool("browser_wait_for", arguments=args)

    async def handle_dialog(self, accept: bool = True, prompt_text: str = None):
        """
        browser_handle_dialog() — Xử lý alert/confirm/prompt popup.

        AI quyết định accept hay dismiss dựa trên context.
        """
        args = {"accept": accept}
        if prompt_text:
            args["promptText"] = prompt_text
        return await self._session.call_tool(
            "browser_handle_dialog", arguments=args
        )

    async def select_option(self, ref: str, values: list[str], element: str = ""):
        """
        browser_select_option() — Chọn option trong dropdown.
        """
        args = {"ref": ref, "values": values}
        if element:
            args["element"] = element
        return await self._session.call_tool(
            "browser_select_option", arguments=args
        )

    async def press_key(self, key: str):
        """
        browser_press_key() — Nhấn phím (Ctrl+a, Backspace, Enter, etc.)
        """
        return await self._session.call_tool(
            "browser_press_key", arguments={"key": key}
        )


    # ═══════════════════════════════════════════════════════
    #  Main AI Interaction Flow
    # ═══════════════════════════════════════════════════════

    async def _wait_for_post_request(self, page, timeout: float = 3.0) -> bool:
        import re
        caught = asyncio.Event()

        SKIP_PATTERNS = re.compile(
            r"ping|heartbeat|keepalive|keep-alive|beacon|alive|health"
            r"|analytics|track|telemetry|metric|stat|log|event|socket",
            re.IGNORECASE,
        )

        def _on_request(request):
            if request.method != "POST":
                return
            if SKIP_PATTERNS.search(request.url):
                return
            caught.set()

        page.on("request", _on_request)
        try:
            await asyncio.wait_for(caught.wait(), timeout=timeout)
            return True
        except asyncio.TimeoutError:
            return False
        finally:
            page.remove_listener("request", _on_request)
    
    async def analyze_and_interact(self, page_context: dict) -> dict:
        """
        1. Snapshot → 2. LLM analyze + plan → 3. Execute fill only → 4. Upload if needed
        → 5. Re-snapshot → 6. LLM tìm submit trên fresh snapshot → 7. Click submit
        """
        # Step 1: Snapshot
        snapshot_text = await self.snapshot()
        print(f"[AI]     Snapshot: {len(snapshot_text)} chars")
        if not snapshot_text or len(snapshot_text) < 50:
            print(f"[AI]     ✗ Empty snapshot")
            return {"actions_taken": 0, "results": [], "sensitive_data": []}

        # Step 2: LLM analyze + plan actions
        llm_response = await self._ask_llm_full(snapshot_text, page_context)
        sensitive_data = llm_response.get("sensitive_data", [])
        actions = llm_response.get("actions", [])

        print(f"[AI]     LLM: {len(sensitive_data)} sensitive items, {len(actions)} actions planned")
        for i, a in enumerate(actions):
            desc = (a.get('value') or a.get('element') or '')[:50]
            print(f"[AI]       [{i}] {a.get('type','')} ref={a.get('ref','')} {desc}")

        if not actions:
            return {"actions_taken": 0, "results": [], "sensitive_data": sensitive_data}

        # Dedup clicks tương tự
        actions = self._dedup_similar_actions(actions, max_similar=1)

        # Step 3: Thực thi CHỈ fill/type/select/click (không submit)
        results = []
        had_fill = False

        for action in actions:
            atype = action.get("type", "")

            if atype == "upload":
                continue

            if self._is_submit_action(action):
                print(f"[AI]     📋 Skipping submit from old plan, will re-find after fill")
                continue

            result = await self._execute_action(action)
            results.append(result)

            if atype in ("fill", "type", "select"):
                had_fill = True

        # Step 4: Upload file nếu cần
        if page_context.get("has_upload") and not any(
            r.get("executed") == "upload" for r in results
        ):
            asset_dir = page_context.get("asset_dir", "")
            if asset_dir:
                r = await self._do_file_upload(asset_dir)
                results.append(r)

        # Step 5: RE-SNAPSHOT sau khi fill xong → thử submit
        if had_fill:
            await asyncio.sleep(1)

            # Lấy page để theo dõi network
            _page = None
            _browser_infra = self._config.get("_browser_infra")
            if _browser_infra and hasattr(_browser_infra, "page"):
                _page = _browser_infra.page

            # Thử nhấn Enter — bắt POST request trong 2.5s
            print(f"[AI]     ⏎ Trying Enter to submit...")
            post_detected = False
            if _page:
                # Bắt đầu lắng nghe TRƯỚC khi nhấn
                _post_task = asyncio.ensure_future(
                    self._wait_for_post_request(_page, timeout=2.5)
                )
                await self.press_key("Enter")
                post_detected = await _post_task
            else:
                await self.press_key("Enter")
                await asyncio.sleep(2)

            if post_detected:
                print(f"[AI]     ✓ Enter submitted successfully (POST request detected)")
                results.append({"executed": "submit", "ref": "Enter", "element": "Enter key"})

                snapshot_after = await self.snapshot()
                if self._detect_issue(snapshot_after):
                    print(f"[AI]     ⚠ Submit issue → retrying...")
                    fix = await self._smart_retry(snapshot_after, page_context)
                    results.extend(fix)
            else:
                # Enter không gửi POST → gọi LLM tìm submit button
                print(f"[AI]     ✗ Enter didn't trigger POST, asking LLM for submit button...")
                fresh_snapshot = await self.snapshot()
                submit_ref = await self._ask_llm_for_submit(fresh_snapshot, page_context)

                if submit_ref:
                    ref = submit_ref.get("ref", "")
                    el  = submit_ref.get("element", "")
                    print(f"[AI]     ➡ Submit (LLM found): ref={ref} {el[:50]}")

                    # Lắng nghe POST TRƯỚC khi click
                    post_detected = False
                    if _page:
                        _post_task = asyncio.ensure_future(
                            self._wait_for_post_request(_page, timeout=3.0)
                        )
                        await self.click(ref, el)
                        post_detected = await _post_task
                    else:
                        await self.click(ref, el)
                        await asyncio.sleep(2)
                        post_detected = True  # fallback: assume ok

                    if post_detected:
                        print(f"[AI]     ✓ SUBMIT clicked (POST request detected)")
                    else:
                        print(f"[AI]     ⚠ SUBMIT clicked but no POST detected")

                    results.append({"executed": "submit", "ref": ref, "element": el,
                                    "post_confirmed": post_detected})

                    post_snap = await self.snapshot()
                    if self._detect_issue(post_snap):
                        print(f"[AI]     ⚠ Submit issue → retrying...")
                        fix = await self._smart_retry(post_snap, page_context)
                        results.extend(fix)
                else:
                    print(f"[AI]     ✗ LLM cannot find submit button")

        return {"actions_taken": len(actions), "results": results, "sensitive_data": sensitive_data}


    async def _ask_llm_full(self, snapshot: str, context: dict) -> dict:
        """
        Gửi page snapshot cho LLM, nhận về sensitive_data + actions.
        """
        system_prompt = self._build_system_prompt()
        user_prompt = self._build_user_prompt(snapshot, context)

        response = await self._llm_client.chat.completions.create(
            model=self._llm_model,
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            max_tokens=self._llm_max_tokens,
            temperature=self._llm_temperature,
            response_format={"type": "json_object"},
        )

        response_text = response.choices[0].message.content
        parsed = json.loads(response_text)

        sensitive = parsed.get("sensitive_data", [])
        if isinstance(sensitive, str):
            sensitive = [sensitive] if sensitive else []

        actions = parsed.get("actions", [])
        if isinstance(parsed, list):
            actions = parsed
            sensitive = []

        actions = [a for a in actions if isinstance(a, dict)]
        return {"sensitive_data": sensitive, "actions": actions}

    def _is_submit_action(self, action: dict) -> bool:
        """Nhận diện action là submit/send button — dùng regex, không phân biệt hoa thường."""
        import re
        atype = action.get("type", "")
        if atype in ("submit", "click_submit"):
            return True
        if atype != "click":
            return False
        el = action.get("element") or ""
        submit_kw = [
            r"submit", r"send", r"save", r"complain", r"checkout",
            r"place[\s_\-]?order", r"complete", r"confirm",
            r"post[\s_\-]?review", r"post", r"gửi", r"đăng",
        ]
        pattern = "|".join(submit_kw)
        return bool(re.search(pattern, el, re.IGNORECASE))

    async def _ask_llm_for_submit(self, snapshot: str, context: dict) -> dict | None:
        """
        Hỏi LLM tìm ĐÚNG nút submit trên fresh snapshot.
        LLM nhìn ngữ cảnh form → chọn đúng button trong cùng container.
        """
        prompt = f"""I just filled out a form on this page. Now I need to find the correct submit button.

Page URL: {context.get('url', '?')}

Current page snapshot (AFTER filling all fields):
{snapshot[:6000]}

RULES:
- Find the submit/send button that belongs to the SAME form or section where the input fields are.
- The correct button is usually at the bottom of the form container (form, card, div, section, span).
- Do NOT pick buttons from: navigation bars, sidebars, headers, footers, cookie banners, or other unrelated sections.
- Do NOT pick "Login", "Register", "Search", "Menu", or navigation buttons.
- If there are multiple forms, pick the button for the form that has filled-in data.

Return JSON: {{"ref": "R<number>", "element": "description of the button"}}
If no submit button found, return: {{"ref": "", "element": ""}}"""

        response = await self._llm_client.chat.completions.create(
            model=self._llm_model,
            messages=[
                {"role": "system", "content": "You find the correct submit button for a form. Return JSON only."},
                {"role": "user", "content": prompt},
            ],
            max_tokens=256,
            temperature=0.0,
            response_format={"type": "json_object"},
        )

        text = response.choices[0].message.content
        parsed = json.loads(text)
        ref = parsed.get("ref", "")
        if ref:
            return parsed
        return None


    @staticmethod
    def _dedup_similar_actions(actions: list[dict], max_similar: int = 2) -> list[dict]:
        """
        Dedup dựa trên semantic category, không overfit vào prefix text.
        fill/type/select/upload giữ nguyên.
        """
        # Map keyword → category
        _CATEGORIES = {
            "delete":  ["delete", "remove", "xóa", "dismiss", "hapus", "eliminar", "supprimer"],
            "add":     ["add", "thêm", "new", "create", "insert", "tambah", "añadir", "ajouter"],
            "like":    ["like", "upvote", "heart", "react", "thích", "suka", "me gusta", "j'aime"],
            "export":  ["export", "download", "print", "tải", "unduh", "descargar", "télécharger"],
            "submit":  ["submit", "send", "save", "confirm", "complete", "gửi", "kirim", "enviar", "envoyer"],
            "edit":    ["edit", "update", "modify", "change", "sửa", "ubah", "editar", "modifier"],
            "share":   ["share", "copy link", "chia sẻ", "bagikan", "compartir", "partager"],
            "follow":  ["follow", "subscribe", "unfollow", "theo dõi", "ikuti", "seguir", "suivre"],
            "report":  ["report", "flag", "báo", "laporkan", "reportar", "signaler"],
        }

        def _categorize(el: str) -> str:
            low = el.lower()
            for cat, kws in _CATEGORIES.items():
                if any(kw in low for kw in kws):
                    return cat
            # fallback: first meaningful word
            words = [w for w in low.split() if len(w) > 2]
            return words[0] if words else low[:10]

        seen: dict[str, int] = {}
        result = []

        for a in actions:
            if a.get("type") not in ("click", "submit", "click_submit"):
                result.append(a)
                continue

            el  = (a.get("element") or "").strip()
            key = _categorize(el)
            count = seen.get(key, 0)

            if count < max_similar:
                result.append(a)
                seen[key] = count + 1
            else:
                print(f"[AI]     ✂ Skip duplicate [{key}]: {el[:50]}")

        if len(result) != len(actions):
            print(f"[AI]     ✂ Dedup: {len(actions)} → {len(result)} actions")
        return result

    def _detect_issue(self, snapshot: str) -> bool:
        """Nhìn snapshot — có dấu hiệu button disabled hoặc validation error?"""
        lowered = snapshot.lower()
        indicators = ["disabled", "invalid", "error", "required", "please fill", "mandatory"]
        return any(word in lowered for word in indicators)

    async def _smart_retry(self, snapshot: str, context: dict, max_retries: int = 2) -> list:
        """
        Nút disabled hoặc form có error → hỏi LLM tại sao → fix → thử lại.
        """
        results = []
        for attempt in range(max_retries):
            print(f"[AI]     Retry {attempt + 1}/{max_retries}: asking LLM to diagnose...")

            prompt = f"""I just tried to submit a form but it didn't go through. Something is wrong.

Current page snapshot after my attempt:
{snapshot[:6000]}

Please look at the page carefully and figure out what happened:
- Did I miss a required field?
- Is there a validation error message visible on the page?
- Is the submit button disabled because something is still incomplete?
- Is there a challenge or verification I didn't complete correctly?

Identify what needs to be fixed and return corrected actions.
Only fix what's wrong — don't re-fill fields that already have correct values.
The LAST action should click the submit button of the SAME form I was filling.

Respond with JSON: {{"diagnosis": "brief description of the problem", "actions": [...]}}"""

            response = await self._llm_client.chat.completions.create(
                model=self._llm_model,
                messages=[
                    {"role": "system", "content": "You are a helpful user trying to complete a form submission. Diagnose why it failed and provide fix actions. Respond with JSON only."},
                    {"role": "user", "content": prompt},
                ],
                max_tokens=1024,
                temperature=0.0,
                response_format={"type": "json_object"},
            )

            text = response.choices[0].message.content
            parsed = json.loads(text)
            diagnosis = parsed.get("diagnosis", "")
            fix_actions = parsed.get("actions", [])
            fix_actions = [a for a in fix_actions if isinstance(a, dict)]

            print(f"[AI]     Diagnosis: {diagnosis[:100]}")

            if not fix_actions:
                print(f"[AI]     No fix suggested, giving up")
                break

            for action in fix_actions:
                result = await self._execute_action(action)
                results.append(result)
                desc = (action.get('value') or action.get('element') or '')[:40]
                print(f"[AI]     Fix: {result.get('executed','')} {desc}")

            await asyncio.sleep(1)

            snapshot = await self.snapshot()
            if not self._detect_issue(snapshot):
                print(f"[AI]     ✓ Issue resolved!")
                break

        return results
    # ─── Automated Helpers (no LLM needed) ─────────────────

    def _find_submit_in_snapshot(self, snapshot: str) -> list[dict]:
        """
        Tìm submit button trong MCP accessibility snapshot.
        In ra tất cả button để debug, rồi tìm theo keyword.
        """
        import re

        # Thu thập TẤT CẢ dòng có ref và button/submit
        all_buttons = []
        for line in snapshot.split('\n'):
            low = line.lower()
            if 'ref=' not in low:
                continue
            if 'button' in low or 'submit' in low:
                all_buttons.append(line.strip())

        print(f"[AI]     Buttons found in snapshot: {len(all_buttons)}")
        for b in all_buttons[:15]:
            print(f"[AI]       │ {b[:120]}")

        # Tìm ref trong mỗi dòng button
        submit_kw = ['submit', 'send', 'save', 'complain', 'checkout', 'place order',
                     'complete', 'confirm', 'post', 'review']
        skip_kw = ['dismiss', 'close', 'cancel', 'cookie', 'add to basket', 'add to cart',
                   'login', 'register', 'sign in', 'sign up', 'log in', 'me want', 'navigation',
                   'account', 'search', 'language']

        # Pass 1: tìm button có submit keyword
        for line in all_buttons:
            low = line.lower()
            if any(kw in low for kw in submit_kw):
                m = re.search(r'ref=(\w+)', line)
                if m:
                    ref = m.group(1)
                    print(f"[AI]     ✓ Found submit: ref={ref} → {line[:80]}")
                    return [{"type": "click", "ref": ref, "element": line.strip()[:80]}]

        # Pass 2: bất kỳ button không nằm trong skip list
        for line in all_buttons:
            low = line.lower()
            if any(kw in low for kw in skip_kw):
                continue
            m = re.search(r'ref=(\w+)', line)
            if m:
                ref = m.group(1)
                print(f"[AI]     ✓ Found button (generic): ref={ref} → {line[:80]}")
                return [{"type": "click", "ref": ref, "element": line.strip()[:80]}]

        print(f"[AI]     ✗ No submit button found in snapshot")
        return []

    async def _do_file_upload(self, asset_dir: str) -> dict:
        """
        Upload file: ưu tiên input[type=file]; nếu không có thì tìm button
        có class/content chứa upload-related keyword và dùng file-chooser.
        """
        import os
        import re

        browser_infra = self._config.get("_browser_infra")
        if not browser_infra or not hasattr(browser_infra, "page") or not browser_infra.page:
            print(f"[AI]     📎 ✗ No browser page available")
            return {"executed": "upload_skipped", "reason": "no browser"}

        page = browser_infra.page

        # ── Chọn file phù hợp ─────────────────────────────────────────────────
        def _pick_file(accept: str = "") -> str:
            acc = accept.lower()
            if any(x in acc for x in ["image", "jpg", "png", "gif", ".jpg", ".png"]):
                candidates = ["test.jpg", "test.png"]
            elif "pdf" in acc:
                candidates = ["test.pdf"]
            elif "xml" in acc:
                candidates = ["test.xml"]
            else:
                candidates = ["test.pdf", "test.jpg"]

            for name in candidates:
                fp = os.path.join(asset_dir, name)
                if os.path.exists(fp):
                    return fp

            # Fallback: bất kỳ file nào trong asset_dir
            for f in os.listdir(asset_dir):
                return os.path.join(asset_dir, f)
            return ""

        # ── 1. Thử input[type=file] ────────────────────────────────────────────
        upload_info = await page.evaluate("""
            () => Array.from(document.querySelectorAll('input[type=file]')).map((el, i) => ({
                index: i,
                accept: el.getAttribute('accept') || '',
                id: el.id || '',
                name: el.name || '',
            }))
        """)

        if upload_info:
            info     = upload_info[0]
            filepath = _pick_file(info.get("accept", ""))
            if not filepath:
                return {"executed": "upload_skipped", "reason": "no asset file"}

            if info.get("id"):
                selector = f'#{info["id"]}'
            elif info.get("name"):
                selector = f'input[name="{info["name"]}"]'
            else:
                selector = "input[type=file]"

            filename = os.path.basename(filepath)
            print(f"[AI]     📎 Uploading {filename} → {selector}")
            await page.locator(selector).set_input_files(filepath)
            print(f"[AI]     📎 ✓ Uploaded {filename}")
            return {"executed": "upload", "file": filename}

        # ── 2. Fallback: tìm button/element có upload keyword ─────────────────
        UPLOAD_KW = ["upload", "image", "document", "file", "photo", "attach", "browse"]

        upload_btn = await page.evaluate("""
            (keywords) => {
                const isUploadEl = el => {
                    const text = (el.textContent || '').toLowerCase();
                    const cls  = (el.className  || '').toLowerCase();
                    return keywords.some(kw => text.includes(kw) || cls.includes(kw));
                };
                // button tag, hoặc element có class chứa 'button'/'btn'
                const candidates = Array.from(document.querySelectorAll(
                    'button, [class*="button"], [class*="btn"], [role="button"]'
                ));
                const found = candidates.find(isUploadEl);
                if (!found) return null;
                return {
                    tagName:   found.tagName,
                    text:      found.textContent.trim().slice(0, 80),
                    className: found.className,
                    id:        found.id || '',
                };
            }
        """, UPLOAD_KW)

        if not upload_btn:
            print(f"[AI]     📎 No upload input or button found")
            return {"executed": "upload_skipped", "reason": "no upload element"}

        filepath = _pick_file()  # không có accept info, dùng default
        if not filepath:
            return {"executed": "upload_skipped", "reason": "no asset file"}

        filename = os.path.basename(filepath)
        btn_id   = upload_btn.get("id", "")
        btn_cls  = upload_btn.get("className", "")
        btn_text = upload_btn.get("text", "")

        if btn_id:
            selector = f'#{btn_id}'
        else:
            # Tìm theo text chứa keyword
            kw_found = next((k for k in UPLOAD_KW if k in btn_text.lower()), None)
            selector = f'button:has-text("{btn_text[:30]}")' if btn_text else '[class*="button"]'

        print(f"[AI]     📎 Upload via button: {selector} | text='{btn_text[:40]}'")
        try:
            async with page.expect_file_chooser(timeout=4000) as fc_info:
                await page.locator(selector).first.click()
            file_chooser = await fc_info.value
            await file_chooser.set_files(filepath)
            print(f"[AI]     📎 ✓ Uploaded {filename} via file-chooser button")
            return {"executed": "upload", "file": filename, "via": "button"}
        except Exception as e:
            print(f"[AI]     📎 ✗ File-chooser failed: {e}")
            return {"executed": "upload_skipped", "reason": str(e)}
    
    async def _ask_llm(self, snapshot: str, context: dict) -> list[dict]:
        """
        Gửi page snapshot cho LLM, nhận về structured actions.

        LLM đọc accessibility tree → hiểu form fields, buttons, links
        → trả về JSON array of actions để MCP thực thi.
        """
        system_prompt = self._build_system_prompt()
        user_prompt = self._build_user_prompt(snapshot, context)

        response = await self._llm_client.chat.completions.create(
            model=self._llm_model,
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            max_tokens=self._llm_max_tokens,
            temperature=self._llm_temperature,
            response_format={"type": "json_object"},
        )

        response_text = response.choices[0].message.content
        parsed = json.loads(response_text)

        if isinstance(parsed, list):
            actions = parsed
        else:
            actions = parsed.get("actions", [])
        # LLM đôi khi trả string thay vì dict — lọc bỏ
        return [a for a in actions if isinstance(a, dict)]
    
    def _build_system_prompt(self) -> str:
        """
        THAY ĐỔI #1: Loại bỏ hardcoded examples về specific website patterns
        THAY ĐỔI #2: Tập trung vào principles thay vì concrete cases
        THAY ĐỔI #3: Generalize action categories
        """
        return """You are an intelligent web testing agent. You have TWO jobs:

JOB 1 — ANALYZE: Look for sensitive or valuable information on the page.
JOB 2 — TEST: Interact with unique functionalities on THIS PAGE ONLY.

Respond with JSON only:
{
  "sensitive_data": ["list of sensitive info found"],
  "actions": [...]
}

═══ JOB 1: SENSITIVE DATA ANALYSIS ═══
Scan the page for:
- Exposed emails, phone numbers, internal IPs, file paths
- API keys, tokens, secrets, credentials visible in the page
- User personal data (names, addresses, IDs) that shouldn't be public
- Configuration values, debug info, version numbers, stack traces
If nothing sensitive found, return "sensitive_data": []

═══ JOB 2: FUNCTIONALITY TESTING ═══

Action format:
- "type": "fill" | "click" | "type" | "select" | "upload_trigger"
- "ref": element ref from snapshot (e.g. "R12")
- "element": description of the element
- "value": text to fill (for fill/type/select)
- "submit": true = press Enter after typing (for type only)

████████████████████████████████████████████████████████████████
██  CORE PRINCIPLE #1 — NAVIGATION vs ACTION                  ██
████████████████████████████████████████████████████████████████

NAVIGATION elements change the page/URL — DO NOT CLICK:
✗ Links (<a href>), menu items, tabs that load new pages
✗ Product cards/names that navigate to detail pages
✗ Pagination (Next, Previous, page numbers)
✗ Breadcrumbs, footer links, sidebar navigation
✗ Category/filter links, "View details", "Read more"
✗ Logo/brand links, language selectors

ACTION elements perform operations on THIS PAGE — DO CLICK:
✓ Action buttons (Add to Cart, Like, Follow, Share, Subscribe)
✓ Rating widgets (stars, thumbs up/down)
✓ Checkboxes, radio buttons, toggle switches
✓ Dropdown menus for selecting options
✓ Expand/collapse within the same page
✓ Modal triggers (open popup/dialog on THIS PAGE)

DECISION RULE: If clicking changes the URL → SKIP. If it does something on the current page → INTERACT.

████████████████████████████████████████████████████████████████
██  CORE PRINCIPLE #2 — ONE REPRESENTATIVE SAMPLE             ██
████████████████████████████████████████████████████████████████

If you see a list/grid with 3+ similar items:
→ Pick the FIRST item as representative
→ Do NOT interact with item #2, #3, or any duplicates
→ ONE sample per pattern type

Example: If there are 10 "Add to Cart" buttons for different products:
- Click the FIRST one only
- Skip all others

████████████████████████████████████████████████████████████████
██  CORE PRINCIPLE #3 — FORMS AS HUMANS                       ██
████████████████████████████████████████████████████████████████

FORMS — act like a real human tester:
- Fill EVERY editable field (skip disabled/readonly)
- Clear pre-filled data and re-type with realistic values
- Use proper formats:
  • Emails: realistic format (test@example.com)
  • Phones: include country code (+1-555-...)
  • Names: realistic names, not "test" or "asdf"
  • Dates: valid dates in expected format
  • Numbers: within reasonable ranges
- Do NOT include submit button — handled separately

SKIP:
- Login/registration forms → return empty actions
- Cookie banners → skip
- Disabled/readonly fields → skip
- CAPTCHA/reCAPTCHA → skip (cannot solve programmatically)

████████████████████████████████████████████████████████████████
██  ACTION BUDGET & DEDUPLICATION                             ██
████████████████████████████████████████████████████████████████

Maximum 12 actions per page.

For each ACTION TYPE (not specific buttons), limit to 2 max:
- If you see 5 "Delete" buttons → click at most 2
- If you see 3 "Like" buttons → click at most 2
- If you see 10 "Export" buttons → click at most 2

Count by semantic meaning, not exact text:
- "Delete", "Remove", "Xóa" → same type
- "Add", "Create", "New", "Thêm" → same type
- "Like", "Upvote", "Heart" → same type

Before returning your actions:
1. Count how many actions per semantic type
2. If any type has >2, keep only the first 2
3. If total >12, prioritize forms first, then unique action types

████████████████████████████████████████████████████████████████
██  LANGUAGE & INTERNATIONALIZATION                           ██
████████████████████████████████████████████████████████████████

You may encounter pages in ANY language. Adapt accordingly:
- Recognize common patterns across languages
- "Submit" = "Gửi" (Vietnamese) = "Enviar" (Spanish) = "Kirim" (Indonesian)
- "Delete" = "Xóa" (Vietnamese) = "Eliminar" (Spanish) = "Hapus" (Indonesian)
- Use the same logic regardless of language

Do NOT hardcode language-specific keywords in your logic."""

    def _build_user_prompt(self, snapshot: str, context: dict) -> str:
        """
        THAY ĐỔI #4: Loại bỏ prescriptive instructions về số lượng action
        THAY ĐỔI #5: Focus on quality over quantity
        """
        return f"""URL: {context.get('url', '?')}
Page title: {context.get('title', '?')}

Page snapshot (accessibility tree):
{snapshot[:8000]}

Instructions:
1. ANALYZE: Scan for sensitive or valuable information. List in "sensitive_data".
2. IDENTIFY unique interaction patterns on this page (forms, action buttons, interactive widgets).
3. For FORMS: fill every editable field with realistic data.
4. For ACTION BUTTONS: test each unique action type (max 2 per type).
5. SKIP all navigation elements (links, menus, pagination, product cards).
6. Do NOT include submit button — handled automatically.
7. Keep total actions ≤12.

Return JSON: {{"sensitive_data": [...], "actions": [...]}}"""



    # ─── Action Execution ──────────────────────────────────

    async def _execute_action(self, action: dict) -> dict:
        atype = action.get("type", "")
        ref = action.get("ref", "")
        value = action.get("value", "")
        element = action.get("element", "")

        if atype == "fill":
            # Click vào field → select all → xóa → gõ lại (như người thật)
            await self.click(ref, element)
            await asyncio.sleep(0.3)
            await self.press_key("Backspace")
            await asyncio.sleep(0.1)
            await self.type_text(ref, value, submit=False)
            return {"executed": "fill", "ref": ref, "value": value}

        elif atype == "type":
            submit = action.get("submit", False)
            await self.click(ref, element)
            await asyncio.sleep(0.3)
            await self.press_key("Backspace")
            await asyncio.sleep(0.1)
            await self.type_text(ref, value, submit=submit)
            return {"executed": "type", "ref": ref, "value": value}

        elif atype == "select":
            await self.select_option(ref, [value] if isinstance(value, str) else value, element)
            return {"executed": "select", "ref": ref, "value": value}

        elif atype == "click":
            await self.click(ref, element)
            return {"executed": "click", "ref": ref, "element": element}

        return {"executed": "unknown", "type": atype}
    
    # ─── Cleanup ───────────────────────────────────────────

    async def disconnect(self):
        """Đóng MCP connection + cleanup"""
        await self._exit_stack.aclose()
        self._connected = False