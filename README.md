# DOAN2HH — ba máy con điều khiển bơm qua LoRa

Bộ mã nguồn đồng bộ cho STM32F103C8 Master, ba STM32 Slave và **DOIT ESP32 DEVKIT V1** kết nối ThingsBoard.

- [Hướng dẫn tiếng Việt: nối dây, build và nạp](firmware/README_VI.md)
- [Tải riêng code ESP32 WiFiManager + nút D27, ZIP nhỏ](firmware/release/ESP32_ThingsBoard/ESP32_DevKitV1_WiFiManager.zip?raw=true)
- [Code ESP32 mở bằng Arduino IDE](firmware/ESP32_ThingsBoard/ESP32_ThingsBoard.ino) — giữ toàn bộ thư mục cùng sketch.
- [Code Master](firmware/Master/Core/Src/main.c)
- [Code Slave dùng chung cho ID 1, 2, 3](firmware/relay32/Core/Src/main.c)
- [File nạp đã build và kết quả kiểm tra](firmware/release/)

## Tải về máy

Bấm **Code → Download ZIP** trên nhánh `firmware-devkitv1-three-pumps-20261101`, giải nén rồi xem thư mục `firmware`.

[Tải ZIP của nhánh firmware](https://github.com/quochien123123-sudo/DOAN2HH/archive/refs/heads/firmware-devkitv1-three-pumps-20261101.zip)

## Chọn đúng file nạp

| Mạch | File |
|---|---|
| STM32 Master | `firmware/release/Master/Master.hex` |
| Slave ID 1 | `firmware/release/Slave1/Slave1.hex` |
| Slave ID 2 | `firmware/release/Slave2/Slave2.hex` |
| Slave ID 3 | `firmware/release/Slave3/Slave3.hex` |
| ESP32 DevKit V1, ảnh ghép tại 0x0 | `firmware/release/ESP32_ThingsBoard/factory.bin` |

ESP32 dùng **WiFiManager**: nối nút D27 (GPIO27) xuống GND, giữ **5 giây** để mở AP `Pump-Setup-xxxxxx` (mật khẩu mặc định `PumpSetup27`); truy cập `http://192.168.4.1` để cấu hình Wi-Fi. Lần đầu chưa có Wi-Fi lưu cũng tự mở portal. Chép `secrets.h.example` thành `secrets.h`, điền token ThingsBoard rồi build lại để kết nối cloud. Arduino IDE dùng board **DOIT ESP32 DEVKIT V1**, core ESP32 **2.0.17**, PubSubClient **2.8**, ArduinoJson **7.3.1**, **WiFiManager by tzapu 2.0.17**.

Relay giữ nguyên trạng thái khi mất liên lạc; reset/mất điện rồi khởi động lại là OFF. Master PB0 chọn máy con, PB1 bật/tắt máy được chọn. Nạp đồng bộ toàn bộ firmware mới, không trộn với bản cũ trong hai tệp RAR.

Đã build STM32/ESP32 và chạy kiểm tra logic trên host. Chưa thử trên mạch và bơm thực tế; xem [báo cáo kiểm chứng](firmware/release/VALIDATION.md).
