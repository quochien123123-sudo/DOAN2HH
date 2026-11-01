# Kết quả kiểm tra bản giao

Ngày: 31/10/2026. Các kết quả dưới đây thuộc mã nguồn trong gói này,
không phải các firmware cũ trong hai tệp RAR.

## Build thực tế

| Target | Toolchain | Flash ứng dụng | RAM tĩnh/vùng linker |
|---|---|---:|---:|
| Master | ARM GCC7.3.1 20180622 |17.044B /63.488B|4.040B /20.480B|
| Slave1 | ARM GCC7.3.1 20180622 |17.672B /63.488B|2.344B /20.480B|
| Slave2 | ARM GCC7.3.1 20180622 |17.668B /63.488B|2.344B /20.480B|
| Slave3 | ARM GCC7.3.1 20180622 |17.656B /63.488B|2.344B /20.480B|
| DOIT ESP32 DEVKIT V1 | Xtensa GCC8.4.0, Arduino-ESP32 2.0.17 |1.035.061B /1.310.720B|48.540B /327.680B|

Không có compiler warning trong các build cuối. STM32 application dùng
-Wall/-Wextra/-Werror; syscalls.c do ST sinh sẵn chỉ tắt cảnh báo unused-parameter.
ESP32 pin PlatformIO6.1.18/platform6.10.0, PubSubClient2.8, ArduinoJson7.3.1, WiFiManager2.0.17.

STM32 ELF32 ARM/Thumb và vector đã kiểm: stack0x20005000, reset vector trùng
entry Thumb, payload nằm dưới ranh62KiB. ESP32 factory image đã kiểm offset
và bytes từng thành phần; nguồn/header hiện tại khớp source hashes của build.

## Sáu nhóm host test đã chạy và đạt

1. Protocol: roundtrip, CRC sai, độ dài sai, ID/value/range, số vượt32bit,
   số có dấu sai và trường trống/thừa.
2. Slave policy: bootOFF, session, apply/duplicate/stale, thao tác tại nút giữa
   retry, CAS conflict, đổi boot, phiên Master mới và sequence wrap.
3. Flash journal:800 lượt boot và xoay hai trang, commit bị gián đoạn, phục hồi
   không coi bản ghi dở là hợp lệ, khóa Flash lại sau ghi.
4. Master: UART epoch cũ/đúng, không nhận khung sáu token cũ, dedup/rejection
   cache, tách node, ACK cũ, preflight mới, override, timeout giữON.
5. Radio fake HAL: TX không chờ foreground, CRC, frame quá dài, pending RX,
   lỗi SPI/FIFO, deadline500ms và radio mất cấu hình.
6. ESP32 parser: ba node, framing/range, độ mới, session/ID/node và hủy lệnh
   khi Master đổi boot, giữ relay-last-known. Test parser dùng ASan/UBSan.

Xem `host-tests.log`, `stm32-build.log`, `ESP32_ThingsBoard/build.log` và manifest.
Firmware ESP32 build sẵn dùng WiFiManager để nhập/lưu Wi-Fi, chưa có token ThingsBoard riêng.

## Chưa được xác nhận bằng các kiểm tra này

Chưa nạp lên board, chưa đo sóng RF/độ trễ/nguồn và chưa vận hành bơm hoặc
kiểm tra tiếp điểm relay thật. Chưa kết nối broker ThingsBoard thực tế hoặc
xác minh chuỗi chứng chỉ hiện tại của endpoint trên thiết bị. Các host test
có stub HAL/network không thay thế các thử nghiệm đó. Trình tự thử trên mạch
đã ghi trong `README_VI.md`.

Đã build lại đúng board `esp32doit-devkit-v1`, environment `devkitv1`; default.csv app0 0x140000 byte được pin rõ. RAM48.540B, Flash1.035.061B; build đạt và không có cảnh báo.

## Cập nhật WiFiManager và D27 — 01/11/2026

Bản ESP32 hiện tại dùng WiFiManager2.0.17 trên DOIT ESP32 DEVKIT V1;
Flash1.035.061/1.310.720B (79%), RAM tĩnh48.540/327.680B (14,8%).
Build Xtensa thực tế đạt, không cảnh báo; image ghép và source hashes đã kiểm.
Master/Slave không đổi, giữ nguyên các binary đã build trước.

Đã chạy lại parser gateway (ASan/UBSan) và sáu tình huống helper D27:
4999/5000ms, chỉ kích hoạt một lần khi giữ, nhả ổn định để kích hoạt lại,
nhấn ngắn không cộng dồn, chống dội cạnh, millis wrap và giữ nút lúc boot.
Xem `wifimanager-tests.log`. WiFiManager lifecycle đã được đối chiếu với mã
thư viện2.0.17; captive portal/save NVS và thời gian đáp ứng thực tế vẫn cần
thử trên ESP32 cùng nút thật.
