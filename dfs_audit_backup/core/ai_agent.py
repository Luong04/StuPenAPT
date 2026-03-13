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

    async def analyze_and_interact(self, page_context: dict) -> dict:
        """
        1. Snapshot → 2. LLM plan → 3. Execute fill only → 4. Upload if needed
        → 5. Re-snapshot → 6. LLM tìm submit trên fresh snapshot → 7. Click submit
        """
        # Step 1: Snapshot
        snapshot_text = await self.snapshot()
        print(f"[AI]     Snapshot: {len(snapshot_text)} chars")
        if not snapshot_text or len(snapshot_text) < 50:
            print(f"[AI]     ✗ Empty snapshot")
            return {"actions_taken": 0, "results": []}

        # Step 2: LLM plan actions (fill/type/select only, submit sẽ xử lý riêng)
        actions = await self._ask_llm(snapshot_text, page_context)
        print(f"[AI]     LLM planned {len(actions)} actions")
        for i, a in enumerate(actions):
            desc = (a.get('value') or a.get('element') or '')[:50]
            print(f"[AI]       [{i}] {a.get('type','')} ref={a.get('ref','')} {desc}")

        if not actions:
            return {"actions_taken": 0, "results": []}

        # Dedup clicks tương tự
        actions = self._dedup_similar_actions(actions, max_similar=2)

        # Step 3: Thực thi CHỈ fill/type/select/click (không submit)
        results = []
        had_fill = False

        for action in actions:
            atype = action.get("type", "")

            # Upload → xử lý riêng ở step 4
            if atype == "upload":
                continue

            # Nhận diện submit → BỎ QUA, sẽ dùng LLM tìm lại trên fresh snapshot
            if self._is_submit_action(action):
                print(f"[AI]     📋 Skipping submit from old plan, will re-find after fill")
                continue

            # Thực thi fill/click/type/select
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

        # Step 5: RE-SNAPSHOT sau khi fill xong
        if had_fill:
            await asyncio.sleep(1)
            print(f"[AI]     🔄 Re-snapshot after filling...")
            fresh_snapshot = await self.snapshot()
            print(f"[AI]     Fresh snapshot: {len(fresh_snapshot)} chars")

            # Step 6: Hỏi LLM tìm submit button trên FRESH snapshot
            submit_ref = await self._ask_llm_for_submit(fresh_snapshot, page_context)

            if submit_ref:
                ref = submit_ref.get("ref", "")
                el = submit_ref.get("element", "")
                print(f"[AI]     ➡ Submit (LLM found): ref={ref} {el[:50]}")
                await self.click(ref, el)
                results.append({"executed": "submit", "ref": ref, "element": el})
                print(f"[AI]     ✓ SUBMIT clicked")
                await self.wait_for(time_seconds=2)

                # Kiểm tra có issue không
                post_snap = await self.snapshot()
                if self._detect_issue(post_snap):
                    print(f"[AI]     ⚠ Submit issue → retrying...")
                    fix = await self._smart_retry(post_snap, page_context)
                    results.extend(fix)
            else:
                print(f"[AI]     ✗ LLM cannot find submit button")

        return {"actions_taken": len(actions), "results": results}

    def _is_submit_action(self, action: dict) -> bool:
        """Nhận diện action là submit/send button click"""
        atype = action.get("type", "")
        if atype in ("submit", "click_submit"):
            return True
        if atype != "click":
            return False
        el = (action.get("element") or "").lower()
        submit_kw = ["submit", "send", "save", "complain", "checkout",
                     "place order", "complete", "confirm", "post comment",
                     "post review", "gửi", "đăng"]
        return any(kw in el for kw in submit_kw)

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
- The correct button is usually at the bottom of the form container (form, card, div, section).
- Do NOT pick buttons from: navigation bars, sidebars, headers, footers, cookie banners, or other unrelated sections.
- Do NOT pick "Add to cart", "Login", "Register", "Search", "Menu", or navigation buttons.
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
        Chỉ dedup click actions tương tự. fill/type/select/upload giữ nguyên.
        So sánh 3 từ đầu của element text.
        """
        seen = {}
        result = []
        for a in actions:
            if a.get("type") not in ("click", "submit", "click_submit"):
                result.append(a)
                continue
            el = (a.get('element') or '').lower().strip()
            words = el.split()[:3]
            prefix = ' '.join(words) if words else el
            count = seen.get(prefix, 0)
            if count < max_similar:
                result.append(a)
                seen[prefix] = count + 1
            else:
                print(f"[AI]     ✂ Skip duplicate: {el[:50]}")
        if len(actions) != len(result):
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
        Upload file qua Playwright set_input_files trực tiếp.
        """
        import os

        browser_infra = self._config.get("_browser_infra")
        if not browser_infra or not hasattr(browser_infra, 'page') or not browser_infra.page:
            print(f"[AI]     📎 ✗ No browser page available")
            return {"executed": "upload_skipped", "reason": "no browser"}

        page = browser_infra.page

        # Tìm tất cả input[type=file] và accept attribute
        upload_info = await page.evaluate("""
            () => Array.from(document.querySelectorAll('input[type=file]')).map((el, i) => ({
                index: i,
                accept: el.getAttribute('accept') || '',
                id: el.id || '',
                name: el.name || '',
            }))
        """)

        if not upload_info:
            print(f"[AI]     📎 No input[type=file] found")
            return {"executed": "upload_skipped", "reason": "no file input"}

        info = upload_info[0]
        accept = (info.get('accept') or '').lower()

        # Chọn file phù hợp
        if any(x in accept for x in ['image', 'jpg', 'png', 'gif', '.jpg', '.png']):
            filename = 'test.jpg'
        elif 'pdf' in accept or '.pdf' in accept:
            filename = 'test.pdf'
        elif 'xml' in accept:
            filename = 'test.xml'
        else:
            filename = 'test.pdf'

        filepath = os.path.join(asset_dir, filename)
        if not os.path.exists(filepath):
            for f in os.listdir(asset_dir):
                filepath = os.path.join(asset_dir, f)
                filename = f
                break

        # Build selector
        if info.get('id'):
            selector = f'#{info["id"]}'
        elif info.get('name'):
            selector = f'input[name="{info["name"]}"]'
        else:
            selector = 'input[type=file]'

        print(f"[AI]     📎 Uploading {filename} → {selector}")
        locator = page.locator(selector)
        await locator.set_input_files(filepath)
        print(f"[AI]     📎 ✓ Uploaded {filename}")
        return {"executed": "upload", "file": filename}

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
        return """You are simulating a real human user interacting with a web page. Your goal is to interact with EVERY form field — just like a real person would.

Respond with JSON only: {"actions": [...]}

Action format:
- "type": "fill" | "click" | "type" | "select" | "upload"
- "ref": element ref from snapshot (e.g. "R12")
- "element": description of the element
- "value": text to fill (for fill/type/select)
- "submit": true = press Enter after typing (for type only)

HOW TO BEHAVE — ACT LIKE A REAL PERSON:
1. A real person touches EVERY field in a form, even if it already has data — UNLESS the field is disabled, readonly, or not editable. Skip those.
2. Go through fields one by one, top to bottom, like a human tabbing through a form.
3. For text fields that already have data: clear them and type new realistic data.
4. For empty fields: type appropriate realistic data matching the label/placeholder.
5. For dropdowns/selects: open them and pick a reasonable option, even if one is pre-selected.
6. For checkboxes/radio buttons: click them to select your choice, even if already checked.
7. For star ratings: click on a star to give a rating (e.g. 4 or 5 stars). Pick a reasonable rating that a normal satisfied user would give.
8. For sliders, range inputs, or any interactive widgets: interact with them naturally by choosing a reasonable value.
9. If there is any challenge, puzzle, or verification step, think it through logically and provide the correct answer.
10. Think about what each field is asking and type something that makes sense — real names, valid emails, proper phone formats, meaningful text.

IMPORTANT — TOUCH EVERY FIELD:
- Do NOT skip any visible input field, even if it looks pre-filled.
- BUT DO skip fields that are disabled, readonly, greyed out, or explicitly not editable.
- Generate a "fill" or "type" action for EVERY editable text input and textarea you see.
- Generate a "click" or "select" action for EVERY checkbox, radio button, and dropdown.
- Generate a "click" action for star rating elements — click the appropriate star (prefer 4 or 5 stars).
- The only exception: hidden fields (type="hidden") and disabled/readonly fields — skip those.

FINDING THE CORRECT SUBMIT BUTTON:
- Do NOT include submit button click in your actions — that is handled separately after you fill everything.
- Focus ONLY on filling and interacting with form fields.

SKIP RULES:
- Login or registration forms: return {"actions": []} — do not attempt.
- If the page has no fillable form, return {"actions": []}.
- Disabled or readonly fields: skip them entirely, do not generate actions for them.

Return ALL field interactions, even for pre-filled fields (except disabled/readonly ones)."""

    def _build_user_prompt(self, snapshot: str, context: dict) -> str:
        return f"""URL: {context.get('url', '?')}

Page snapshot (accessibility tree):
{snapshot[:8000]}

Instructions:
1. Identify ALL form fields on this page (inputs, textareas, selects, checkboxes, radio buttons, star ratings, sliders, etc).
2. Generate an action for EVERY editable field — do not skip any, even if they already have values.
3. SKIP fields that are disabled, readonly, or not editable.
4. For pre-filled fields: clear and re-type with your own realistic data.
5. For empty fields: fill with appropriate realistic data.
6. For selects/checkboxes/radios: interact with each one.
7. For star ratings: click a star to rate (prefer 4 or 5 stars as a satisfied user would).
8. Do NOT include a submit button click — that is handled automatically after your actions.

Return the complete action plan as JSON covering EVERY visible editable field."""



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
            await self.press_key("Control+a")
            await asyncio.sleep(0.1)
            await self.press_key("Backspace")
            await asyncio.sleep(0.1)
            await self.type_text(ref, value, submit=False)
            return {"executed": "fill", "ref": ref, "value": value}

        elif atype == "type":
            submit = action.get("submit", False)
            await self.click(ref, element)
            await asyncio.sleep(0.3)
            await self.press_key("Control+a")
            await asyncio.sleep(0.1)
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
