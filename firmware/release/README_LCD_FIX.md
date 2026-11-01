# Bản sửa màn hình LCD STM32

Gói này có HEX đã build cho Master và Slave ID1/2/3. Dùng STM32CubeProgrammer
qua ST-Link/SWD, chọn đúng file cho từng mạch, Download+Verify rồi Reset.
HEX đã chứa địa chỉ; nếu dùng BIN trong repository, địa chỉ là0x08000000.
Nạp/reset Slave khởi động relayOFF như thiết kế; mất liên lạc vẫn giữ relay.

- Master/Master.hex: LCD20x4, Master PB0chọnnode/PB1điềukhiển.
- Slave1/Slave1.hex: máy con ID1, LCD16x2.
- Slave2/Slave2.hex: máy con ID2.
- Slave3/Slave3.hex: máy con ID3.

Đã sửa Master màn hình trắng khi radio chưa sẵn sàng; sửa Slave dừng LCD khi
BUSY ngắn sau STOP. Nguồn, sơ đồ chân và loại backpack vẫn cần đúng thực tế.
LCD I2C: PB6=SCL, PB7=SDA, chungGND; nguồn theo thông số module (thường5V).
Nếu đèn nền sáng mà không có chữ, xoay chậm biến trở tương phản trên backpack.
Nếu có ô vuông mà không có chữ, kiểm tra init/I2C và đúng firmware. Driver tự
thử0x27/0x3F; địa chỉ backpack khác cần xác định riêng. Nếu đèn nền không sáng,
kiểm tra VCC/GND/jumper backlight trước.

Cả Master lẫn Slave phải dùng cùng giao thức mới trong repository. Code ESP32
WiFiManager đang dùng giữ nguyên. Bộ source đầy đủ và hướng dẫn tại:
https://github.com/quochien123123-sudo/DOAN2HH/tree/firmware-devkitv1-three-pumps-20261101

Build và host tests đạt; chưa thử trên mạch thực tế.
