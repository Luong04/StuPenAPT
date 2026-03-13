# Modal Watchdog - Auto Modal Dismiss System

## Tổng Quan

**Modal Watchdog** là cơ chế tự động **đóng modal/notification** khi script bị treo quá **10 giây**. Nó hoạt động **toàn bộ quá trình** (Phase 1, 2, 3) - không chỉ ở 1 phase nhất định.

## Kiến Trúc

### 1. **Global Modal Handlers**

```python
async def _try_close_modal_global(page: Page)
```
- **Toàn cục**, không phụ thuộc class
- 4-step strategy:
  1. **Escape key** (tương tác)
  2. **CSS pattern matching** (6 patterns)
  3. **JavaScript RegExp matching** (20 keywords)
  4. **Fallback mouse click** (vị trí viewport center)

### 2. **Timeout Wrapper**

```python
async def _wait_with_modal_timeout(operation_coro, timeout=10, page=None, retry=True)
```
- Wrapper cho các operation có thể treo (`page.goto()`, `page.fill()`, `page.click()`)
- **Flow**:
  1. Thực hiện operation với timeout
  2. **Nếu timeout (>10s)**: Tự động gọi `_try_close_modal_global()`
  3. **Nếu retry=True**: Thử lại operation
  4. **Nếu retry=False**: Trả về error

## Cách Hoạt Động

### Phase 1: LoginPageFinder
```python
# Thay vì:
await page.goto(self.base_url, timeout=30000)

# Bây giờ:
await _wait_with_modal_timeout(
    page.goto(self.base_url, timeout=30000),
    timeout=10,     # Auto-close modal nếu >10s
    page=page,
    retry=True      # Thử lại sau khi close modal
)
```

**Các operation được bảo vệ:**
- ✅ `page.goto()` - Điều hướng URL (3 lần)
- ✅ `page.fill()` - Điền form
- ✅ `page.click()` - Click nút

### Phase 2: MultiEndpointInterceptor
```python
# Tải trang login
await _wait_with_modal_timeout(
    page.goto(login_url, timeout=30000),
    timeout=10,
    page=page,
    retry=True
)

# Gọi modal dismissal ngay
await _try_close_modal_global(page)

# Điền form với timeout protection
await _wait_with_modal_timeout(
    page.fill(sel, test_email),
    timeout=5,
    page=page,
    retry=False
)
```

## Dismissal Strategies

### 1. **Escape Key** (Fastest)
- Thực hiện ngay
- Timeout: 0.5s

### 2. **CSS Patterns** (Reliable)
```
[id*='close'], [id*='dismiss'], [id*='cancel']
[class*='close-btn'], [class*='dismiss-btn']
[class*='modal-close'], [class*='modal-dismiss']
.close, .dismiss, .cancel, .overlay-close
[aria-label*='close' i], [aria-label*='dismiss' i]
button.btn-close, button.close-button, button.exit-button
```

### 3. **JavaScript RegExp** (Comprehensive)
20+ từ khóa:
- Dismiss actions: `dismiss`, `close`, `cancel`, `skip`, `exit`
- Accept actions: `ok`, `confirm`, `continue`, `next`, `got it`
- Decline actions: `no`, `ignore`, `decline`, `refuse`, `no thanks`
- Special: `x`, `done`, `later`, `maybe later`, `agreed`

### 4. **Fallback Click** (Last Resort)
- Click tại viewport center: `(width/6*5, height/4*3)`

## Logging

Tất cả watchdog action được log với `[WATCHDOG]` prefix:

```
[2026-03-02 10:15:30] [INFO] [WATCHDOG] Operation timed out after 10s, trying to close modal...
[2026-03-02 10:15:31] [INFO] [WATCHDOG] Closing modal via CSS: [id*='close']
[2026-03-02 10:15:32] [INFO] [WATCHDOG] Retrying operation...
```

## Configuration

| Parameter | Default | Vị Trí |
|-----------|---------|-------|
| `timeout` | 10s | `_wait_with_modal_timeout()` argument |
| `retry` | True | `_wait_with_modal_timeout()` argument |
| Modal step timeout | 0.5-2s | Inside `_try_close_modal_global()` |

## Kích Hoạt

### Manual Test
```bash
python3 smart_login.py -u http://localhost:3000 --userlist users.txt -w pass.txt --visible
```

Monitor output cho:
```
[WATCHDOG] Operation timed out...
[WATCHDOG] Closing modal via CSS...
[WATCHDOG] Retrying operation...
```

### Log File
```bash
tail -f attack.log | grep WATCHDOG
```

## Ví Dụ Thực Tế

### Scenario 1: Modal chặn tải trang
```
[10:15:30] Opening: http://localhost:3000
(trang load 12 giây do modal banner)
[10:15:40] [WATCHDOG] Operation timed out after 10s
[10:15:40] [WATCHDOG] Closing modal via CSS: [id*='dismiss']
[10:15:41] [WATCHDOG] Retrying operation...
[10:15:42] ✓ Successfully loaded page
```

### Scenario 2: Modal chặn form submit
```
[10:16:00] [*] Attempting to close any blocking modals...
(Proactive close trước fill form)
[10:16:00] [WATCHDOG] Closing modal via JavaScript
[10:16:01] Filling email "test@test.com"...
```

## Note

- **Timeout = 10s**: Dành cho `page.goto()` (chờ network)
- **Timeout = 5s**: Dành cho `page.fill()`, `page.click()` (chờ DOM)
- **retry = True**: Khi cần phải succeed (`page.goto`)
- **retry = False**: Khi lỗi OK (`page.fill`)

## Troubleshooting

| Vấn đề | Giải Pháp |
|--------|----------|
| Modal vẫn không đóng | Thêm CSS pattern hoặc JS keyword |
| Retry vô tận | Giảm `timeout` từ 10 → 5 |
| Quá nhều log WATCHDOG | Tăng `timeout` từ 10 → 15 |
| Modal mới xuất hiện sau đóng | Gọi `_try_close_modal_global()` lần 2 |

---

**Sự khác biệt so với trước:**
- ❌ Cũ: `_try_close_modal()` chỉ Phase 2 sau `page.goto()`
- ✅ Mới: `_try_close_modal_global()` **bất cứ lúc nào** trên **tất cả phases** khi operation timeout
