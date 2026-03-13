pip install playwright --break-system-packages
playwright install chromium
🚀 SỬ DỤNG:
bash# Detect only (xem script tìm được gì)
python3 smart_login.py \
    -u http://localhost:3000 \
    --detect-only \
    --visible          # ← Hiện browser để debug

# Full attack
python3 smart_login.py \
    -u http://localhost:3000 \
    --userlist /home/resource/usernamelist.txt \
    -w /home/resource/passwordlist.txt \
    --proxy http://127.0.0.1:8080 \
    --visible          # ← Bỏ flag này để chạy headless
