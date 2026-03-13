#!/bin/bash

# ==============================================================================
#  SUBDOMAIN RECON - ENUMERATION & VALIDATION
#  Output: live_subdomains.txt, live_urls.txt
# ==============================================================================

DOMAIN="$1"
OUTPUT_DIR="/home/pikachu/workspaces/subdomain_recon"

# Màu sắc
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# --- KIỂM TRA ĐẦU VÀO & CÔNG CỤ ---
if [ -z "$DOMAIN" ]; then
    echo -e "${RED}[!] Usage: ./subdomain_recon.sh <domain.com>${NC}"
    echo -e "${YELLOW}[*] Example: ./subdomain_recon.sh example.com${NC}"
    exit 1
fi

# Kiểm tra tools
for tool in subfinder gobuster dnsx httpx; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}[!] Error: '$tool' not installed${NC}"
        exit 1
    fi
done

mkdir -p "$OUTPUT_DIR"

# Files output chính
FILE_SUBDOMAINS="$OUTPUT_DIR/live_subdomains.txt"
FILE_URLS="$OUTPUT_DIR/live_urls.txt"

# Files tạm
TEMP_SUBFINDER="$OUTPUT_DIR/temp_subfinder.txt"
TEMP_GOBUSTER="$OUTPUT_DIR/temp_gobuster.txt"
TEMP_ALL_SUBS="$OUTPUT_DIR/temp_all_subdomains.txt"
TEMP_DNS_ALIVE="$OUTPUT_DIR/temp_dns_alive.txt"

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║    SUBDOMAIN RECONNAISSANCE PIPELINE       ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo -e "${YELLOW}[*] Target Domain: ${DOMAIN}${NC}"
echo -e "${YELLOW}[*] Output Directory: ${OUTPUT_DIR}${NC}"
echo ""

# =======================================================
# BƯỚC 1: SUBFINDER (PASSIVE ENUMERATION)
# =======================================================
echo -e "${BLUE}[1/5] Running Subfinder (Passive DNS)...${NC}"

subfinder -d "$DOMAIN" -all -silent -o "$TEMP_SUBFINDER" 2>/dev/null

if [ -s "$TEMP_SUBFINDER" ]; then
    COUNT_SUBFINDER=$(wc -l < "$TEMP_SUBFINDER")
    echo -e "${GREEN}    ✓ Found ${COUNT_SUBFINDER} subdomains${NC}"
else
    echo -e "${YELLOW}    ! No results from Subfinder${NC}"
    touch "$TEMP_SUBFINDER"
fi

# =======================================================
# BƯỚC 2: GOBUSTER DNS (ACTIVE BRUTEFORCE)
# =======================================================
echo -e "${BLUE}[2/5] Running Gobuster DNS (Active Bruteforce)...${NC}"

# Tìm wordlist (SecLists common)
WORDLIST=""
if [ -f "/usr/share/seclists/Discovery/DNS/subdomains-top1million-5000.txt" ]; then
    WORDLIST="/usr/share/seclists/Discovery/DNS/subdomains-top1million-5000.txt"
elif [ -f "/usr/share/wordlists/seclists/Discovery/DNS/subdomains-top1million-5000.txt" ]; then
    WORDLIST="/usr/share/wordlists/seclists/Discovery/DNS/subdomains-top1million-5000.txt"
elif [ -f "subdomains.txt" ]; then
    WORDLIST="subdomains.txt"
    echo -e "${YELLOW}    ! Wordlist not found. Creating basic wordlist...${NC}"
    echo -e "www\napi\ndev\nstaging\nadmin\ntest\nmail\nftp\nvpn\nportal\ndashboard" > "$OUTPUT_DIR/basic_wordlist.txt"
    WORDLIST="$OUTPUT_DIR/basic_wordlist.txt"
fi

gobuster dns -d "$DOMAIN" \
    -w "$WORDLIST" \
    -t 50 \
    --no-error \
    -q \
    -o "$TEMP_GOBUSTER" 2>/dev/null

if [ -s "$TEMP_GOBUSTER" ]; then
    # Gobuster output format: "Found: subdomain.domain.com"
    cat "$TEMP_GOBUSTER" | grep "Found:" | awk '{print $2}' > "${TEMP_GOBUSTER}.clean"
    mv "${TEMP_GOBUSTER}.clean" "$TEMP_GOBUSTER"
    COUNT_GOBUSTER=$(wc -l < "$TEMP_GOBUSTER")
    echo -e "${GREEN}    ✓ Found ${COUNT_GOBUSTER} subdomains${NC}"
else
    echo -e "${YELLOW}    ! No results from Gobuster${NC}"
    touch "$TEMP_GOBUSTER"
fi

# =======================================================
# BƯỚC 3: MERGE & DEDUPLICATE
# =======================================================
echo -e "${BLUE}[3/5] Merging & Deduplicating...${NC}"

cat "$TEMP_SUBFINDER" "$TEMP_GOBUSTER" | sort -u > "$TEMP_ALL_SUBS"

COUNT_ALL=$(wc -l < "$TEMP_ALL_SUBS")
echo -e "${GREEN}    ✓ Total unique subdomains: ${COUNT_ALL}${NC}"

if [ "$COUNT_ALL" -eq 0 ]; then
    echo -e "${RED}[!] No subdomains found. Exiting.${NC}"
    exit 1
fi

# =======================================================
# BƯỚC 4: DNSX (VERIFY DNS RESOLUTION)
# =======================================================
echo -e "${BLUE}[4/5] Validating DNS resolution (dnsx)...${NC}"

dnsx -l "$TEMP_ALL_SUBS" -resp -silent -o "$TEMP_DNS_ALIVE" 2>/dev/null

if [ -s "$TEMP_DNS_ALIVE" ]; then
    # dnsx output: "subdomain.com [IP]"
    cat "$TEMP_DNS_ALIVE" | awk '{print $1}' | sort -u > "$FILE_SUBDOMAINS"
    COUNT_DNS=$(wc -l < "$FILE_SUBDOMAINS")
    echo -e "${GREEN}    ✓ Live subdomains: ${COUNT_DNS}${NC}"
else
    echo -e "${RED}[!] No live subdomains found${NC}"
    touch "$FILE_SUBDOMAINS"
    exit 1
fi

# =======================================================
# BƯỚC 5: HTTPX (GET LIVE WEB URLS + FOLLOW REDIRECTS)
# =======================================================
echo -e "${BLUE}[5/5] Probing web servers (httpx)...${NC}"

httpx -l "$FILE_SUBDOMAINS" \
    -fr \
    -mc 200,201,401,403 \
    -silent \
    -no-color \
    -o "$FILE_URLS" 2>/dev/null

if [ -s "$FILE_URLS" ]; then
    # Chuẩn hóa URLs: Chỉ giữ root domain (bỏ path sau redirect)
    cat "$FILE_URLS" | awk -F/ '{print $1 "//" $3}' | sort -u > "${FILE_URLS}.tmp"
    mv "${FILE_URLS}.tmp" "$FILE_URLS"
    
    COUNT_URLS=$(wc -l < "$FILE_URLS")
    echo -e "${GREEN}    ✓ Live web URLs: ${COUNT_URLS}${NC}"
else
    echo -e "${YELLOW}    ! No live web servers found${NC}"
    touch "$FILE_URLS"
fi

# =======================================================
# DỌN DẸP
# =======================================================
rm -f "$TEMP_SUBFINDER" "$TEMP_GOBUSTER" "$TEMP_ALL_SUBS" "$TEMP_DNS_ALIVE"

# =======================================================
# SUMMARY
# =======================================================
echo ""
echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           RECON COMPLETED                  ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo -e "${GREEN}[+] Output files:${NC}"
echo -e "    📁 ${FILE_SUBDOMAINS} (${COUNT_DNS} subdomains)"
echo -e "    📁 ${FILE_URLS} (${COUNT_URLS} URLs)"
echo ""
echo -e "${YELLOW}[*] Next step: Use live_urls.txt for deep crawling${NC}"


