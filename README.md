# Mô tả
 Dự án được thực hiện và lấy ý tưởng theo bài báo ADAPT, nhưng tối ưu lại về tính thực tiễn của nó. Việc pentest tự động mà không kiểm soát có thể gây hỏng hệ thống mục tiêu, gây ảnh hưởng đến quá trình vận hành của hệ thống, không phù hợp với các dự án Pentest/RedTeam thực tế. Hơn nữa, việc sử dụng các công cụ và các kỹ thuật Post-Exploitation một cách máy móc khiến việc tấn công dễ bị phát hiện và bị chặn bởi các AV hay BlueTeam. Mục tiêu của Dự án này là RCE được càng nhiều hệ thống trong Target càng tốt (hoặc giới hạn lại theo yêu cầu của người sử dụng), hoặc ít nhất phát hiện các lỗ hổng bảo mật mức trung bình và cao, với các lỗ hổng phía client-side, do cần sự tương tác của những người sử dụng khác nên hệ thống chỉ dừng ở mức xác nhận lỗ hổng (confirm).
# Quy trình Recon
Ưu tiên subdomain => nmap =>katana (nếu mã không bị obfuscate)=> nuclei
nếu obfuscate => playwright + mimtproxy =>nuclei

- subfinder + gobuster =>alterx => dnsx => nuclei /network => metasploit
- subfinder + gobuster =>alterx => dnsx => => need_headless?katana+jq + gau +feroxbuster =>uro => httpx -H -mc 200,201 => arjun (GET,POST) => nuclei /http

** Tất cả phần xử lý trước khi có clean_urls.txt (danh sách toàn bộ url được xử lý sạch - không lặp + tồn tại.) đều qua file, sau đó có thể xóa toàn bộ file và để lại mỗi clean_urls.txt. Sử dụng katana, nếu dữ liệu POST ít hoặc không hoạt động được chuyển sang crawlgo.
(dungf katana sử dụng flag -cs để giới hạn domain api)
Các kịch bản tấn công sau khi có tệp clean_urls:
Kịch bản 1: JS file: gf js => linkfinder
Kịch bản 2: SQLi
Kịch bản 3: XSS
Kịch bản 4: LFI
Kịch bản 5: SSRF
Kịch bản 6: SSTI
Kịch bản 6: Upload -> tool fuxploider
# Các vấn đề khi xây dựng app
## Viết các tệp trong thư mục plugins

**Cần bổ sung -H để nhận cookie ở các plugin, nên truyền cookie vào Target.arch sau đó các tool có option -H sẽ sử dụng, nếu không có cookie mặc định sẽ bỏ -H**
- Viết các hàm khác nhau để chạy ở các chế độ khác nhau (tham khảo nmap, sayHello)
- Cần viết tách thành file **run** và file **guard**, mục địch để sau đó viết file AttackRepertoire.arch sẽ tách thành các Interface trong AttackSteps, mỗi 1 Interface là 1 chế độ trong 1 tool. Ví dụ nmap có 2 chế độ quét là FullScan và QuickScan thì sẽ đóng góp vào AttackSteps 2 Interface, mỗi Interface trong AttackSteps gồm 3 thành phần: MitreAttck - nhận giá trị là mã định danh kỹ thuật tấn công trong MitreAtt&ch, Guard và Run tương ứng nhận giá trị là hàm guard và hàm run của mỗi chế độ.
- Build các file .c thành các file .o, sau đó thành các file .so

## Viết 4 tệp .arch trong thư mục ./arch
**Dựa trên việc đọc các file .c trong source (loaders.c,...), hoàn thiện các tệp trong thư mục .arch như sau:**
- Thư mục gồm 4 file được gọi theo thứ tự: Tooling.arch, AttackRepertoire.arch, ScanRepertoire.arch, Target.arch.
- Tooling.arch cấu hình đường dẫn các tệp .so để định nghĩa các tools. *(đã hiểu và hoàn thành cấu trúc chính xác)*
- AttackRepertoire.arch gồm 2 Component là AttackSteps và AttackPatterns, trong đó
* AttackSteps định nghĩa các chế độ tấn công (đã đề cập cụ thể ở trên)
* AttackPatterns gồm 6 thuộc tính:
+ AttackVector - NETWORK/LOCAL/ADJACENT/PHYSICAL
+ AttackComplexity - LOW/HIGH
+ RequiredPrivileges - NONE/LOW/HIGH
+ UserInteraction - NONE/REQUIRED
+ RunningTime - SHORT/MEDIUM/LONG
+ Steps - Là **tên của Interface trong Component AttackSteps**
Cách nhau bởi dấu phẩy , ví dụ "Step1, Step2".
- ScanRepertoire.arch ...

!