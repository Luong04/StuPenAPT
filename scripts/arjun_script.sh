#!/usr/bin/env bash
# =============================================================================
# arjun_scan.sh — Wrapper cho Arjun: quét nhiều URL từ file
# Tác giả: wrapper script cho s0md3v/Arjun
# Mô tả: Nhận file URL đầu vào, quét với các method GET/POST/PUT,
#         xuất ra file JSON tổng hợp và danh sách URL kèm params tìm được.
# =============================================================================
# Cách dùng:
#   chmod +x arjun_scan.sh
#   ./arjun_scan.sh -i urls.txt -o output_dir [options]
#
# Options:
#   -i <file>       File chứa danh sách URL (mỗi dòng 1 URL) [bắt buộc]
#   -o <dir>        Thư mục lưu kết quả (mặc định: ./arjun_results)
#   -m <methods>    Method cần quét, phân tách bằng dấu phẩy (mặc định: GET,POST,JSON)
#   -t <threads>    Số luồng (mặc định: 5)
#   -d <delay>      Delay giữa các request (giây, mặc định: 0)
#   -T <timeout>    Timeout HTTP (giây, mặc định: 15)
#   -w <wordlist>   Đường dẫn wordlist tùy chỉnh
#   -c <chunk>      Số params gửi mỗi request (mặc định: 250)
#   --passive       Kích hoạt thu thập param thụ động (CommonCrawl, WaybackMachine)
#   --stable        Chỉ báo cáo kết quả ổn định, ít false positive hơn
#   -h              Hiển thị trợ giúp
# =============================================================================

set -euo pipefail

# ── Màu sắc terminal ──────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Giá trị mặc định ──────────────────────────────────────────────────────────
INPUT_FILE=""
OUTPUT_DIR="./arjun_results"
METHODS="GET,POST,JSON"
THREADS=5
DELAY=0
TIMEOUT=15
WORDLIST=""
CHUNK=250
PASSIVE=false
STABLE=false

# ── Hàm trợ giúp ─────────────────────────────────────────────────────────────
usage() {
    echo -e "${BOLD}Arjun Multi-URL Scanner${RESET}"
    echo ""
    echo -e "Cách dùng: ${CYAN}$0 -i <url_file> [options]${RESET}"
    echo ""
    echo "Options:"
    echo "  -i <file>       File chứa danh sách URL (mỗi dòng 1 URL) [bắt buộc]"
    echo "  -o <dir>        Thư mục lưu kết quả (mặc định: ./arjun_results)"
    echo "  -m <methods>    Methods: GET,POST,JSON,XML — phân tách bằng dấu phẩy (mặc định: GET,POST,JSON)"
    echo "  -t <threads>    Số luồng (mặc định: 5)"
    echo "  -d <delay>      Delay giữa request (giây, mặc định: 0)"
    echo "  -T <timeout>    Timeout HTTP (giây, mặc định: 15)"
    echo "  -w <wordlist>   Wordlist tùy chỉnh (mặc định: Arjun built-in large.txt)"
    echo "  -c <chunk>      Số params/request (mặc định: 250)"
    echo "  --passive       Thu thập params thụ động từ CommonCrawl/WaybackMachine"
    echo "  --stable        Giảm false positive (chạy chậm hơn)"
    echo "  -h              Hiển thị trợ giúp"
    echo ""
    echo "Ví dụ:"
    echo "  $0 -i urls.txt"
    echo "  $0 -i urls.txt -o results -m GET,POST -t 10"
    echo "  $0 -i urls.txt -m GET,POST,JSON --stable -d 1"
    exit 0
}

log_info()    { echo -e "${BLUE}[*]${RESET} $*"; }
log_ok()      { echo -e "${GREEN}[+]${RESET} $*"; }
log_warn()    { echo -e "${YELLOW}[!]${RESET} $*"; }
log_error()   { echo -e "${RED}[-]${RESET} $*"; }
log_section() { echo -e "\n${BOLD}${CYAN}══════════════════════════════════════════════${RESET}"; echo -e "${BOLD}${CYAN}  $*${RESET}"; echo -e "${BOLD}${CYAN}══════════════════════════════════════════════${RESET}"; }

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -i) INPUT_FILE="$2"; shift 2 ;;
        -o) OUTPUT_DIR="$2"; shift 2 ;;
        -m) METHODS="$2"; shift 2 ;;
        -t) THREADS="$2"; shift 2 ;;
        -d) DELAY="$2"; shift 2 ;;
        -T) TIMEOUT="$2"; shift 2 ;;
        -w) WORDLIST="$2"; shift 2 ;;
        -c) CHUNK="$2"; shift 2 ;;
        --passive) PASSIVE=true; shift ;;
        --stable) STABLE=true; shift ;;
        -h|--help) usage ;;
        *) log_error "Tham số không hợp lệ: $1"; usage ;;
    esac
done

# ── Kiểm tra đầu vào ──────────────────────────────────────────────────────────
if [[ -z "$INPUT_FILE" ]]; then
    log_error "Thiếu file URL đầu vào. Dùng -i <file>"
    usage
fi

if [[ ! -f "$INPUT_FILE" ]]; then
    log_error "File không tồn tại: $INPUT_FILE"
    exit 1
fi

# ── Kiểm tra Arjun đã cài chưa ───────────────────────────────────────────────
if ! command -v arjun &>/dev/null; then
    log_error "Arjun chưa được cài đặt!"
    echo ""
    echo "  Cài đặt bằng pipx (khuyến nghị):"
    echo "    pipx install arjun"
    echo ""
    echo "  Hoặc pip:"
    echo "    pip install arjun"
    exit 1
fi

ARJUN_VERSION=$(arjun --version 2>/dev/null || echo "unknown")
log_ok "Phát hiện Arjun: $ARJUN_VERSION"

# ── Tạo thư mục output ───────────────────────────────────────────────────────
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RUN_DIR="${OUTPUT_DIR}/${TIMESTAMP}"
mkdir -p "$RUN_DIR"

# Đường dẫn các file output
SUMMARY_JSON="${RUN_DIR}/summary.json"
URLS_WITH_PARAMS="${RUN_DIR}/urls_with_params.txt"
ALL_PARAMS_TXT="${RUN_DIR}/all_params.txt"
SCAN_LOG="${RUN_DIR}/scan.log"

# ── Đọc danh sách URL ─────────────────────────────────────────────────────────
mapfile -t URLS < <(grep -v '^\s*$' "$INPUT_FILE" | grep -v '^\s*#' | sed 's/[[:space:]]*$//')
TOTAL_URLS=${#URLS[@]}

if [[ $TOTAL_URLS -eq 0 ]]; then
    log_error "Không có URL nào trong file: $INPUT_FILE"
    exit 1
fi

# ── Chuyển METHODS thành mảng ────────────────────────────────────────────────
IFS=',' read -ra METHOD_LIST <<< "$METHODS"

# ── In thông tin cấu hình ─────────────────────────────────────────────────────
log_section "CẤU HÌNH QUÉT"
echo -e "  File URL    : ${CYAN}$INPUT_FILE${RESET} (${TOTAL_URLS} URLs)"
echo -e "  Methods     : ${CYAN}${METHODS}${RESET}"
echo -e "  Threads     : ${CYAN}${THREADS}${RESET}"
echo -e "  Delay       : ${CYAN}${DELAY}s${RESET}"
echo -e "  Timeout     : ${CYAN}${TIMEOUT}s${RESET}"
echo -e "  Chunk size  : ${CYAN}${CHUNK}${RESET}"
echo -e "  Passive     : ${CYAN}${PASSIVE}${RESET}"
echo -e "  Stable mode : ${CYAN}${STABLE}${RESET}"
echo -e "  Output dir  : ${CYAN}${RUN_DIR}${RESET}"
[[ -n "$WORDLIST" ]] && echo -e "  Wordlist    : ${CYAN}${WORDLIST}${RESET}"
echo ""

# ── Khởi tạo file tổng hợp ───────────────────────────────────────────────────
echo "[]" > "$SUMMARY_JSON"
> "$URLS_WITH_PARAMS"
> "$ALL_PARAMS_TXT"
> "$SCAN_LOG"

# ── Hàm build Arjun command ───────────────────────────────────────────────────
build_arjun_cmd() {
    local url="$1"
    local method="$2"
    local out_json="$3"

    local cmd="arjun -u \"$url\" -m $method -oJ \"$out_json\""
    cmd+=" -t $THREADS"
    cmd+=" -d $DELAY"
    cmd+=" -T $TIMEOUT"
    cmd+=" -c $CHUNK"
    cmd+=" -q"  # quiet — giảm noise, parse JSON để lấy kết quả

    [[ -n "$WORDLIST" ]] && cmd+=" -w \"$WORDLIST\""
    [[ "$PASSIVE" == true ]] && cmd+=" --passive -"
    [[ "$STABLE" == true ]] && cmd+=" --stable"

    echo "$cmd"
}

# ── Hàm merge JSON vào summary ───────────────────────────────────────────────
merge_result() {
    local url="$1"
    local method="$2"
    local json_file="$3"

    if [[ ! -f "$json_file" ]] || [[ ! -s "$json_file" ]]; then
        return
    fi

    # Kiểm tra JSON hợp lệ và có params
    if ! python3 -c "import json,sys; d=json.load(open('$json_file')); sys.exit(0 if d else 1)" 2>/dev/null; then
        return
    fi

    python3 - "$url" "$method" "$json_file" "$SUMMARY_JSON" "$URLS_WITH_PARAMS" "$ALL_PARAMS_TXT" <<'PYEOF'
import json, sys, urllib.parse

url, method, result_file, summary_file, urls_file, params_file = sys.argv[1:]

# Đọc kết quả Arjun
with open(result_file) as f:
    data = json.load(f)

# Arjun JSON output có thể là list hoặc dict tuỳ version
params = []
if isinstance(data, list):
    for item in data:
        if isinstance(item, dict) and 'params' in item:
            params = item['params']
            break
elif isinstance(data, dict):
    params = data.get('params', [])

if not params:
    sys.exit(0)

# ── Tạo URL với params ────────────────────────────────────────────────────────
if method.upper() == 'GET':
    param_str = '&'.join(f"{p}=FUZZ" for p in params)
    full_url = f"{url}{'&' if '?' in url else '?'}{param_str}"
else:
    full_url = url  # POST/JSON params không thêm vào URL

# ── Ghi URLs với params ───────────────────────────────────────────────────────
with open(urls_file, 'a') as f:
    f.write(f"[{method}] {url}\n")
    f.write(f"  Params   : {', '.join(params)}\n")
    if method.upper() == 'GET':
        f.write(f"  Full URL : {full_url}\n")
    f.write("\n")

# ── Ghi params vào all_params.txt ────────────────────────────────────────────
with open(params_file, 'a') as f:
    for p in params:
        f.write(f"{p}\n")

# ── Cập nhật summary.json ─────────────────────────────────────────────────────
with open(summary_file) as f:
    summary = json.load(f)

entry = {
    "url": url,
    "method": method.upper(),
    "params": params,
    "count": len(params),
    "full_url": full_url if method.upper() == 'GET' else None
}
summary.append(entry)

with open(summary_file, 'w') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False)

print(f"    → Tìm thấy {len(params)} params: {', '.join(params)}")
PYEOF
}

# ── Hàm in progress ──────────────────────────────────────────────────────────
print_progress() {
    local current="$1"
    local total="$2"
    local pct=$(( current * 100 / total ))
    local filled=$(( pct / 5 ))
    local bar=""
    for ((i=0; i<20; i++)); do
        [[ $i -lt $filled ]] && bar+="█" || bar+="░"
    done
    echo -ne "\r  [${bar}] ${pct}% (${current}/${total})  "
}

# ══════════════════════════════════════════════════════════════════════════════
# ── VÒNG LẶP CHÍNH ────────────────────────────────────────────────────────────
# ══════════════════════════════════════════════════════════════════════════════
SCAN_COUNT=0
FOUND_COUNT=0

for METHOD in "${METHOD_LIST[@]}"; do
    METHOD=$(echo "$METHOD" | tr '[:lower:]' '[:upper:]' | xargs)

    # Arjun hỗ trợ: GET, POST, JSON, XML — không hỗ trợ PUT natively
    # → Với PUT, dùng POST và note lại
    ARJUN_METHOD="$METHOD"
    if [[ "$METHOD" == "PUT" ]]; then
        log_warn "Arjun không hỗ trợ trực tiếp PUT. Sử dụng POST để phát hiện params, gắn nhãn PUT."
        ARJUN_METHOD="POST"
    fi

    log_section "ĐANG QUÉT METHOD: $METHOD"
    METHOD_DIR="${RUN_DIR}/${METHOD}"
    mkdir -p "$METHOD_DIR"

    URL_IDX=0
    for URL in "${URLS[@]}"; do
        URL_IDX=$((URL_IDX + 1))
        print_progress "$URL_IDX" "$TOTAL_URLS"

        # Tên file an toàn cho output
        SAFE_NAME=$(echo "$URL" | sed 's|https\?://||; s|[^a-zA-Z0-9._-]|_|g' | cut -c1-100)
        OUT_JSON="${METHOD_DIR}/${SAFE_NAME}.json"

        # Build và chạy lệnh Arjun
        CMD=$(build_arjun_cmd "$URL" "$ARJUN_METHOD" "$OUT_JSON")

        echo ">> [$(date +%H:%M:%S)] [$METHOD] $URL" >> "$SCAN_LOG"
        echo "   CMD: $CMD" >> "$SCAN_LOG"

        if eval "$CMD" >> "$SCAN_LOG" 2>&1; then
            merge_result "$URL" "$METHOD" "$OUT_JSON"
        else
            echo "   FAILED" >> "$SCAN_LOG"
        fi

        SCAN_COUNT=$((SCAN_COUNT + 1))
    done

    echo ""  # xuống dòng sau progress bar
done

# ══════════════════════════════════════════════════════════════════════════════
# ── TẠO BÁO CÁO TỔNG HỢP ─────────────────────────────────────────────────────
# ══════════════════════════════════════════════════════════════════════════════
REPORT_FILE="${RUN_DIR}/report.txt"

python3 - "$SUMMARY_JSON" "$REPORT_FILE" "$TOTAL_URLS" "${METHOD_LIST[*]}" "$TIMESTAMP" <<'PYEOF'
import json, sys
from collections import Counter

summary_file, report_file, total_urls, methods_str, timestamp = sys.argv[1:]

with open(summary_file) as f:
    data = json.load(f)

total_found = len(data)
all_params  = [p for entry in data for p in entry['params']]
param_freq  = Counter(all_params)

lines = []
lines.append("=" * 60)
lines.append("  ARJUN SCAN REPORT")
lines.append(f"  Thời gian   : {timestamp}")
lines.append(f"  Tổng URL    : {total_urls}")
lines.append(f"  Methods     : {methods_str}")
lines.append(f"  URL có params: {total_found}")
lines.append(f"  Tổng params  : {len(all_params)} (unique: {len(param_freq)})")
lines.append("=" * 60)
lines.append("")

# Nhóm theo method
from collections import defaultdict
by_method = defaultdict(list)
for entry in data:
    by_method[entry['method']].append(entry)

for method, entries in sorted(by_method.items()):
    lines.append(f"[ {method} ] — {len(entries)} URL có params")
    lines.append("-" * 50)
    for e in entries:
        lines.append(f"  URL    : {e['url']}")
        lines.append(f"  Params : {', '.join(e['params'])}")
        if e.get('full_url'):
            lines.append(f"  Full   : {e['full_url']}")
        lines.append("")

lines.append("=" * 60)
lines.append("  TOP 20 PARAMS PHỔ BIẾN NHẤT")
lines.append("=" * 60)
for param, count in param_freq.most_common(20):
    lines.append(f"  {param:<30} ({count} lần)")

lines.append("")
lines.append("[ Kết thúc báo cáo ]")

report_text = "\n".join(lines)
print(report_text)

with open(report_file, 'w') as f:
    f.write(report_text)
PYEOF

# ── Tóm tắt cuối ──────────────────────────────────────────────────────────────
log_section "KẾT QUẢ QUÉT"

FOUND_URLS=$(python3 -c "import json; d=json.load(open('$SUMMARY_JSON')); print(len(d))" 2>/dev/null || echo 0)
TOTAL_PARAMS=$(python3 -c "import json; d=json.load(open('$SUMMARY_JSON')); print(sum(e['count'] for e in d))" 2>/dev/null || echo 0)

echo -e "  URLs quét   : ${CYAN}${TOTAL_URLS}${RESET}"
echo -e "  URL có params: ${GREEN}${FOUND_URLS}${RESET}"
echo -e "  Tổng params  : ${GREEN}${TOTAL_PARAMS}${RESET}"
echo ""
echo -e "${BOLD}Files output:${RESET}"
echo -e "  ${CYAN}${SUMMARY_JSON}${RESET}         — JSON tổng hợp toàn bộ"
echo -e "  ${CYAN}${URLS_WITH_PARAMS}${RESET}  — URLs kèm params (dễ đọc)"
echo -e "  ${CYAN}${ALL_PARAMS_TXT}${RESET}      — Danh sách tất cả params"
echo -e "  ${CYAN}${REPORT_FILE}${RESET}            — Báo cáo chi tiết"
echo -e "  ${CYAN}${SCAN_LOG}${RESET}             — Log raw từ Arjun"
echo ""
log_ok "Hoàn thành! Kết quả lưu tại: ${RUN_DIR}"