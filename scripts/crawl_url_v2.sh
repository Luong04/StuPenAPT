#!/bin/bash
# ==============================================================================
#  CRAWL URL v2 — Deep URL Discovery (Katana + GAU + ffuf_crawl)
#
#  3 complementary tools:
#    1. Katana   — JS-aware web crawler (endpoints in client-side code)
#    2. GAU      — Archived URLs from Wayback Machine / Common Crawl / OTX
#    3. ffuf     — Smart endpoint fuzzing via ffuf_crawl.sh (baseline filter)
#
#  Output: crawl_YYYYMMDD_HHMMSS/
#    summary/endpoints_summary.txt  ← merged URLs with params, sorted, deduped (for arjun/gf/nuclei)
#    summary/endpoints_detail.txt   ← merged detail: METHOD STATUS SIZEB URL [BODY: ...]
#    katana/endpoints.txt           ← plain URLs with params (tool file)
#    katana/endpoints_detail.txt    ← METHOD STATUS SIZEB URL [BODY: ...]
#    gau/endpoints.txt              ← plain URLs with params (tool file)
#    gau/endpoints_detail.txt       ← STATUS SIZEB URL
#    ffuf/endpoints.txt             ← plain URLs (tool file)
#    ffuf/endpoints_detail.txt      ← STATUS SIZEB URL
#
#  Usage: ./crawl_url_v2.sh <URL> <session_cookies.json> [--proxy URL]
# ==============================================================================

set -o pipefail

# ═══════════════════════════════════════════════════════════════════════════
# ARGUMENT PARSING
# ═══════════════════════════════════════════════════════════════════════════
TARGET=""
SESSION_FILE=""
PROXY=""
ARG_POS=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --proxy|-p) PROXY="$2"; shift 2 ;;
        -*)         shift ;;
        *)
            if (( ARG_POS == 0 )); then TARGET="$1"
            elif (( ARG_POS == 1 )); then SESSION_FILE="$1"
            fi
            ((ARG_POS++))
            shift
            ;;
    esac
done

TARGET="${TARGET%/}"

if [ -z "$TARGET" ] || [ -z "$SESSION_FILE" ]; then
    echo -e "\033[1mCRAWL URL v2\033[0m — Deep URL Discovery (Katana + GAU + ffuf)"
    echo ""
    echo "Usage: $0 <URL> <session_cookies.json> [--proxy URL]"
    echo ""
    echo "Examples:"
    echo "  $0 http://target.com session_cookies.json"
    echo "  $0 http://target.com session_cookies.json --proxy http://127.0.0.1:8080"
    echo ""
    echo "Output: crawl_YYYYMMDD_HHMMSS/{katana,gau,ffuf}/"
    echo "  endpoints.txt        — plain URLs for tools (arjun, gf, nuclei)"
    echo "  endpoints_detail.txt — human-readable with status + size"
    exit 0
fi

# ═══════════════════════════════════════════════════════════════════════════
# SETUP
# ═══════════════════════════════════════════════════════════════════════════
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; MAGENTA='\033[0;35m'
BOLD='\033[1m'; NC='\033[0m'

SCAN_START=$(date +%s)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="./crawl_$(date +%Y%m%d_%H%M%S)"
TEMP_DIR="/tmp/crawl_v2_$$"

mkdir -p "$OUTPUT_DIR"/{katana,gau,ffuf,summary}
mkdir -p "$TEMP_DIR"

format_time() {
    local S=$1
    if   (( S >= 3600 )); then printf "%dh %dm" $((S/3600)) $(((S%3600)/60))
    elif (( S >= 60 ));   then printf "%dm %ds" $((S/60)) $((S%60))
    else printf "%ds" "$S"; fi
}

# Static file filter patterns (reused across tools)
STATIC_FILTER='\.(png|jpg|jpeg|gif|bmp|webp|svg|ico|cur|css|scss|sass|less|woff|woff2|ttf|eot|otf|mp4|mp3|avi|mov|wmv|flv|webm|mkv|wav|ogg|m4a)(\?|$|#)'
JS_FILTER='^[^?]*\.(js|map)(\?|$|#)'
DIR_FILTER='/(fonts?|images?|img|pics?|pictures?|media|assets?|static|cdn|cdn-cgi)(/|$)'

# ═══════════════════════════════════════════════════════════════════════════
# CLEANUP HANDLER
# ═══════════════════════════════════════════════════════════════════════════
cleanup() {
    echo ""
    echo -e "${YELLOW}[!] Interrupted (Ctrl+C)${NC}"
    pkill -P $$ 2>/dev/null || true
    rm -rf "$TEMP_DIR" 2>/dev/null
    echo -e "${RED}[!] Partial results in: $OUTPUT_DIR${NC}"
    exit 130
}
trap cleanup SIGINT SIGTERM

# ═══════════════════════════════════════════════════════════════════════════
# CHECK TOOLS
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${MAGENTA}[*] Checking tools...${NC}"
MISSING=()
for tool in katana gau httpx ffuf jq curl uro whatweb; do
    command -v "$tool" &>/dev/null || MISSING+=("$tool")
done
if (( ${#MISSING[@]} > 0 )); then
    echo -e "${RED}[!] Missing tools: ${MISSING[*]}${NC}"
    exit 1
fi
echo -e "${GREEN}[✓] All tools ready${NC}"

# ═══════════════════════════════════════════════════════════════════════════
# SESSION EXTRACTION
# ═══════════════════════════════════════════════════════════════════════════
COOKIE_STRING=""
AUTH_HEADER=""
USER_AGENT="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"

if [ -f "$SESSION_FILE" ]; then
    echo -e "${CYAN}[*] Loading session: ${SESSION_FILE}${NC}"
    if jq -e '.sessions' "$SESSION_FILE" &>/dev/null; then
        COOKIE_STRING=$(jq -r '.sessions[0].cookie_string // empty' "$SESSION_FILE" 2>/dev/null)
        AUTH_HEADER=$(jq -r '.sessions[0].headers.Authorization // empty' "$SESSION_FILE" 2>/dev/null)
        SESSION_UA=$(jq -r '.sessions[0].headers["User-Agent"] // empty' "$SESSION_FILE" 2>/dev/null)
        [ -n "$SESSION_UA" ] && [ "$SESSION_UA" != "null" ] && USER_AGENT="$SESSION_UA"
    else
        COOKIE_STRING=$(jq -r '.cookie_string // empty' "$SESSION_FILE" 2>/dev/null)
        AUTH_HEADER=$(jq -r '.headers.Authorization // empty' "$SESSION_FILE" 2>/dev/null)
        SESSION_UA=$(jq -r '.headers["User-Agent"] // empty' "$SESSION_FILE" 2>/dev/null)
        [ -n "$SESSION_UA" ] && [ "$SESSION_UA" != "null" ] && USER_AGENT="$SESSION_UA"
    fi
    # Validate extracted values
    [ "$COOKIE_STRING" = "null" ] || [ -z "$COOKIE_STRING" ] && COOKIE_STRING=""
    [ "$AUTH_HEADER" = "null" ] || [ -z "$AUTH_HEADER" ] && AUTH_HEADER=""
    [ -n "$COOKIE_STRING" ] && echo -e "${GREEN}    ✓ Cookies loaded${NC}"
    [ -n "$AUTH_HEADER" ]   && echo -e "${GREEN}    ✓ Authorization loaded${NC}"
else
    echo -e "${YELLOW}[*] No session file — crawling without auth${NC}"
fi

# Build common httpx args array (avoids quoting hell in pipes)
HTTPX_BASE_ARGS=(-H "User-Agent: $USER_AGENT" -timeout 10 -retries 0 -silent -json)
[ -n "$COOKIE_STRING" ] && HTTPX_BASE_ARGS+=(-H "Cookie: $COOKIE_STRING")
[ -n "$AUTH_HEADER" ]   && HTTPX_BASE_ARGS+=(-H "Authorization: $AUTH_HEADER")
[ -n "$PROXY" ]         && HTTPX_BASE_ARGS+=(-proxy "$PROXY")

# ═══════════════════════════════════════════════════════════════════════════
# BANNER
# ═══════════════════════════════════════════════════════════════════════════
echo ""
echo -e "${BOLD}${CYAN}┌─────────────────────────────────────────────────────┐${NC}"
echo -e "${BOLD}${CYAN}│  CRAWL URL v2 — Deep URL Discovery                 │${NC}"
echo -e "${BOLD}${CYAN}│  Katana + GAU + ffuf_crawl                         │${NC}"
echo -e "${BOLD}${CYAN}└─────────────────────────────────────────────────────┘${NC}"
echo -e "  Target  : ${TARGET}"
echo -e "  Session : ${SESSION_FILE}"
echo -e "  Output  : ${OUTPUT_DIR}"
[ -n "$PROXY" ] && echo -e "  Proxy   : ${PROXY}"
echo ""

# ═══════════════════════════════════════════════════════════════════════════
# STEP 1: FRAMEWORK DETECTION (WhatWeb + HTML fallback)
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}[1/5] Detecting web framework...${NC}"

IS_MODERN=0
WHATWEB_OUTPUT=$(timeout 10 whatweb -a 3 "$TARGET" 2>/dev/null || echo "")

if [ -n "$WHATWEB_OUTPUT" ]; then
    if echo "$WHATWEB_OUTPUT" | grep -qiE "(Angular|React|Vue|Next\.?js|Nuxt|Svelte|Backbone|Ember)"; then
        IS_MODERN=1
        FRAMEWORK=$(echo "$WHATWEB_OUTPUT" | grep -oiE "(Angular|React|Vue|Next\.?js|Nuxt|Svelte|Backbone|Ember)" | head -1)
        echo -e "${GREEN}    ✓ Modern framework: ${FRAMEWORK}${NC}"
    elif echo "$WHATWEB_OUTPUT" | grep -qE "(Script\[module\]|__NEXT|__nuxt|app\.bundle|main\.[a-f0-9])"; then
        IS_MODERN=1
        echo -e "${GREEN}    ✓ Modern SPA detected${NC}"
    fi
fi

if [ "$IS_MODERN" -eq 0 ]; then
    FRAMEWORK=$(curl -s -m 10 "$TARGET" 2>/dev/null | \
        grep -oiE "(app-root|<div id=['\"]root|<div id=['\"]app|ng-app|ng-version|__NEXT|__nuxt)" | head -1 || echo "")
    if [ -n "$FRAMEWORK" ]; then
        IS_MODERN=1
        echo -e "${GREEN}    ✓ SPA detected: ${FRAMEWORK}${NC}"
    else
        echo -e "${YELLOW}    → Traditional/Static web${NC}"
    fi
fi

# ═══════════════════════════════════════════════════════════════════════════
# STEP 2: KATANA CRAWL
#   Pass 1: GET endpoints with qurl format (preserves params)
#   Pass 2: POST endpoints with form extraction (captures body)
#   Detail: METHOD STATUS SIZEB URL [BODY: ...]
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}[2/5] Katana crawler...${NC}"

# Common katana base args
KATANA_BASE=(-u "$TARGET" -H "User-Agent: $USER_AGENT"
    -jc -kf all -timeout 20 -retry 2)

[ -n "$COOKIE_STRING" ] && KATANA_BASE+=(-H "Cookie: $COOKIE_STRING")
[ -n "$AUTH_HEADER" ]   && KATANA_BASE+=(-H "Authorization: $AUTH_HEADER")
[ -n "$PROXY" ]         && KATANA_BASE+=(-proxy "$PROXY")

# ── Pass 1: GET endpoints (qurl format, preserves params) ────────────────
KATANA_GET_ARGS=("${KATANA_BASE[@]}" -f qurl -silent)

if [ "$IS_MODERN" -eq 1 ]; then
    KATANA_GET_ARGS+=(-hl -nos -d 3)
    echo -e "${CYAN}    → SPA mode: headless + depth 3${NC}"
else
    KATANA_GET_ARGS+=(-d 4 -p 20 -c 20)
    echo -e "${CYAN}    → Static mode: depth 4, parallel 20${NC}"
fi

katana "${KATANA_GET_ARGS[@]}" > "$TEMP_DIR/katana_get_raw.txt" 2>/dev/null

COUNT_RAW_GET=$(wc -l < "$TEMP_DIR/katana_get_raw.txt" 2>/dev/null || echo 0)
echo -e "${CYAN}    → GET raw: ${COUNT_RAW_GET} URLs${NC}"

# ── Pass 2: POST endpoints (JSON format with form extraction) ────────────
KATANA_POST_ARGS=("${KATANA_BASE[@]}" -form-extraction -d 3 -f json -silent)

katana "${KATANA_POST_ARGS[@]}" > "$TEMP_DIR/katana_post_raw.jsonl" 2>/dev/null

# Extract POST requests with body
jq -r 'select(.request.method == "POST") |
    .request.endpoint + " [BODY: " + (.request.body // "empty") + "]"' \
    "$TEMP_DIR/katana_post_raw.jsonl" 2>/dev/null | \
    sort -u > "$TEMP_DIR/katana_post_parsed.txt"

COUNT_RAW_POST=$(wc -l < "$TEMP_DIR/katana_post_parsed.txt" 2>/dev/null || echo 0)
echo -e "${CYAN}    → POST raw: ${COUNT_RAW_POST} forms${NC}"

# --- Tool file: GET URLs filtered + POST URLs (plain URL only) ---
# GET: filter static, dedup with uro (keeps params)
cat "$TEMP_DIR/katana_get_raw.txt" 2>/dev/null | \
    sort -u | \
    grep -viE "$STATIC_FILTER" | \
    grep -viE "$JS_FILTER" | \
    grep -viE "$DIR_FILTER" | \
    grep -viE "(minified|\.min\.)" | \
    uro --filters keepcontent,keepslash \
    > "$TEMP_DIR/katana_get_filtered.txt" 2>/dev/null

# POST: extract just the URL part (before " [BODY:")
awk -F' \\[BODY:' '{print $1}' "$TEMP_DIR/katana_post_parsed.txt" 2>/dev/null | \
    sort -u > "$TEMP_DIR/katana_post_urls.txt"

# Merge GET + POST URLs → tool file
cat "$TEMP_DIR/katana_get_filtered.txt" "$TEMP_DIR/katana_post_urls.txt" 2>/dev/null | \
    sort -u > "$OUTPUT_DIR/katana/endpoints.txt"

# --- Human detail: METHOD STATUS SIZEB URL [BODY: ...] ---
# GET: probe with httpx for status + size
> "$OUTPUT_DIR/katana/endpoints_detail.txt"

if [ -s "$TEMP_DIR/katana_get_filtered.txt" ]; then
    cat "$TEMP_DIR/katana_get_filtered.txt" | \
        httpx "${HTTPX_BASE_ARGS[@]}" \
        > "$TEMP_DIR/katana_get_probe.jsonl" 2>/dev/null

    jq -r '"GET \(.status_code // "-") \(.content_length // 0)B \(.url // "-")"' \
        "$TEMP_DIR/katana_get_probe.jsonl" 2>/dev/null | \
        sort -u >> "$OUTPUT_DIR/katana/endpoints_detail.txt"
fi

# POST: add with body info (no live probe — POST needs specific body)
if [ -s "$TEMP_DIR/katana_post_parsed.txt" ]; then
    while IFS= read -r line; do
        echo "POST - - $line"
    done < "$TEMP_DIR/katana_post_parsed.txt" >> "$OUTPUT_DIR/katana/endpoints_detail.txt"
fi

COUNT_KATANA=$(wc -l < "$OUTPUT_DIR/katana/endpoints.txt" 2>/dev/null || echo 0)
COUNT_KATANA_GET=$(wc -l < "$TEMP_DIR/katana_get_filtered.txt" 2>/dev/null || echo 0)
COUNT_KATANA_POST=$(wc -l < "$TEMP_DIR/katana_post_urls.txt" 2>/dev/null || echo 0)
echo -e "${GREEN}    ✓ ${COUNT_KATANA} endpoints (GET: ${COUNT_KATANA_GET} | POST: ${COUNT_KATANA_POST})${NC}"

# ═══════════════════════════════════════════════════════════════════════════
# STEP 3: GAU (Archived URLs)
#   - Fetch from Wayback/CommonCrawl/OTX
#   - Filter with httpx (only live URLs with matching status)
#   - Soft-404 filter: if >=3 URLs share same (status, size), keep only 1
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}[3/5] GAU (archived URLs)...${NC}"

DOMAIN=$(echo "$TARGET" | awk -F/ '{print $3}')

echo "$DOMAIN" | gau --threads 5 \
    --blacklist ttf,woff,woff2,eot,otf,svg,png,jpg,jpeg,gif,ico,bmp,webp,css,scss,sass,mp4,mp3,avi,mov,wmv,flv,webm,mkv,wav,ogg \
    2>/dev/null | \
    httpx "${HTTPX_BASE_ARGS[@]}" -mc 200,201,401,403 \
    > "$TEMP_DIR/gau_raw.jsonl" 2>/dev/null

# ── Soft-404 filter: group by (status_code, content_length) ──────────────
#    If a (status, size) combo appears >=3 times → likely catch-all/soft-404
#    Keep only 1 representative URL per such group
COUNT_GAU_RAW=$(wc -l < "$TEMP_DIR/gau_raw.jsonl" 2>/dev/null || echo 0)

# Build frequency map: "status|size" → count
jq -r '"\(.status_code // "-")|\(.content_length // 0)"' \
    "$TEMP_DIR/gau_raw.jsonl" 2>/dev/null | \
    sort | uniq -c | sort -rn > "$TEMP_DIR/gau_freq.txt"

# Extract keys that appear >=3 times (soft-404 signatures)
SOFT404_KEYS=$(awk '$1 >= 3 {print $2}' "$TEMP_DIR/gau_freq.txt")

# Save soft-404 keys to file
echo "$SOFT404_KEYS" | grep -v '^$' > "$TEMP_DIR/gau_soft404_keys.txt" 2>/dev/null

if [ -s "$TEMP_DIR/gau_soft404_keys.txt" ]; then
    echo -e "${CYAN}    → Soft-404 signatures detected:${NC}"

    # Print detected signatures
    while IFS= read -r sig_key; do
        SIG_STATUS=$(echo "$sig_key" | cut -d'|' -f1)
        SIG_SIZE=$(echo "$sig_key" | cut -d'|' -f2)
        SIG_COUNT=$(awk -v k="$sig_key" '$2 == k {print $1}' "$TEMP_DIR/gau_freq.txt")
        echo -e "${YELLOW}      ${SIG_STATUS} ${SIG_SIZE}B × ${SIG_COUNT} URLs → kept 1${NC}"
    done < "$TEMP_DIR/gau_soft404_keys.txt"

    # Build per-line key file (matches gau_raw.jsonl line-by-line)
    jq -r '"\(.status_code // "-")|\(.content_length // 0)"' \
        "$TEMP_DIR/gau_raw.jsonl" > "$TEMP_DIR/gau_line_keys.txt" 2>/dev/null

    # Filter: paste keys alongside original JSON, use awk to keep only 1 per soft-404 group
    paste "$TEMP_DIR/gau_line_keys.txt" "$TEMP_DIR/gau_raw.jsonl" | \
    awk -F'\t' '
    BEGIN {
        while ((getline k < "'"$TEMP_DIR/gau_soft404_keys.txt"'") > 0)
            soft404[k] = 1
    }
    {
        key = $1
        if (key in soft404) {
            if (!(key in seen)) {
                seen[key] = 1
                sub(/^[^\t]*\t/, "")
                print
            }
        } else {
            sub(/^[^\t]*\t/, "")
            print
        }
    }' > "$TEMP_DIR/gau.jsonl"

    COUNT_GAU_FILTERED=$(wc -l < "$TEMP_DIR/gau.jsonl" 2>/dev/null || echo 0)
    REMOVED=$((COUNT_GAU_RAW - COUNT_GAU_FILTERED))
    echo -e "${CYAN}    → Removed ${REMOVED} soft-404 duplicates (${COUNT_GAU_RAW} → ${COUNT_GAU_FILTERED})${NC}"
else
    cp "$TEMP_DIR/gau_raw.jsonl" "$TEMP_DIR/gau.jsonl"
    echo -e "${CYAN}    → No soft-404 patterns detected${NC}"
fi

# --- Tool file: plain URLs, filtered (static + js + dir), deduped ---
jq -r '.url // empty' "$TEMP_DIR/gau.jsonl" 2>/dev/null | \
    sort -u | \
    grep -viE "$STATIC_FILTER" | \
    grep -viE "$JS_FILTER" | \
    grep -viE "$DIR_FILTER" | \
    grep -viE "(minified|\.min\.)" | \
    uro --filters keepcontent,keepslash \
    > "$OUTPUT_DIR/gau/endpoints.txt"

# --- Human file: STATUS SIZEB URL (static-filtered) ---
jq -r '"\(.status_code // "-") \(.content_length // 0)B \(.url // "-")"' \
    "$TEMP_DIR/gau.jsonl" 2>/dev/null | \
    grep -viE "$STATIC_FILTER" | \
    grep -viE "$JS_FILTER" | \
    grep -viE "$DIR_FILTER" | \
    sort -u > "$OUTPUT_DIR/gau/endpoints_detail.txt"

COUNT_GAU=$(wc -l < "$OUTPUT_DIR/gau/endpoints.txt" 2>/dev/null || echo 0)
echo -e "${GREEN}    ✓ ${COUNT_GAU} endpoints${NC}"

# ═══════════════════════════════════════════════════════════════════════════
# STEP 4: FFUF (via ffuf_crawl.sh)
#   - Baseline detection + smart filtering
#   - API, sensitive, directory discovery
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}[4/5] ffuf smart fuzzing (ffuf_crawl.sh)...${NC}"

FFUF_SCRIPT="$SCRIPT_DIR/ffuf_crawl.sh"

if [ ! -f "$FFUF_SCRIPT" ]; then
    echo -e "${RED}    [!] ffuf_crawl.sh not found at: $FFUF_SCRIPT${NC}"
    echo -e "${YELLOW}    Skipping ffuf scan${NC}"
    touch "$OUTPUT_DIR/ffuf/endpoints.txt" "$OUTPUT_DIR/ffuf/endpoints_detail.txt"
else
    # Record existing ffuf_crawl dirs to detect the new one
    EXISTING_DIRS=$(ls -d ffuf_crawl_* 2>/dev/null | sort)

    # Run ffuf_crawl.sh
    FFUF_CMD=(bash "$FFUF_SCRIPT" "$TARGET" "$SESSION_FILE")
    [ -n "$PROXY" ] && FFUF_CMD+=(--proxy "$PROXY")

    echo -e "${CYAN}    → Running: ffuf_crawl.sh${NC}"
    "${FFUF_CMD[@]}"

    # Find the newly created output directory
    NEW_DIRS=$(ls -d ffuf_crawl_* 2>/dev/null | sort)
    FFUF_OUTPUT=$(comm -13 <(echo "$EXISTING_DIRS") <(echo "$NEW_DIRS") | tail -1)

    # Fallback: most recent ffuf_crawl dir
    [ -z "$FFUF_OUTPUT" ] && FFUF_OUTPUT=$(ls -td ffuf_crawl_* 2>/dev/null | head -1)

    if [ -n "$FFUF_OUTPUT" ] && [ -d "$FFUF_OUTPUT" ]; then
        if [ -f "$FFUF_OUTPUT/merged/all_unique.txt" ]; then
            # Human file: copy as-is (format: "STATUS SIZEB URL")
            cp "$FFUF_OUTPUT/merged/all_unique.txt" "$OUTPUT_DIR/ffuf/endpoints_detail.txt"

            # Tool file: extract just URLs (3rd field)
            awk '{print $3}' "$FFUF_OUTPUT/merged/all_unique.txt" | sort -u \
                > "$OUTPUT_DIR/ffuf/endpoints.txt"
        fi

        # Remove intermediate ffuf_crawl output
        rm -rf "$FFUF_OUTPUT"
    fi
fi

# Ensure files exist even if ffuf found nothing
touch "$OUTPUT_DIR/ffuf/endpoints.txt" "$OUTPUT_DIR/ffuf/endpoints_detail.txt"

COUNT_FFUF=$(wc -l < "$OUTPUT_DIR/ffuf/endpoints.txt" 2>/dev/null || echo 0)
echo -e "${GREEN}    ✓ ${COUNT_FFUF} endpoints${NC}"

# ═══════════════════════════════════════════════════════════════════════════
# STEP 5: MERGE → summary/
#   endpoints_summary.txt  — all URLs with params, sorted, deduped
#   endpoints_detail.txt   — merged detail with METHOD info (keeps POST+body)
# ═══════════════════════════════════════════════════════════════════════════
echo -e "${BLUE}[5/5] Generating summary/...${NC}"

# --- Tool file: merge all endpoints.txt, sort + dedup ---
cat "$OUTPUT_DIR"/katana/endpoints.txt \
    "$OUTPUT_DIR"/gau/endpoints.txt \
    "$OUTPUT_DIR"/ffuf/endpoints.txt 2>/dev/null | \
    sort -u > "$OUTPUT_DIR/summary/endpoints_summary.txt"

COUNT_SUMMARY=$(wc -l < "$OUTPUT_DIR/summary/endpoints_summary.txt" 2>/dev/null || echo 0)
echo -e "${GREEN}    ✓ ${COUNT_SUMMARY} unique endpoints${NC}"

# --- Detail file: merge all detail files, keep POST with body ---
# Katana detail already has METHOD prefix (GET/POST)
# GAU + ffuf don't have METHOD → prefix with GET
{
    # Katana: already formatted as "METHOD STATUS SIZEB URL [BODY: ...]"
    cat "$OUTPUT_DIR/katana/endpoints_detail.txt" 2>/dev/null

    # GAU: "STATUS SIZEB URL" → "GET STATUS SIZEB URL"
    awk '{print "GET " $0}' "$OUTPUT_DIR/gau/endpoints_detail.txt" 2>/dev/null

    # ffuf: "STATUS SIZEB URL" → "GET STATUS SIZEB URL"
    awk '{print "GET " $0}' "$OUTPUT_DIR/ffuf/endpoints_detail.txt" 2>/dev/null
} | sort -t' ' -k4 -u > "$OUTPUT_DIR/summary/endpoints_detail.txt"

COUNT_SUMMARY_DETAIL=$(wc -l < "$OUTPUT_DIR/summary/endpoints_detail.txt" 2>/dev/null || echo 0)
COUNT_SUMMARY_POST=$(grep -c '^POST ' "$OUTPUT_DIR/summary/endpoints_detail.txt" 2>/dev/null || echo 0)
echo -e "${GREEN}    ✓ ${COUNT_SUMMARY_DETAIL} detail entries (${COUNT_SUMMARY_POST} POST)${NC}"

# ═══════════════════════════════════════════════════════════════════════════
# REPORT
# ═══════════════════════════════════════════════════════════════════════════
ELAPSED=$(( $(date +%s) - SCAN_START ))

echo ""
echo -e "${BOLD}┌──────────────────────────────────────────────────────┐${NC}"
echo -e "${BOLD}│  CRAWL URL v2 — COMPLETE                             │${NC}"
echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
printf  "${BOLD}│${NC}  %-20s %-31s${BOLD}│${NC}\n" "Target" "$TARGET"
printf  "${BOLD}│${NC}  %-20s ${YELLOW}%-31s${NC}${BOLD}│${NC}\n" "Duration" "$(format_time $ELAPSED)"
echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
printf  "${BOLD}│${NC}  %-20s ${GREEN}%-10s${NC} (GET:%-4s POST:%-3s) ${BOLD}│${NC}\n" "Katana" "$COUNT_KATANA" "$COUNT_KATANA_GET" "$COUNT_KATANA_POST"
printf  "${BOLD}│${NC}  %-20s ${GREEN}%-10s${NC} endpoints      ${BOLD}│${NC}\n" "GAU" "$COUNT_GAU"
printf  "${BOLD}│${NC}  %-20s ${GREEN}%-10s${NC} endpoints      ${BOLD}│${NC}\n" "ffuf" "$COUNT_FFUF"
echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
printf  "${BOLD}│${NC}  %-20s ${CYAN}%-31s${NC}${BOLD}│${NC}\n" "Summary (deduped)" "$COUNT_SUMMARY endpoints"
echo -e "${BOLD}├──────────────────────────────────────────────────────┤${NC}"
printf  "${BOLD}│${NC}  Output: ${CYAN}%-41s${NC}${BOLD}│${NC}\n" "$OUTPUT_DIR"
echo -e "${BOLD}└──────────────────────────────────────────────────────┘${NC}"

echo ""
echo -e "${GREEN}[+] Summary (merged, for arjun/gf/nuclei):${NC}"
echo -e "    ${OUTPUT_DIR}/summary/endpoints_summary.txt  (URLs with params)"
echo -e "    ${OUTPUT_DIR}/summary/endpoints_detail.txt   (METHOD STATUS SIZEB URL [BODY])"
echo ""
echo -e "${GREEN}[+] Per-tool:${NC}"
echo -e "    ${OUTPUT_DIR}/katana/endpoints.txt / endpoints_detail.txt"
echo -e "    ${OUTPUT_DIR}/gau/endpoints.txt    / endpoints_detail.txt"
echo -e "    ${OUTPUT_DIR}/ffuf/endpoints.txt   / endpoints_detail.txt"
echo ""
echo -e "${YELLOW}[*] Quick use:${NC}"
echo -e "    nuclei -l ${OUTPUT_DIR}/summary/endpoints_summary.txt -t ~/nuclei-templates/"
echo -e "    cat ${OUTPUT_DIR}/summary/endpoints_summary.txt | gf sqli"
echo -e "    arjun -i ${OUTPUT_DIR}/summary/endpoints_summary.txt"
echo ""

# Cleanup temp
rm -rf "$TEMP_DIR"
