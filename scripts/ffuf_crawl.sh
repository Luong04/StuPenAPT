#!/bin/bash
# ════════════════════════════════════════════════════════════════════════════
# SMART FUZZ - Intelligent Endpoint Discovery with False Positive Elimination
#
# Strategy:
#   1. Detect baseline response (SPA shell, 404 page, etc.)
#   2. Auto-filter responses matching baseline size
#   3. Focus on API endpoints for SPAs (skip useless dir bruteforce)
#   4. Smart wordlist: api-endpoints.txt + Juice Shop patterns + sensitive
#   5. common.txt with recursion for directory discovery
#
# Output structure:
#   baseline/   — baseline_size.txt, filter_sizes.txt, size_frequency.txt
#   api/        — api_endpoints.json, api_juice.json, rest_endpoints.json, rest_juice.json
#   sensitive/  — sensitive_files.json
#   dirs/       — directories.json
#   merged/     — all_unique.txt, api_endpoints.txt
# ════════════════════════════════════════════════════════════════════════════

set -o pipefail

ORIG_ARGC=$#

TARGET="${1:-https://example.com}"
TARGET="${TARGET%/}"  # Strip trailing slash to avoid double-slash in URLs
SESSION_FILE="${2:-session_cookies.json}"
PROXY="${PROXY:-}"
OUTPUT_DIR="./ffuf_crawl_$(date +%Y%m%d_%H%M%S)"
THREADS=30
RATE=100

SCAN_START=$(date +%s)

# Wordlists
BASE="/usr/share/seclists/Discovery/Web-Content"
WL_API="$BASE/api/api-endpoints.txt"    # 283 words
WL_COMMON="$BASE/common.txt"            # 4750 words

# Parse flags
while [[ $# -gt 0 ]]; do
    case $1 in
        --proxy|-p) PROXY="$2"; shift 2 ;;
        *) shift ;;
    esac
done

# ═══════════════════════════════════════════════════════════════════════════
# SESSION EXTRACTION
# ═══════════════════════════════════════════════════════════════════════════
COOKIE_STRING=""
AUTH_HEADER=""

if [ -f "$SESSION_FILE" ]; then
    if jq -e '.sessions' "$SESSION_FILE" &>/dev/null; then
        COOKIE_STRING=$(jq -r '.sessions[0].cookie_string // empty' "$SESSION_FILE" 2>/dev/null)
        AUTH_HEADER=$(jq -r '.sessions[0].headers.Authorization // empty' "$SESSION_FILE" 2>/dev/null)
    else
        COOKIE_STRING=$(jq -r '.cookie_string // empty' "$SESSION_FILE" 2>/dev/null)
        AUTH_HEADER=$(jq -r '.headers.Authorization // empty' "$SESSION_FILE" 2>/dev/null)
    fi
fi

# ─── COLORS ──────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m'

mkdir -p "$OUTPUT_DIR"/{baseline,api,sensitive,dirs,merged}
LOG="$OUTPUT_DIR/run.log"

log()  { echo -e "${CYAN}[*]${NC} $1" | tee -a "$LOG"; }
ok()   { echo -e "${GREEN}[+]${NC} $1" | tee -a "$LOG"; }
warn() { echo -e "${YELLOW}[!]${NC} $1" | tee -a "$LOG"; }
err()  { echo -e "${RED}[-]${NC} $1" | tee -a "$LOG"; }

# Globals
BASELINE_SIZE=""
HOME_SIZE=""
IS_SPA=false
FILTER_SIZES=""

format_time() {
    local S=$1
    if   (( S >= 3600 )); then printf "%dh %dm" $((S/3600)) $(((S%3600)/60))
    elif (( S >= 60 ));   then printf "%dm %ds" $((S/60)) $((S%60))
    else printf "%ds" "$S"; fi
}

# ═══════════════════════════════════════════════════════════════════════════
# STEP 1: BASELINE DETECTION
# Sends 30 random non-existent paths, records sizes, filters common ones
# ═══════════════════════════════════════════════════════════════════════════
detect_baseline() {
    log "━━━ STEP 1: Baseline Detection ━━━"

    local SIZES_FILE="/tmp/baseline_sizes_$$.txt"
    > "$SIZES_FILE"

    gen_rand() { tr -dc 'a-z0-9' < /dev/urandom | head -c "$1" 2>/dev/null; }

    # Dot-prefix paths to detect 403 responses (e.g., .htaccess → 239B)
    local DOT_PATHS=(".env.test" ".htaccess.bak" ".svn" ".git" ".config")
    local DIR_PATHS=("api/" "admin/" "notexist/" "auth/" "user/")

    log "Probing 30 random + 5 dot-prefix + 5 dir paths..."

    local i=0
    while (( i < 40 )); do
        local RPATH
        if (( i >= 30 && i < 35 )); then
            RPATH="/${DOT_PATHS[$((i - 30))]}"
        elif (( i >= 35 )); then
            RPATH="/${DIR_PATHS[$((i - 35))]}"
        else
            local PATH_LEN=$(( 6 + (i % 12) ))  # 6-17 chars
            RPATH="/$(gen_rand $PATH_LEN)"
        fi

        local RESP
        RESP=$(curl -sk --max-time 8 -o /dev/null \
            -w '%{http_code}|%{size_download}' \
            -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36" \
            ${COOKIE_STRING:+-H "Cookie: $COOKIE_STRING"} \
            ${AUTH_HEADER:+-H "Authorization: $AUTH_HEADER"} \
            ${PROXY:+-x "$PROXY"} \
            "${TARGET}${RPATH}" 2>/dev/null) || true

        local SIZE="${RESP##*|}"
        echo "$SIZE" >> "$SIZES_FILE"

        (( i % 10 == 0 )) && echo -ne "\r    Progress: $i/40..."
        (( i++ ))
    done
    echo -e "\r    Progress: 40/40... done"

    # Size frequency analysis
    local SIZE_FREQ
    SIZE_FREQ=$(sort "$SIZES_FILE" | uniq -c | sort -rn)
    echo "$SIZE_FREQ" > "$OUTPUT_DIR/baseline/size_frequency.txt"

    local TOTAL_PROBES=40

    # Primary baseline = most common size
    BASELINE_SIZE=$(echo "$SIZE_FREQ" | head -1 | awk '{print $2}')
    local BASELINE_COUNT=$(echo "$SIZE_FREQ" | head -1 | awk '{print $1}')
    [ -z "$BASELINE_SIZE" ] && BASELINE_SIZE="0"

    ok "Primary baseline: ${BASELINE_SIZE}B (${BASELINE_COUNT}/${TOTAL_PROBES} hits)"

    # Filter sizes that appear 2+ times (lowered threshold to catch 403 patterns)
    local COMMON_SIZES
    COMMON_SIZES=$(echo "$SIZE_FREQ" | awk '$1 >= 2 {print $2}' | tr '\n' ',' | sed 's/,$//')
    FILTER_SIZES="$COMMON_SIZES"

    # Get homepage size and add to filter
    HOME_SIZE=$(curl -sk --max-time 8 -o /dev/null \
        -w '%{size_download}' \
        -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36" \
        ${COOKIE_STRING:+-H "Cookie: $COOKIE_STRING"} \
        ${AUTH_HEADER:+-H "Authorization: $AUTH_HEADER"} \
        ${PROXY:+-x "$PROXY"} \
        "${TARGET}/" 2>/dev/null) || HOME_SIZE="0"

    ok "Homepage size: ${HOME_SIZE}B"

    # Add homepage to filter if not already there
    if [ -n "$FILTER_SIZES" ]; then
        if ! echo ",$FILTER_SIZES," | grep -q ",$HOME_SIZE,"; then
            FILTER_SIZES="${FILTER_SIZES},${HOME_SIZE}"
        fi
    else
        FILTER_SIZES="$HOME_SIZE"
    fi

    # SPA detection (informational only — dir scan still runs with -fs filtering)
    if (( BASELINE_COUNT >= 20 )); then
        IS_SPA=true
        warn "SPA detected! ${BASELINE_COUNT}/${TOTAL_PROBES} paths return ${BASELINE_SIZE}B"
        warn "→ Dir scan will use -fs $FILTER_SIZES to filter SPA shell responses"
    fi

    # Save baseline
    echo "$BASELINE_SIZE" > "$OUTPUT_DIR/baseline/baseline_size.txt"
    echo "$FILTER_SIZES" > "$OUTPUT_DIR/baseline/filter_sizes.txt"

    ok "Filter sizes: $FILTER_SIZES"

    log "Response size distribution:"
    echo "$SIZE_FREQ" | head -5 | while read -r COUNT SIZE; do
        local PCT=$(( COUNT * 100 / TOTAL_PROBES ))
        printf "    %8sB: %2d hits (%2d%%)\n" "$SIZE" "$COUNT" "$PCT"
    done

    rm -f "$SIZES_FILE"
}

# ═══════════════════════════════════════════════════════════════════════════
# STEP 2: API ENDPOINT DISCOVERY
#
# 4 scans:
#   1. api-endpoints.txt on /api/FUZZ
#   2. api-endpoints.txt on /rest/FUZZ
#   3. Juice Shop patterns on /api/FUZZ  (custom wordlist)
#   4. Juice Shop patterns on /rest/FUZZ (custom wordlist)
# ═══════════════════════════════════════════════════════════════════════════
api_discovery() {
    echo ""
    log "━━━ STEP 2: API Endpoint Discovery ━━━"

    # Build common ffuf options
    local FFUF_OPTS="-ac -ic -t $THREADS -rate $RATE -timeout 10"
    [ -n "$FILTER_SIZES" ] && FFUF_OPTS="$FFUF_OPTS -fs $FILTER_SIZES"
    FFUF_OPTS="$FFUF_OPTS -fc 404,429,503"
    FFUF_OPTS="$FFUF_OPTS -mc 200,201,204,301,302,400,401,403,405,500"

    local FFUF_HEADERS=""
    [ -n "$COOKIE_STRING" ] && FFUF_HEADERS="$FFUF_HEADERS -H 'Cookie: $COOKIE_STRING'"
    [ -n "$AUTH_HEADER" ]   && FFUF_HEADERS="$FFUF_HEADERS -H 'Authorization: $AUTH_HEADER'"
    [ -n "$PROXY" ]         && FFUF_HEADERS="$FFUF_HEADERS -x '$PROXY'"

    # ── 2.1: api-endpoints.txt on /api/FUZZ ──────────────────────────────
    log "api-endpoints.txt → /api/FUZZ ($(wc -l < "$WL_API" 2>/dev/null) words)"
    eval ffuf -u "'${TARGET}/api/FUZZ'" -w "'$WL_API'" \
        $FFUF_OPTS \
        -H "'Accept: application/json'" \
        -H "'Content-Type: application/json'" \
        $FFUF_HEADERS \
        -o "'$OUTPUT_DIR/api/api_endpoints.json'" -of json 2>&1

    local H1=$(jq '.results | length' "$OUTPUT_DIR/api/api_endpoints.json" 2>/dev/null || echo 0)
    ok "  → $H1 hits"

    # ── 2.2: api-endpoints.txt on /rest/FUZZ ─────────────────────────────
    log "api-endpoints.txt → /rest/FUZZ"
    eval ffuf -u "'${TARGET}/rest/FUZZ'" -w "'$WL_API'" \
        $FFUF_OPTS \
        -H "'Accept: application/json'" \
        -H "'Content-Type: application/json'" \
        $FFUF_HEADERS \
        -o "'$OUTPUT_DIR/api/rest_endpoints.json'" -of json 2>&1

    local H2=$(jq '.results | length' "$OUTPUT_DIR/api/rest_endpoints.json" 2>/dev/null || echo 0)
    ok "  → $H2 hits"

    # ── 2.3: Juice Shop / common API patterns ────────────────────────────
    # Focused wordlist: Sequelize models, Express routes, common REST patterns
    local JUICE_WL="/tmp/juice_api_$$.txt"
    cat > "$JUICE_WL" << 'EOF'
Products
Users
Feedbacks
Recycles
Complaints
Cards
Deliverys
Challenges
SecurityAnswers
SecurityQuestions
BasketItems
Quantitys
PrivacyRequests
Captchas
ImageCaptchas
Memorys
Memories
Addresss
Wallets
Languages
ChatBot
ContinueCode
country-mapping
user/whoami
user/login
user/change-password
user/reset-password
user/security-question
user/authentication-details
admin/application-configuration
admin/application-version
product/search
products/search
track-order
trackOrder
data-export
delivery
recycle
wallet
chatbot
challenge
saveLoginIp
updateUserProfile
product/reviews
erasure-request
order-history
repeat-notification
deluxe-membership
premium-payment
video
b2b/v2/orders
EOF

    log "Juice Shop patterns → /api/FUZZ"
    eval ffuf -u "'${TARGET}/api/FUZZ'" -w "'$JUICE_WL'" \
        -ac -ic -t 20 -rate 50 -timeout 10 \
        ${FILTER_SIZES:+-fs "$FILTER_SIZES"} \
        -fc 404,429,503 \
        -mc "200,201,204,301,302,400,401,403,405,500" \
        -H "'Accept: application/json'" \
        $FFUF_HEADERS \
        -o "'$OUTPUT_DIR/api/api_juice.json'" -of json -s 2>&1

    local H3=$(jq '.results | length' "$OUTPUT_DIR/api/api_juice.json" 2>/dev/null || echo 0)
    ok "  → $H3 hits"

    # ── 2.4: Juice Shop patterns on /rest/FUZZ ───────────────────────────
    log "Juice Shop patterns → /rest/FUZZ"
    eval ffuf -u "'${TARGET}/rest/FUZZ'" -w "'$JUICE_WL'" \
        -ac -ic -t 20 -rate 50 -timeout 10 \
        ${FILTER_SIZES:+-fs "$FILTER_SIZES"} \
        -fc 404,429,503 \
        -mc "200,201,204,301,302,400,401,403,405,500" \
        -H "'Accept: application/json'" \
        $FFUF_HEADERS \
        -o "'$OUTPUT_DIR/api/rest_juice.json'" -of json -s 2>&1

    local H4=$(jq '.results | length' "$OUTPUT_DIR/api/rest_juice.json" 2>/dev/null || echo 0)
    ok "  → $H4 hits"

    rm -f "$JUICE_WL"
    echo ""
    ok "API Discovery total: $((H1+H2+H3+H4)) hits"
}

# ═══════════════════════════════════════════════════════════════════════════
# STEP 3: SENSITIVE FILE SCAN
# Small curated wordlist, fast (<30 seconds)
# ═══════════════════════════════════════════════════════════════════════════
sensitive_scan() {
    echo ""
    log "━━━ STEP 3: Sensitive File Scan ━━━"

    local SENS_WL="/tmp/sensitive_$$.txt"
    cat > "$SENS_WL" << 'EOF'
.git/HEAD
.git/config
.git/index
.gitignore
.svn/entries
.svn/wc.db
.env
.env.local
.env.development
.env.production
.env.staging
.env.backup
.env.bak
.htaccess
.htpasswd
.well-known/security.txt
security.txt
robots.txt
sitemap.xml
humans.txt
crossdomain.xml
favicon.ico
package.json
composer.json
requirements.txt
Gemfile
web.config
server.xml
config.json
config.yml
appsettings.json
application.properties
application.yml
swagger.json
swagger.yaml
openapi.json
openapi.yaml
api-docs
api-docs.json
graphql
graphiql
Dockerfile
docker-compose.yml
.dockerignore
.gitlab-ci.yml
Jenkinsfile
phpinfo.php
info.php
wp-config.php
wp-login.php
elmah.axd
trace.axd
backup.sql
backup.zip
backup.tar.gz
backup.tar
backup.gz
backup.bak
database.sql
dump.sql
db.sqlite
db.sqlite3
sitemap.gz
tar.gz
archive.zip
archive.tar.gz
site.tar.gz
www.zip
www.tar.gz
backup.7z
backup.rar
source.zip
src.zip
app.zip
dist.zip
deploy.zip
deploy.tar.gz
.DS_Store
Thumbs.db
web.config.bak
.bash_history
.ssh/id_rsa
.ssh/id_rsa.pub
.ssh/authorized_keys
wp-config.php.bak
wp-config.php.old
wp-config.php.save
config.php
config.php.bak
config.bak
settings.py
local_settings.py
.npmrc
.yarnrc
yarn.lock
package-lock.json
composer.lock
Gemfile.lock
.terraform/terraform.tfstate
terraform.tfstate
.env.old
.env.save
.env.swp
.env.dist
error.log
debug.log
access.log
npm-debug.log
yarn-error.log
server.log
application.log
ftp
assets
media
admin
console
dashboard
login
register
promotion
profile
account
upload
uploads
files
download
downloads
backup
backups
tmp
temp
cache
logs
static
public
private
api
v1
v2
README.md
CHANGELOG.md
LICENSE
LICENSE.txt
VERSION
VERSION.txt
BUILDINFO
MANIFEST.MF
INSTALL
INSTALL.txt
EOF

    # Merge with Common-DB-Backups.txt
    local DB_WL="/usr/share/seclists/Discovery/Web-Content/Common-DB-Backups.txt"
    if [ -f "$DB_WL" ]; then
        cat "$DB_WL" >> "$SENS_WL"
        log "Added Common-DB-Backups.txt ($(wc -l < "$DB_WL") entries)"
    fi

    # Deduplicate
    sort -u -o "$SENS_WL" "$SENS_WL"
    log "Total sensitive wordlist: $(wc -l < "$SENS_WL") unique entries"

    eval ffuf -u "'${TARGET}/FUZZ'" -w "'$SENS_WL'" \
        -ic \
        ${FILTER_SIZES:+-fs "$FILTER_SIZES"} \
        -mc "200" \
        -t 30 -rate 100 -timeout 10 \
        ${COOKIE_STRING:+-H "'Cookie: $COOKIE_STRING'"} \
        ${AUTH_HEADER:+-H "'Authorization: $AUTH_HEADER'"} \
        ${PROXY:+-x "'$PROXY'"} \
        -o "'$OUTPUT_DIR/sensitive/sensitive_files.json'" -of json 2>&1

    local HITS=$(jq '.results | length' "$OUTPUT_DIR/sensitive/sensitive_files.json" 2>/dev/null || echo 0)
    ok "→ $HITS sensitive files found"

    rm -f "$SENS_WL"
}

# ═══════════════════════════════════════════════════════════════════════════
# STEP 4: DIRECTORY DISCOVERY
# common.txt (4750 words) with recursion depth 2
# Always runs — relies on -fs filtering to eliminate SPA false positives
# ═══════════════════════════════════════════════════════════════════════════
dir_discovery() {
    echo ""

    log "━━━ STEP 4: Directory Discovery (common.txt + recursion depth 2) ━━━"
    log "  $(wc -l < "$WL_COMMON" 2>/dev/null) words, recursion depth 2"

    eval ffuf -u "'${TARGET}/FUZZ'" -w "'$WL_COMMON'" \
        -ac \
        ${FILTER_SIZES:+-fs "$FILTER_SIZES"} \
        -fc 404,429,503 \
        -mc "200,201,204,301,302,400,401,403,405" \
        -t $THREADS -rate $RATE -timeout 10 \
        -recursion -recursion-depth 2 \
        ${COOKIE_STRING:+-H "'Cookie: $COOKIE_STRING'"} \
        ${AUTH_HEADER:+-H "'Authorization: $AUTH_HEADER'"} \
        ${PROXY:+-x "'$PROXY'"} \
        -o "'$OUTPUT_DIR/dirs/directories.json'" -of json 2>&1

    local HITS=$(jq '.results | length' "$OUTPUT_DIR/dirs/directories.json" 2>/dev/null || echo 0)
    ok "→ $HITS directories/files found"
}

# ═══════════════════════════════════════════════════════════════════════════
# STEP 5: MERGE & REPORT
# ═══════════════════════════════════════════════════════════════════════════
merge_results() {
    echo ""
    log "━━━ STEP 5: Merge & Deduplicate ━━━"

    local MERGED="$OUTPUT_DIR/merged/all_unique.txt"

    # Collect all results
    cat "$OUTPUT_DIR"/{api,sensitive,dirs}/*.json 2>/dev/null | \
        jq -r '.results[] | "\(.status) \(.length)B \(.url)"' 2>/dev/null | \
        sort -u > "$MERGED"

    # API endpoints list
    cat "$OUTPUT_DIR"/api/*.json 2>/dev/null | \
        jq -r '.results[] | .url' 2>/dev/null | \
        sort -u > "$OUTPUT_DIR/merged/api_endpoints.txt"

    # Counts
    local TOTAL T200 TREDIR TAUTH T500 TAPI
    TOTAL=$(wc -l < "$MERGED" 2>/dev/null || echo 0)
    T200=$(grep -cE '^20[0-9] ' "$MERGED" 2>/dev/null || echo 0)
    TREDIR=$(grep -cE '^(301|302|307) ' "$MERGED" 2>/dev/null || echo 0)
    TAUTH=$(grep -cE '^(401|403) ' "$MERGED" 2>/dev/null || echo 0)
    T500=$(grep -cE '^500 ' "$MERGED" 2>/dev/null || echo 0)
    TAPI=$(wc -l < "$OUTPUT_DIR/merged/api_endpoints.txt" 2>/dev/null || echo 0)

    local ELAPSED=$(( $(date +%s) - SCAN_START ))

    echo ""
    echo -e "${BOLD}┌──────────────────────────────────────────────────────┐${NC}"
    echo -e "${BOLD}│  SMART FUZZ COMPLETE                                 │${NC}"
    echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
    printf  "${BOLD}│${NC}  %-20s %-31s${BOLD}│${NC}\n" "Target" "$TARGET"
    printf  "${BOLD}│${NC}  %-20s %-31s${BOLD}│${NC}\n" "Type" "$([ "$IS_SPA" = true ] && echo 'SPA' || echo 'Traditional')"
    printf  "${BOLD}│${NC}  %-20s %-31s${BOLD}│${NC}\n" "Filter" "-fs $FILTER_SIZES"
    printf  "${BOLD}│${NC}  %-20s ${YELLOW}%-31s${NC}${BOLD}│${NC}\n" "Duration" "$(format_time $ELAPSED)"
    echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
    printf  "${BOLD}│${NC}  %-30s ${GREEN}%-19s${NC} ${BOLD}│${NC}\n" "Total unique" "$TOTAL"
    printf  "${BOLD}│${NC}  %-30s ${GREEN}%-19s${NC} ${BOLD}│${NC}\n" "200 OK" "$T200"
    printf  "${BOLD}│${NC}  %-30s %-19s ${BOLD}│${NC}\n" "Redirects (301/302)" "$TREDIR"
    printf  "${BOLD}│${NC}  %-30s ${YELLOW}%-19s${NC} ${BOLD}│${NC}\n" "Auth needed (401/403)" "$TAUTH"
    printf  "${BOLD}│${NC}  %-30s %-19s ${BOLD}│${NC}\n" "500 Errors" "$T500"
    printf  "${BOLD}│${NC}  %-30s ${CYAN}%-19s${NC} ${BOLD}│${NC}\n" "API endpoints" "$TAPI"
    echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
    printf  "${BOLD}│${NC}  Output: ${CYAN}%-41s${NC}${BOLD}│${NC}\n" "$OUTPUT_DIR"
    echo -e "${BOLD}└──────────────────────────────────────────────────────┘${NC}"

    if [ -s "$OUTPUT_DIR/merged/api_endpoints.txt" ]; then
        echo ""
        ok "API Endpoints:"
        cat "$OUTPUT_DIR/merged/api_endpoints.txt" | sed 's/^/    /'
    fi

    if (( T500 > 0 )); then
        echo ""
        warn "500 Errors:"
        grep -E '^500 ' "$MERGED" | sed 's/^/    /'
    fi
}

# ═══════════════════════════════════════════════════════════════════════════
# CLEANUP
# ═══════════════════════════════════════════════════════════════════════════
cleanup() {
    echo ""
    warn "Interrupted (Ctrl+C)"
    pkill -f ffuf 2>/dev/null || true
    rm -f /tmp/juice_api_$$.txt /tmp/sensitive_$$.txt /tmp/baseline_sizes_$$.txt 2>/dev/null
    err "Partial results in: $OUTPUT_DIR"
    exit 130
}
trap cleanup SIGINT SIGTERM

# ═══════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════
main() {
    echo -e "${BOLD}${CYAN}"
    echo "  ┌─────────────────────────────────────────────────────┐"
    echo "  │     SMART FUZZ — Endpoint Discovery                 │"
    echo "  │     Baseline filter + API focus + Sensitive scan    │"
    echo "  └─────────────────────────────────────────────────────┘"
    echo -e "${NC}"

    command -v ffuf &>/dev/null || { err "ffuf not found!"; exit 1; }
    command -v jq   &>/dev/null || { err "jq not found!"; exit 1; }
    command -v curl &>/dev/null || { err "curl not found!"; exit 1; }

    log "Target  : $TARGET"
    log "Threads : $THREADS | Rate: ${RATE}/s"
    [ -n "$COOKIE_STRING" ] && log "Cookie  : SET" || log "Cookie  : none"
    [ -n "$AUTH_HEADER" ]   && log "Auth    : SET"
    [ -n "$PROXY" ]         && log "Proxy   : $PROXY"
    echo ""

    detect_baseline
    api_discovery
    sensitive_scan
    dir_discovery
    merge_results

    rm -f /tmp/baseline_sizes_$$.txt 2>/dev/null
}

# ═══════════════════════════════════════════════════════════════════════════
# USAGE
# ═══════════════════════════════════════════════════════════════════════════
if (( ORIG_ARGC == 0 )); then
    echo -e "${BOLD}SMART FUZZ${NC} — Endpoint discovery with false positive elimination"
    echo ""
    echo "Usage: $0 <TARGET_URL> [SESSION_FILE] [--proxy URL]"
    echo ""
    echo "Examples:"
    echo "  $0 http://target.com"
    echo "  $0 http://target.com session_cookies.json"
    echo "  $0 http://target.com session_cookies.json --proxy http://127.0.0.1:8080"
    echo ""
    echo "Scans:"
    echo "  1. Baseline   — 30 random paths → detect FP sizes"
    echo "  2. API        — api-endpoints.txt on /api/ + /rest/ + Juice Shop patterns"
    echo "  3. Sensitive  — curated sensitive files (~70 entries)"
    echo "  4. Dirs       — common.txt (4750) with recursion depth 2"
    echo ""
    echo "Output: ffuf_crawl_YYYYMMDD_HHMMSS/{api,sensitive,dirs,baseline,merged}/"
    exit 0
fi

main
