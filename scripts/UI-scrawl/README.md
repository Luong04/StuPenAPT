## UI-scrawl: PlaywrightCrawler + filter_and_test + Nuclei

Thư mục này gom toàn bộ pipeline bạn yêu cầu vào 1 chỗ:

- `js_crawler/` (Node.js): dùng Crawlee `PlaywrightCrawler` để crawl + bắt traffic
- `filter_and_test_server.py` (Python): lọc request đáng test + chạy `nuclei`

Hiện tại pipeline này tập trung mạnh vào:
- Cào toàn bộ endpoint (same-origin) và mở rộng bề mặt qua chiến lược click các nút “reveal” (menu/account/settings...).
- Inject cookie từ `scripts/session_cookies.json` (token) trước navigation.
- Proxy traffic qua `http://127.0.0.1:8080` để bạn xem trên Burp.

Chưa fully-integrate phần “playwright-mcp xử lý nghiệp vụ form” vì cần bạn xác nhận kiến trúc chia sẻ browser/CDP.

---

## 1) Chuẩn bị

### Node deps (PlaywrightCrawler)

```bash
cd scripts/UI-scrawl/js_crawler
npm install
```

### Nuclei + Burp proxy

- Đảm bảo `nuclei` có thể chạy từ terminal (`nuclei --version`).
- Burp phải lắng nghe tại `127.0.0.1:8080`.

---

## 2) Chạy filter_and_test (Python)

Mở terminal 1:

```bash
cd scripts/UI-scrawl
python3 filter_and_test_server.py
```

Mặc định:
- Lắng nghe: `http://127.0.0.1:9000/traffic`
- Nuclei targets: `scripts/katana_output/nuclei_targets.txt`
- Kết quả: `scripts/katana_output/nuclei_results.jsonl`

Bạn có thể chỉnh chu kỳ:

```bash
NUCLEI_BATCH_INTERVAL=120 python3 filter_and_test_server.py
```

---

## 3) Chạy crawler (Node)

Mở terminal 2:

```bash
cd scripts/UI-scrawl/js_crawler
node crawl_endpoints.mjs \
  --url https://preview.owasp-juice.shop/#/ \
  --visible \
  --debugFlow \
  --proxyUrl http://127.0.0.1:8080 \
  --cookieFile ../../session_cookies.json \
  --outputDir ../../katana_output \
  --maxDepth 7 \
  --maxRequestsPerCrawl 2000 \
  --concurrency 5
```

Gợi ý:
- `--visible` = bật chế độ headful để nhìn rõ crawler đang click gì / enqueue gì.
- `--debugFlow` = log chi tiết thứ tự inject cookies, click reveal triggers, số lượng URLs được enqueue.

---

## 4) Chiến lược “reveal by clicking” (SPA)

Trên mỗi trang, crawler sẽ:
1. Wait DOM readiness (`domcontentloaded` + selector có interactive)
2. `revealUI()`:
   - tìm các nút có `text/class/aria-label` chứa keywords: `menu, account, profile, settings,...`
   - click một số lượng giới hạn (`--maxRevealClicks`)
3. Sau khi panel/modal/drawer mở ra:
   - extract thêm `a[href]` + `data-href/data-url/data-link`
   - cố gắng lấy router/manifest links (Next.js / React router) theo best-effort
4. enqueue các link same-origin vào request queue

Ngoài ra crawler còn dùng `enqueueLinksByClickingElements()` để bắt các điều hướng được sinh ra bởi click.

---

## 5) playwight-mcp integration cần bạn xác nhận

Để “điền form + gửi form + thao tác dữ liệu” bằng playwright-mcp cho **đúng trên đúng trang đang crawler duyệt**, có 2 hướng:

1. **Shared browser/CDP** (đúng luồng nhất)
   - Node crawler phải launch browser với `--remote-debugging-port`
   - Python/LLM agent qua playwright-mcp connect vào cùng browser (CDP) và thao tác trực tiếp
   - Phải đảm bảo chỉ có 1 instance browser (hoặc quản lý port theo browser) để tránh xung đột

2. **Separate run per URL** (dễ implement hơn nhưng không “liền mạch state”)
   - Node crawler chỉ crawl + bắt traffic
   - Một quá trình Python riêng sẽ mở URL, inject cookie, rồi dùng playwright-mcp làm nghiệp vụ
   - Luồng công việc không chia sẻ cùng state UI của Node crawler

Bạn muốn theo hướng nào?
- Trả lời: `1` (shared CDP) hoặc `2` (separate per URL).

Sau khi bạn chọn, mình sẽ code nốt phần playwright-mcp để fill/submit/click chính xác và vẫn giữ pipeline filter_and_test liên tục.

