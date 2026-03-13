 🔍 RECON & CRAWL TOOLKIT

Bộ công cụ reconnaissance tách biệt gồm 2 scripts:
1. **subdomain_recon.sh** - Tìm subdomain sống
2. **deep_crawl.sh** - Crawl sâu single target với session support

---

## 📦 YÊU CẦU

### Tools cần cài đặt:

```bash
# Script 1 (subdomain_recon.sh)
go install -v github.com/projectdiscovery/subfinder/v2/cmd/subfinder@latest
go install -v github.com/OJ/gobuster/v3@latest
go install -v github.com/projectdiscovery/dnsx/cmd/dnsx@latest
go install -v github.com/projectdiscovery/httpx/cmd/httpx@latest

# Script 2 (deep_crawl.sh)
go install -v github.com/projectdiscovery/katana/cmd/katana@latest
go install -v github.com/lc/gau/v2/cmd/gau@latest
go install -v github.com/epi052/feroxbuster@latest
pip3 install uro
sudo apt install jq  # hoặc brew install jq
```

---

## 🚀 SCRIPT 1: SUBDOMAIN RECON

### Mục đích:
- Tìm subdomain bằng Subfinder (passive) + Gobuster (active)
- Verify DNS resolution bằng dnsx
- Probe web servers bằng httpx

### Output:
- `live_subdomains.txt` - Danh sách subdomain sống
- `live_urls.txt` - Danh sách URL web servers

### Sử dụng:

```bash
chmod +x subdomain_recon.sh

# Cơ bản
./subdomain_recon.sh example.com

# Output sẽ nằm trong thư mục: subdomain_recon/
```

### Workflow:

```
Input: example.com
  ↓
[1] Subfinder → passive enumeration
  ↓
[2] Gobuster DNS → active bruteforce
  ↓
[3] Merge & Deduplicate
  ↓
[4] DNSX → verify DNS resolution
  ↓
[5] HTTPX → probe web servers (200,201,401,403)
  ↓
Output: live_subdomains.txt (subdomains)
        live_urls.txt (URLs)
```

### Ví dụ Output:

**live_subdomains.txt:**
```
api.example.com
dev.example.com
staging.example.com
www.example.com
```

**live_urls.txt:**
```
https://api.example.com
https://dev.example.com
https://staging.example.com
https://www.example.com
```

---

## 🔎 SCRIPT 2: DEEP CRAWL

### Mục đích:
- Crawl sâu 1 target URL
- Hỗ trợ session cookies (từ auto login script)
- Tách riêng GET và POST endpoints

### Output:
- `get_endpoints.txt` - GET endpoints (dùng cho Nuclei)
- `post_endpoints.txt` - POST endpoints với body parameters

### Sử dụng:

```bash
chmod +x deep_crawl.sh

# Crawl không có session (unauthenticated)
./deep_crawl.sh https://example.com

# Crawl với session cookies (authenticated)
./deep_crawl.sh https://example.com session_cookies.json

# Custom session file path
./deep_crawl.sh https://example.com /path/to/custom_session.json
```

### Session File Format:

Script tự động đọc file `session_cookies.json` (được tạo bởi auto login script):

```json
{
  "timestamp": "2024-01-01T12:00:00",
  "cookies": {
    "sessionid": "abc123...",
    "csrftoken": "xyz789..."
  },
  "headers": {
    "User-Agent": "Mozilla/5.0..."
  },
  "cookie_string": "sessionid=abc123; csrftoken=xyz789",
  "base_url": "https://example.com"
}
```

Script sẽ extract:
- `cookie_string` → Dùng làm Cookie header
- `headers.User-Agent` → Custom User-Agent (optional)

### Workflow:

```
Input: https://example.com + session_cookies.json (optional)
  ↓
[0] Check session file → Extract cookies/headers
  ↓
[1] Detect modern web (React/Vue/SPA)
  ↓
[2] Katana Crawler:
    - GET endpoints (qurl format)
    - POST endpoints (form extraction với body)
  ↓
[3] GAU (archived URLs) → Verify với httpx
  ↓
[4] Feroxbuster (directory bruteforce)
  ↓
[5] Merge & Deduplicate với uro
  ↓
Output: get_endpoints.txt
        post_endpoints.txt
```

### Ví dụ Output:

**get_endpoints.txt:**
```
https://example.com/
https://example.com/api/users?page=1
https://example.com/api/users?page=2
https://example.com/dashboard
https://example.com/profile?id=123
```

**post_endpoints.txt:**
```
https://example.com/api/login [BODY: {"username":"","password":""}]
https://example.com/api/register [BODY: {"email":"","password":"","confirm":""}]
https://example.com/api/update-profile [BODY: {"name":"","bio":""}]
```

---

## 🎯 USE CASE SCENARIOS

### Scenario 1: Reconnaissance từ đầu (Chưa có target)

```bash
# Bước 1: Tìm subdomains
./subdomain_recon.sh example.com

# Kết quả: subdomain_recon/live_urls.txt

# Bước 2: Crawl từng URL
while read url; do
    ./deep_crawl.sh "$url"
done < subdomain_recon/live_urls.txt
```

### Scenario 2: Đã có 1 target cụ thể

```bash
# Crawl trực tiếp
./deep_crawl.sh https://target.com
```

### Scenario 3: Đã login được (có session)

```bash
# Bước 1: Chạy auto login script (của bạn)
python3 intruder.py -u https://target.com -U admin -w passwords.txt

# Kết quả: session_cookies.json

# Bước 2: Crawl với session
./deep_crawl.sh https://target.com session_cookies.json

# Kết quả: Được crawl vùng authenticated (dashboard, profile, admin...)
```

### Scenario 4: Pipeline đầy đủ → Nuclei

```bash
# 1. Recon subdomains
./subdomain_recon.sh example.com

# 2. Pick 1 target để deep crawl
./deep_crawl.sh https://api.example.com

# 3. Scan với Nuclei
nuclei -l deep_crawl/get_endpoints.txt -t ~/nuclei-templates/ -o nuclei_results.txt

# 4. Scan POST endpoints (nếu có templates hỗ trợ)
nuclei -l deep_crawl/post_endpoints.txt -t ~/nuclei-templates/http/vulnerabilities/ -o nuclei_post_results.txt
```

---

## ⚙️ TÙNG BIẾN

### Script 1: subdomain_recon.sh

Sửa trong script nếu cần:

```bash
# Wordlist cho Gobuster
WORDLIST="/path/to/your/wordlist.txt"

# Status codes cho httpx
httpx -mc 200,201,401,403  # Thêm/bớt codes
```

### Script 2: deep_crawl.sh

```bash
# Katana depth (dòng ~130, ~140)
-d 3  # Tăng lên 4-5 nếu muốn crawl sâu hơn

# Feroxbuster depth (dòng ~220)
--depth 1  # Tăng lên 2-3 cho bruteforce sâu hơn

# Feroxbuster time limit (dòng ~220)
--time-limit 10m  # Tăng lên 20m, 30m nếu target lớn
```

---

## 📊 PERFORMANCE TIPS

### Tối ưu tốc độ:

```bash
# Script 1: Giảm wordlist Gobuster
# Dùng top 1000 thay vì 5000
WORDLIST="subdomains-top1million-1000.txt"

# Script 2: Giảm depth
katana -d 2  # Thay vì -d 3
feroxbuster --depth 1  # Giữ nguyên
```

### Tối ưu kết quả (chậm hơn nhưng đầy đủ):

```bash
# Script 1: Dùng wordlist lớn
WORDLIST="subdomains-top1million-20000.txt"

# Script 2: Tăng depth
katana -d 4
feroxbuster --depth 2 --time-limit 30m
```

---

## 🔍 TROUBLESHOOTING

### Lỗi: "tool not found"

```bash
# Kiểm tra PATH
echo $PATH
export PATH=$PATH:$HOME/go/bin

# Hoặc symlink
sudo ln -s ~/go/bin/subfinder /usr/local/bin/
```

### Lỗi: "No subdomains found"

```bash
# Thử chạy manual để debug
subfinder -d example.com -v
gobuster dns -d example.com -w wordlist.txt -v
```

### Lỗi: "jq: command not found"

```bash
# Ubuntu/Debian
sudo apt install jq

# macOS
brew install jq

# CentOS/RHEL
sudo yum install jq
```

### Session cookies không hoạt động

```bash
# Kiểm tra format JSON
jq . session_cookies.json

# Kiểm tra cookie_string có tồn tại không
jq -r '.cookie_string' session_cookies.json

# Test manual
curl -H "Cookie: $(jq -r '.cookie_string' session_cookies.json)" https://target.com
```

---

## 📈 EXPECTED RESULTS

### Với domain nhỏ (< 10 subdomains):
- Script 1: ~2-5 phút
- Script 2: ~5-10 phút/target

### Với domain trung bình (10-50 subdomains):
- Script 1: ~5-15 phút
- Script 2: ~10-20 phút/target

### Với domain lớn (> 100 subdomains):
- Script 1: ~20-60 phút
- Script 2: Nên chạy parallel cho nhiều targets

---

## 🎓 NOTES

### Về status codes:

**Script 1 (httpx):** Giữ 200,201,401,403
- `401/403`: Để tìm protected endpoints (có thể bypass)

**Script 2 (feroxbuster):** Chỉ giữ 200,201
- Bỏ `401/403` vì feroxbuster tạo quá nhiều false positives

**Script 2 (GAU + httpx):** Giữ 200,201,401,403
- Archive URLs ít rác hơn nên giữ `401/403`

### Về POST endpoints:

Format output:
```
URL [BODY: json_string]
```

Nuclei có thể parse format này nếu template được viết đúng:
```yaml
- method: POST
  path:
    - "{{BaseURL}}/api/login"
  body: |
    {"username":"{{username}}","password":"{{password}}"}
```

---

## 📝 CHANGELOG

### v1.0 (Current)
- Tách riêng 2 scripts: subdomain recon vs deep crawl
- Hỗ trợ session cookies qua jq
- Output tối ưu cho Nuclei (GET/POST riêng biệt)
- Lọc status codes thông minh (giảm rác, giữ 401/403 khi cần)

---

## 🤝 INTEGRATION

### Với auto login script (intruder.py):

```bash
# 1. Login & lấy session
python3 intruder.py -u https://target.com -U admin -w pass.txt
# → Tạo: session_cookies.json

# 2. Crawl authenticated
./deep_crawl.sh https://target.com session_cookies.json
# → Tạo: get_endpoints.txt, post_endpoints.txt

# 3. Scan với Nuclei
nuclei -l deep_crawl/get_endpoints.txt -t templates/
```

### Pipeline hoàn chỉnh:

```bash
#!/bin/bash
DOMAIN="example.com"

# Step 1: Find subdomains
./subdomain_recon.sh "$DOMAIN"

# Step 2: Try auto login on main domain
python3 intruder.py -u "https://$DOMAIN" -U admin -w passwords.txt

# Step 3: Deep crawl with session
./deep_crawl.sh "https://$DOMAIN" session_cookies.json

# Step 4: Nuclei scan
nuclei -l deep_crawl/get_endpoints.txt -severity critical,high,medium -o results.txt
```

---

**Author:** Your Security Team  
**Version:** 1.0  
**Last Updated:** 2024

