# DFS Web Crawler + Security Auditor

> **Hệ thống crawl web theo chiến lược DFS (Depth-First Search), kết hợp AI agent tự động tương tác với UI, bắt toàn bộ network traffic, và scan lỗ hổng bảo mật bằng Nuclei.**

---

## Mục lục

- [Tổng quan](#tổng-quan)
- [Kiến trúc hai lớp](#kiến-trúc-hai-lớp)
- [Cài đặt](#cài-đặt)
- [Cấu hình](#cấu-hình)
- [Cách chạy](#cách-chạy)
- [Chi tiết từng module](#chi-tiết-từng-module)
- [Luồng xử lý DFS](#luồng-xử-lý-dfs)
- [Output](#output)
- [Câu hỏi thường gặp](#câu-hỏi-thường-gặp)

---

## Tổng quan

### Hệ thống làm gì?

1. **Crawl DFS**: Duyệt website theo chiều sâu (DFS), tự động mở từng link theo thứ tự stack
2. **Bắt toàn bộ traffic**: Mọi HTTP request/response (headers + body) và WebSocket frames đều được ghi lại
3. **AI tương tác UI**: Khi gặp form, trang login, CAPTCHA — một AI agent (LLM) tự đọc trang, điền form, login, click button
4. **Scan lỗ hổng**: Các endpoint đã bắt được sẽ được quét bằng Nuclei templates
5. **Báo cáo**: Xuất JSON report tổng hợp kết quả crawl + vulnerabilities

### Tại sao dùng hai công cụ?

| Vấn đề | Playwright Python giải quyết | Playwright-MCP giải quyết |
|--------|------------------------------|---------------------------|
| Bắt response body | `page.route()` + `route.fetch()` → full body | MCP `browser_network_requests` **không có body** |
| WebSocket frames | CDP session trực tiếp | MCP **không hỗ trợ** |
| Backtrack state | `context.storage_state()` | MCP **không có** |
| HAR recording | Built-in `record_har_path` | MCP **không có** |
| AI đọc trang | ✗ Không có accessibility tree | `browser_snapshot()` → AI đọc được cấu trúc |
| AI điền form | ✗ Phải hardcode selector | `browser_fill_form()` → AI biết điền gì |
| AI click | ✗ Phải biết CSS selector | `browser_click(ref)` → AI chỉ đúng element |
| AI xử lý popup | ✗ Phức tạp | `browser_handle_dialog()` → tự động |

**→ Playwright Python = "phần máy móc" (hạ tầng, traffic, navigation)**
**→ Playwright-MCP = "đôi mắt của AI" (nhìn trang, tương tác thông minh)**

---

## Kiến trúc hai lớp

```
 ┌─────────────────────────────────────────────────────────────────┐
 │                      DFS Engine (Orchestrator)                  │
 │  Quản lý stack, depth, scope, visited set, periodic scan       │
 └────────────┬───────────────────────────────────┬────────────────┘
              │                                   │
    ┌─────────▼──────────┐             ┌──────────▼──────────┐
    │ 🎭 Playwright Python│             │ 🤖 Playwright-MCP    │
    │ (BrowserInfra)      │             │ (AIAgent + LLM)      │
    │                     │             │                      │
    │ • page.route()      │◄──── CÙNG ──►• browser_snapshot()  │
    │ • CDP session       │   BROWSER   │ • browser_fill_form()│
    │ • storage_state()   │  INSTANCE   │ • browser_click()    │
    │ • goto() / links    │             │ • browser_wait_for() │
    │ • add_init_script() │             │ • browser_handle_    │
    │ • HAR recording     │             │   dialog()           │
    └─────────┬──────────┘             └──────────────────────┘
              │
    ┌─────────▼──────────┐             ┌──────────────────────┐
    │ 📦 TrafficStore     │────────────►│ 🔍 NucleiRunner      │
    │ HTTP + WebSocket    │  export     │ Template-based scan  │
    │ JSONL persistence   │  urls.txt   │ Parse JSONL results  │
    └────────────────────┘             └──────────────────────┘
```

### CDP Sharing — Như thế nào hai lớp cùng dùng 1 browser?

```
 Browser (Chrome) launched với: --remote-debugging-port=9222
         │
         ├── Playwright Python kết nối qua internal pipe
         │     → Sở hữu toàn bộ Page object
         │     → page.route() bắt ALL traffic
         │     → CDP session bắt WebSocket
         │
         └── Playwright-MCP kết nối qua CDP endpoint http://localhost:9222
               → Nhìn CÙNG page qua accessibility tree
               → click/fill/type tác động lên CÙNG page
               → Mọi request do MCP tạo ra → page.route() vẫn bắt được
```

**Key insight**: Khi AI agent click submit button qua MCP, Playwright Python vẫn chạy nền bắt mọi request/response phát sinh → **không bỏ sót traffic nào**.

---

## Cài đặt

### Yêu cầu hệ thống

- Python 3.11+
- Node.js 18+ và npm (để chạy `@playwright/mcp`)
- [Nuclei](https://github.com/projectdiscovery/nuclei) (CLI scanner)
- GitHub account + Personal Access Token (dùng GitHub Models API — **KHÔNG cần OpenAI API key**)

### Bước 1: Cài Python dependencies

```bash
cd dfs_audit
pip install -r requirements.txt
```

Dependencies:
| Package | Vai trò |
|---------|---------|
| `playwright` | Browser automation API (lớp hạ tầng) |
| `mcp` | MCP Python SDK — giao tiếp với Playwright-MCP server |
| `openai` | LLM API client (hỗ trợ mọi OpenAI-compatible provider) |
| `pyyaml` | Load file config.yaml |
| `aiofiles` | Async file I/O |

### Bước 2: Cài Playwright browsers

```bash
playwright install chromium
```

### Bước 3: Cài Playwright-MCP server

```bash
npm install -g @playwright/mcp@latest
```

Kiểm tra: `npx @playwright/mcp@latest --help`

### Bước 4: Cài Nuclei

```bash
# Trên Linux
go install github.com/projectdiscovery/nuclei/v3/cmd/nuclei@latest

# Hoặc dùng package manager
# brew install nuclei  (macOS)
# sudo apt install nuclei  (nếu có trong repo)
```

Kiểm tra: `nuclei -version`

### Bước 5: Tạo GitHub Personal Access Token (PAT)

Hệ thống dùng **GitHub Models API** — tương thích OpenAI, KHÔNG cần mua OpenAI API key riêng. Dùng luôn tài khoản GitHub hiện tại.

1. Vào [github.com/settings/tokens](https://github.com/settings/tokens?type=beta)
2. Click **"Generate new token"** (Fine-grained token)
3. Đặt tên, ví dụ: `dfs-audit-llm`
4. Token expiration: chọn thời gian phù hợp
5. Không cần chọn repository hay permission đặc biệt (GitHub Models dùng account-level access)
6. Click **"Generate token"**, copy chuỗi `github_pat_...`

```bash
export GITHUB_TOKEN="github_pat_xxxxxxxxxxxx"
```

Hoặc thêm vào `~/.bashrc` / `~/.zshrc` để set vĩnh viễn.

> **Tại sao không cần OpenAI API key?** GitHub Models API cho phép bạn dùng GPT-4o, GPT-4o-mini, và nhiều model khác thông qua tài khoản GitHub. Endpoint `models.inference.ai.azure.com` tương thích 100% với OpenAI API format.

---

## Cấu hình

Toàn bộ cấu hình nằm trong file `config.yaml`. Dưới đây là giải thích chi tiết từng section:

### Target

```yaml
target: "https://example.com"
```

URL bắt đầu crawl. Có thể override bằng CLI: `--target https://target.com`.

### Browser (lớp Playwright Python)

```yaml
browser:
  headless: true              # true = không hiển thị UI, false = có UI
  cdp_port: 9222              # Port cho CDP (MCP sẽ kết nối qua port này)
  proxy: null                 # HTTP proxy (ví dụ: "http://127.0.0.1:8080" nếu dùng mitmproxy)
  user_agent: "Mozilla/5.0 ..." # User-Agent header
  har_path: "./output/session.har"  # File HAR ghi lại toàn bộ session
  init_script: "./scripts/init_inject.js"  # JS inject vào mọi page
```

**Proxy**: Nếu muốn deep SSL inspection (ví dụ kết hợp với mitmproxy), điền proxy URL vào đây. Browser sẽ route tất cả traffic qua proxy.

**HAR**: File `.har` ghi lại TOÀN BỘ network activity theo chuẩn HAR 1.2, có thể mở bằng Chrome DevTools hoặc HARViewer.

**Init script**: JavaScript được inject vào MỌI page trước khi page load. Script mặc định (`scripts/init_inject.js`) patch `fetch()`, `XMLHttpRequest`, và `sendBeacon` để log dynamic requests.

### Crawl Settings

```yaml
crawl:
  max_depth: 10               # Giới hạn chiều sâu DFS
  max_pages: 500              # Giới hạn tổng số trang crawl
  scope_pattern: "example.com"  # Chỉ crawl URL chứa pattern này (domain)
  exclude_patterns:            # Bỏ qua URL chứa các pattern này
    - "logout"
    - "signout"
    - "delete"
    - ".pdf"
    - ".zip"
    - ".png"
    # ...

  ai_trigger:                  # Khi nào kích hoạt AI agent
    on_forms: true             # Phát hiện form → gọi AI điền
    on_login: true             # Phát hiện login → gọi AI đăng nhập
    on_captcha: true           # Phát hiện captcha → gọi AI giải
```

**scope_pattern**: Chỉ crawl các URL có chứa pattern này trong domain. Ví dụ `"example.com"` sẽ match `https://example.com/page`, `https://sub.example.com/api`, nhưng bỏ qua `https://other-site.com`.

**exclude_patterns**: Danh sách các pattern cần tránh. Thường exclude logout (tránh mất session), delete actions (tránh xóa data), và static files (không cần kiểm tra bảo mật).

**ai_trigger**: Ba trigger mode quyết định khi nào gọi AI:
- `on_forms: true` → Khi page có `<form>` với `<input>` → AI đọc snapshot, quyết định điền gì, submit
- `on_login: true` → Khi page có `<input type="password">` → AI dùng credentials từ config để login
- `on_captcha: true` → (Phần mở rộng) Khi gặp CAPTCHA → AI cố giải

### LLM Settings (GitHub Models)

```yaml
llm:
  provider: "github"
  model: "gpt-4o"              # Hoặc "gpt-4o-mini", "o1-mini", etc.
  base_url: "https://models.inference.ai.azure.com"  # GitHub Models endpoint
  api_key_env: "GITHUB_TOKEN"  # Tên biến môi trường chứa GitHub PAT
  max_tokens: 4096
  temperature: 0.1             # 0.1 = deterministic, 1.0 = creative
```

**model**: Các model có sẵn trên GitHub Models:
- `gpt-4o` — Mạnh nhất, chính xác cao (khuyên dùng)
- `gpt-4o-mini` — Nhanh hơn, rẻ hơn, đủ tốt cho hầu hết trường hợp
- `o1-mini` — Model reasoning

**base_url**: Mặc định dùng GitHub Models (`models.inference.ai.azure.com`). Nếu muốn dùng OpenAI trực tiếp hoặc provider khác, đổi URL tương ứng.

**temperature**: Nên giữ thấp (0.1) cho security testing — cần actions nhất quán, logic.

### Credentials

```yaml
credentials:
  username: "testuser"
  password: "testpass123"
```

AI agent sẽ dùng credentials này khi gặp login form. Chỉ dùng cho test accounts — **đừng bao giờ dùng credentials thật**.

### Nuclei Scanner

```yaml
nuclei:
  enabled: true                # false = không chạy Nuclei
  templates:                   # Danh sách template categories
    - "cves"
    - "vulnerabilities"
    - "misconfiguration"
    - "exposed-panels"
  severity: "low,medium,high,critical"  # Filter theo severity
  rate_limit: 50               # Max requests/second
  timeout: 30                  # Timeout mỗi template (giây)
```

**enabled**: Tắt Nuclei nếu chỉ muốn crawl + bắt traffic mà không scan.

**templates**: Các category template của Nuclei. Xem thêm: `nuclei -tl` để list tất cả templates có sẵn.

### Output

```yaml
output:
  dir: "./output"
  traffic_log: "traffic.jsonl"       # Mọi HTTP request/response
  crawl_log: "crawl.jsonl"           # Log từng page đã crawl
  nuclei_results: "nuclei_results.jsonl"  # Kết quả scan
  report: "report.json"              # Báo cáo tổng hợp
```

---

## Cách chạy

### Chế độ cơ bản

```bash
python main.py
```

Dùng `config.yaml` mặc định.

### Chỉ định target từ CLI

```bash
python main.py --target https://testsite.com
```

Tự động set `scope_pattern` theo domain của target.

### Chế độ headless (không UI)

```bash
python main.py --target https://testsite.com --headless
```

Browser chạy ngầm, không hiển thị cửa sổ. Phù hợp cho server/CI.

### Chế độ headed (có UI — debug)

```bash
python main.py --target https://testsite.com --headed
```

Mở browser có giao diện, bạn có thể theo dõi AI agent click, điền form trong real-time. **Rất hữu ích khi debug.**

### Giới hạn crawl

```bash
python main.py --target https://testsite.com --max-depth 3 --max-pages 50
```

Crawl tối đa 3 cấp sâu, tối đa 50 trang. Dùng khi muốn test nhanh.

### Dùng config tùy chỉnh

```bash
python main.py --config my_config.yaml
```

### Tổng hợp tất cả CLI arguments

| Argument | Mặc định | Mô tả |
|----------|----------|-------|
| `--config` | `config.yaml` | Đường dẫn file config |
| `--target` | (từ config) | Override target URL |
| `--max-depth` | (từ config) | Override max depth của DFS |
| `--max-pages` | (từ config) | Override max pages |
| `--headless` | (từ config) | Chạy browser không UI |
| `--headed` | (từ config) | Chạy browser có UI |

---

## Chi tiết từng module

### `core/browser_infra.py` — 🎭 Lớp hạ tầng Playwright Python

**Vai trò**: Sở hữu và quản lý browser. Chạy NỀN liên tục. Không cần AI.

**Các chức năng chính**:

| Method | Chức năng |
|--------|-----------|
| `launch(traffic_store)` | Khởi động browser, setup CDP, route capture, HAR, init script |
| `navigate(url)` | `page.goto()` — điều hướng do DFS engine kiểm soát |
| `extract_links(base_url)` | Evaluate JS để lấy tất cả link từ page (a[href], form[action], onclick) |
| `has_forms()` | Detect `<form>` có interactive inputs → trigger AI |
| `has_login_form()` | Detect `<input type=password>` → trigger AI login |
| `has_file_upload()` | Detect `<input type=file>` |
| `save_state()` | `context.storage_state()` — snapshot cookies + localStorage cho backtracking |
| `restore_state(state)` | Khôi phục browser state từ snapshot |
| `close()` | Tắt browser, CDP, tất cả connections |

**Traffic capture hoạt động thế nào?**

```
Mọi request trên page
       │
  page.route("**/*")
       │
       ▼
  Resource type?
       │
  ┌────┴────┐
  │ document │──► route.fetch() → đọc body → store → route.fulfill()
  │ xhr      │    (FULL capture: headers + body)
  │ fetch    │
  └─────────┘
       │
  ┌────┴────┐
  │ image   │──► store metadata → route.continue_()
  │ css     │    (metadata only, pass through nhanh)
  │ font    │
  │ script  │
  └─────────┘
```

- **Kiểu quan trọng** (document, xhr, fetch): Dùng `route.fetch()` để forward request, đọc response body, rồi `route.fulfill()` trả về cho browser. → Bắt được TOÀN BỘ headers + body.
- **Kiểu tĩnh** (image, css, font): Chỉ log metadata (URL, method), rồi `route.continue_()` để không ảnh hưởng performance.

### `core/ai_agent.py` — 🤖 Lớp AI (Playwright-MCP + LLM)

**Vai trò**: Đôi mắt và bàn tay của AI. CHỈ được gọi khi DFS engine phát hiện page cần tương tác thông minh.

**Luồng xử lý khi AI được kích hoạt**:

```
 DFS Engine phát hiện form/login
       │
       ▼
 1. browser_snapshot()          ← MCP: chụp accessibility tree
       │
       ▼
 2. Gửi snapshot cho LLM       ← OpenAI API: "page trông thế này, tôi nên làm gì?"
       │
       ▼
 3. LLM trả về JSON actions    ← [{"type":"fill","ref":"R12","value":"test@test.com"}, ...]
       │
       ▼
 4. Thực thi từng action        ← MCP: browser_fill_form(), browser_click(), ...
       │
       ▼
 5. Chờ + snapshot lại          ← Xem kết quả (success? error? redirect?)
```

**MCP tools sử dụng**:

| MCP Tool | Khi nào dùng |
|----------|-------------|
| `browser_snapshot()` | AI cần "nhìn" cấu trúc trang trước khi quyết định |
| `browser_fill_form(fields)` | Điền nhiều form fields cùng lúc |
| `browser_click(ref)` | Click element (submit button, link, etc.) |
| `browser_type(ref, text)` | Nhập text vào input/textarea |
| `browser_wait_for(text/time)` | Chờ sau submit (đợi "Success" hoặc N giây) |
| `browser_handle_dialog(accept)` | Xử lý alert/confirm/prompt popup |
| `browser_select_option(ref, values)` | Chọn option trong dropdown `<select>` |

**LLM prompt**: AI được instruct như một web security auditor:
- Login form → điền credentials, click submit
- Search form → thử `' OR 1=1 --` (SQL injection test)
- Registration form → điền data thật
- Luôn submit form sau khi điền
- Ghi nhận error messages, hidden inputs, CSRF tokens

### `core/dfs_engine.py` — 🔄 DFS Orchestrator

**Vai trò**: Não bộ của hệ thống. Kết nối và điều phối tất cả modules.

**5 phases thực thi**:

| Phase | Hành động |
|-------|-----------|
| 1 | Launch 🎭 Playwright → browser sẵn sàng, traffic capture chạy nền |
| 2 | Connect 🤖 MCP → AI agent kết nối vào CÙNG browser qua CDP |
| 3 | DFS crawl loop → pop stack, navigate, check form, extract links, push |
| 4 | Final 🔍 Nuclei scan → quét toàn bộ endpoints đã bắt |
| 5 | Generate report → JSON summary + export data files |

**DFS loop chi tiết**:

```
Stack: [start_url (depth=0)]

while stack not empty AND pages_crawled < max_pages:
    node = stack.pop()

    if URL đã visited → skip
    if URL ngoài scope → skip
    if depth > max_depth → skip

    visited.add(normalize(URL))

    🎭 browser.navigate(URL)          ← tự động bắt traffic
    check has_forms() / has_login()

    if cần AI:
        🤖 agent.analyze_and_interact()   ← snapshot → LLM → actions → execute

    links = 🎭 browser.extract_links()
    in_scope = filter(links, scope)

    for link in reversed(in_scope):    ← reversed vì DFS dùng stack
        stack.push(CrawlNode(url=link, depth=depth+1))

    if pages_crawled % 20 == 0:
        🔍 nuclei.scan(traffic.export_urls())
```

### `core/traffic_store.py` — 📦 Traffic Storage

**Vai trò**: Kho lưu trữ mọi network traffic. Vừa in-memory (cho query nhanh) vừa persistent JSONL (không mất data nếu crash).

| Method | Chức năng |
|--------|-----------|
| `store_http(req, resp)` | Lưu 1 HTTP exchange, append vào JSONL file |
| `store_websocket(direction, params)` | Lưu 1 WebSocket frame từ CDP |
| `get_unique_urls()` | Unique URLs (dedup) cho Nuclei |
| `get_endpoints_with_params()` | Endpoints có parameters → target cho security testing |
| `export_urls_file()` | Export `urls.txt` cho `nuclei -l` |
| `export_raw_requests()` | Export `requests.json` chi tiết |
| `export_parameterized()` | Export `params.json` — endpoints có params |

### `core/nuclei_runner.py` — 🔍 Nuclei Scanner

**Vai trò**: Wrapper cho Nuclei CLI. Nhận URLs từ TrafficStore, chạy Nuclei subprocess, parse kết quả JSONL.

| Method | Chức năng |
|--------|-----------|
| `scan_urls(urls_file)` | Scan danh sách URLs từ file (`nuclei -l urls.txt -jsonl`) |
| `scan_single(url)` | Scan 1 URL đơn lẻ |
| `export_results()` | Export kết quả scan ra file |

**Khi nào Nuclei được trigger?**
- Periodic: mỗi 20 trang đã crawl
- Final: sau khi crawl xong toàn bộ

### `scripts/init_inject.js` — JavaScript Injection

**Vai trò**: Inject vào MỌI page qua `add_init_script()` TRƯỚC khi page load. Patch các API dynamic request:

- `window.fetch()` → log `[DFS_CRAWL] fetch: GET https://...`
- `XMLHttpRequest.open()` → log `[DFS_CRAWL] XHR: POST https://...`
- `navigator.sendBeacon()` → log `[DFS_CRAWL] beacon: https://...`
- Expose `window.__dfs_extractURLs()` — helper collect tất cả URL từ page

---

## Luồng xử lý DFS

### Ví dụ thực tế

```
Crawl https://shop.example.com

[DFS] [1/500] depth=0 → https://shop.example.com
  ✓ Shop Home Page
  📋 Form detected → triggering AI agent
  🤖 AI: snapshot → thấy search form → type "test" → submit
  🔗 15 new links (stack: 15)

[DFS] [2/500] depth=1 → https://shop.example.com/login
  ✓ Login Page
  🔐 Login form → triggering AI agent
  🤖 AI: snapshot → thấy email + password → fill credentials → click Login
  🔗 3 new links (stack: 17)

[DFS] [3/500] depth=2 → https://shop.example.com/account/profile
  ✓ My Profile
  📋 Form detected → triggering AI agent
  🤖 AI: snapshot → thấy profile form → fill test data → submit
  🔗 8 new links (stack: 24)

[DFS] [4/500] depth=2 → https://shop.example.com/account/orders
  ✓ Order History
  🔗 5 new links (stack: 28)

...

[NUCLEI] 🔍 Scanning 45 unique URLs...
[NUCLEI] ⚠ Found 2 issues:
  [HIGH] CVE-2024-XXXX → https://shop.example.com/api/users
  [MEDIUM] misconfiguration-xxx → https://shop.example.com/.env
```

### Quy trình quyết định "có gọi AI không?"

```
Page loaded
    │
    ├── has_login_form()? (input[type=password])
    │     └── Có → purpose = "login", gọi AI với credentials
    │
    ├── has_forms()? (form + interactive inputs)
    │     └── Có → purpose = "form_fill", gọi AI
    │
    └── Không có form
          └── Chỉ extract links, KHÔNG gọi AI (tiết kiệm LLM tokens)
```

---

## Output

Sau khi chạy xong, thư mục `output/` chứa:

```
output/
├── traffic.jsonl          # Toàn bộ HTTP request/response (JSONL format)
├── crawl.jsonl            # Log từng page đã crawl (url, depth, status, has_forms, ai_interacted)
├── session.har            # HAR file — mở bằng Chrome DevTools > Network > Import HAR
├── urls.txt               # Unique URLs (dùng cho nuclei -l)
├── requests.json          # HTTP exchanges chi tiết (document/xhr/fetch only)
├── params.json            # Endpoints có parameters (quan trọng cho pentest)
├── nuclei_raw.jsonl       # Raw Nuclei output
├── nuclei_results.jsonl   # Parsed Nuclei results
└── report.json            # Báo cáo tổng hợp
```

### Giải thích report.json

```json
{
  "target": "https://example.com",
  "timestamp": 1710000000,
  "duration_seconds": 123.4,
  "summary": {
    "pages_crawled": 150,
    "total_requests": 2340,
    "websocket_frames": 12,
    "unique_endpoints": 89,
    "parameterized_endpoints": 34,
    "vulnerabilities_found": 3
  },
  "vulnerabilities": [
    {
      "template": "CVE-2024-XXXX",
      "severity": "high",
      "url": "https://example.com/api/users",
      "description": "..."
    }
  ],
  "crawl_log": [...]
}
```

### Xem HAR file

1. Mở Chrome → DevTools (F12) → Network tab
2. Click nút Import (hoặc kéo thả file `session.har`)
3. Xem toàn bộ network history của session crawl

---

## Câu hỏi thường gặp

### Q: Có thể dùng model LLM khác không?

Có. Bất kỳ provider nào tương thích OpenAI API đều dùng được. Chỉ cần đổi `base_url` và `api_key_env` trong config:

```yaml
# GitHub Models (mặc định)
llm:
  model: "gpt-4o"
  base_url: "https://models.inference.ai.azure.com"
  api_key_env: "GITHUB_TOKEN"

# OpenAI trực tiếp
llm:
  model: "gpt-4o"
  base_url: "https://api.openai.com/v1"
  api_key_env: "OPENAI_API_KEY"

# Ollama (local, miễn phí, không cần API key)
llm:
  model: "llama3.1"
  base_url: "http://localhost:11434/v1"
  api_key_env: "OLLAMA_KEY"  # set OLLAMA_KEY="ollama" (dummy value)
```

Code dùng thư viện `openai` với `base_url` tuỳ chỉnh, nên tương thích với mọi provider có OpenAI-compatible API.

### Q: Có thể tắt AI agent không?

Có. Set tất cả trigger thành `false`:

```yaml
crawl:
  ai_trigger:
    on_forms: false
    on_login: false
    on_captcha: false
```

Hệ thống sẽ chỉ crawl + bắt traffic, không gọi LLM.

### Q: Có thể tắt Nuclei scan không?

Có:

```yaml
nuclei:
  enabled: false
```

### Q: Sao cần proxy?

Không bắt buộc. Mặc định `proxy: null`. Nếu muốn:
- **mitmproxy**: Deep SSL inspection, xem traffic chi tiết hơn
- **Burp Suite**: Kết hợp manual testing với automated crawl
- **Corporate proxy**: Nếu target nằm sau firewall

```yaml
browser:
  proxy: "http://127.0.0.1:8080"
```

### Q: Scope hoạt động thế nào?

`scope_pattern` là substring match trên domain (netloc):

```yaml
scope_pattern: "example.com"
```

- ✅ `https://example.com/page` → match
- ✅ `https://sub.example.com/api` → match
- ❌ `https://other-site.com/link` → skip
- ❌ `https://evil.com/example.com/fake` → skip (check netloc, không check full URL)

### Q: DFS vs BFS — tại sao dùng DFS?

- **DFS** đi sâu trước → nhanh chóng đạt đến các trang ẩn (admin panel, deep API endpoints)
- **DFS** dùng ít bộ nhớ hơn BFS (stack vs queue)
- **DFS** phù hợp cho security testing: muốn tìm trang "sâu nhất" có thể
- Nếu muốn BFS, chỉ cần đổi `self._stack.pop()` thành `self._stack.pop(0)` trong `dfs_engine.py`

### Q: Tôi muốn crawl site có JavaScript rendering nặng (SPA)?

Đã hỗ trợ sẵn. Playwright chạy full Chromium browser → render JavaScript hoàn chỉnh. Thêm vào đó:
- `init_inject.js` patch `fetch()` và `XHR` → bắt được mọi API call
- `page.route("**/*")` bắt tất cả request kể cả dynamic
- `wait_for_load_state("networkidle")` chờ cho đến khi mọi request hoàn tất

---

## Cấu trúc tổng quan files

```
dfs_audit/
├── main.py                    # Entry point, CLI argument parsing
├── config.yaml                # Cấu hình toàn bộ hệ thống
├── requirements.txt           # Python dependencies
├── core/
│   ├── __init__.py
│   ├── browser_infra.py       # 🎭 Playwright Python — hạ tầng, traffic capture
│   ├── ai_agent.py            # 🤖 Playwright-MCP + LLM — AI tương tác UI
│   ├── dfs_engine.py          # 🔄 DFS orchestrator — điều phối tất cả
│   ├── traffic_store.py       # 📦 Lưu trữ HTTP + WebSocket traffic
│   └── nuclei_runner.py       # 🔍 Nuclei CLI wrapper
├── scripts/
│   └── init_inject.js         # JS inject vào mọi page (patch fetch/XHR)
└── output/                    # (tạo khi chạy) kết quả crawl + scan
```

---

## Lưu ý quan trọng

- **Chỉ dùng cho mục đích kiểm thử bảo mật hợp pháp**. Luôn có sự cho phép bằng văn bản trước khi pentest.
- **Credentials trong config**: Chỉ dùng test accounts. Không bao giờ lưu credentials thật trong config file.
- **Rate limiting**: Điều chỉnh `nuclei.rate_limit` và thêm delay nếu target server yếu.
- **exclude_patterns**: Luôn exclude logout, delete, và các destructive actions.
