# Kết quả kiểm tra bản giao

Ngày: 31/10/2026. Các kết quả dưới đây thuộc mã nguồn trong gói này,
không phải các firmware cũ trong hai tệp RAR.

## Build thực tế

| Target | Toolchain | Flash ứng dụng | RAM tĩnh/vùng linker |
|---|---|---:|---:|
| Master | ARM GCC7.3.1 20180622 |17.068B /63.488B|4.040B /20.480B|
| Slave1 | ARM GCC7.3.1 20180622 |17.724B /63.488B|2.344B /20.480B|
| Slave2 | ARM GCC7.3.1 20180622 |17.720B /63.488B|2.344B /20.480B|
| Slave3 | ARM GCC7.3.1 20180622 |17.708B /63.488B|2.344B /20.480B|
| DOIT ESP32 DEVKIT V1 | Xtensa GCC8.4.0, Arduino-ESP32 2.0.17 |1.036.561B /1.310.720B|48.540B /327.680B|

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

## Bản sửa LCD — 01/11/2026

Đã sửa hai lỗi có thể gây màn hình trắng:
- Master không còn để các poll chưa thể chạy (radio lỗi) chặn vĩnh viễn việc
  vẽ LCD; màn hình vẫn hiện trạng thái RF:ERR.
- Slave chờ BUSY tối đa3ms để kết thúc STOP bình thường thay vì vô hiệu hóa LCD
  ngay khi thấy BUSY thoáng qua. Chờ nguồn50ms trước lần dò I2C đầu ở cả hai.

Đã chạy đủ9nhóm host test qua tools/test_all.py; thêm kiểm tra scheduler LCD
Master và driver LCD Slave với HAL giả. Kiểm tra BUSY thoáng qua, BUSY kẹt,
HAL lỗi, địa chỉ0x27/0x3F, init/write sequence, tick wrap và không có radio đều
đạt. Build lại4target STM32 đạt, không cảnh báo. ESP32 WiFiManager không đổi.
Những kiểm tra này chứng minh các đường lỗi trong mã; chưa xác nhận nguyên
nhân phần cứng trên màn hình của người dùng hoặc thử LCD thật sau bản sửa.

## Chẩn đoán ThingsBoard Cloud — 01/11/2026

ESP32 giữ endpoint `mqtt.thingsboard.cloud:8883`, TLS xác minh CA và Device
Access Token làm MQTT username. Firmware hiện ghi nguyên nhân reset, endpoint,
RSSI, heap, thời gian thử, mã/trạng thái PubSubClient và lỗi tầng TLS khi có;
không ghi giá trị token. Lịch retry vẫn để task UART hoạt động độc lập.

Đã build lại đúng `esp32doit-devkit-v1`: Flash1.036.561/1.310.720B (79,1%),
RAM tĩnh48.540/327.680B (14,8%), không có cảnh báo. Host test biên dịch trực
tiếp các helper chẩn đoán trong `gateway.cpp`, kiểm tra đủ state -4..5 và năm
tình huống token/TLS. Chưa dùng token thật hoặc kết nối broker từ phần kiểm tra
này; người dùng cần nạp lên ESP32 và gửi dòng lỗi mới để xác định sự cố mạng.
