# ESP32 → ThingsBoard cho ba máy con

Mở `ESP32_ThingsBoard.ino` bằng Arduino IDE. Giữ cùng thư mục các tệp
`gateway.cpp`, `config.h` và các header kèm theo; Arduino sẽ biên dịch chúng
cùng sketch. Không chỉ chép nội dung `.ino` sang một thư mục trống.

## Board và thư viện

- DOIT ESP32 DEVKIT V1 (ESP32-WROOM-32), Flash 4 MB.
- Arduino-ESP32 **2.0.17** (gói ESP32 **espressif32 6.10.0** trong PlatformIO).
- PubSubClient **2.8**.
- ArduinoJson **7.3.1**.
- UART 115200, 8N1: STM32 Master PA9 → ESP32 GPIO16;
  STM32 Master PA10 ← ESP32 GPIO17; nối chung GND. Tín hiệu logic 3,3 V.

Build PlatformIO dùng cấu hình của bộ firmware ở thư mục cha. Khi dùng Arduino
IDE, chọn board và cổng ESP32 thực tế rồi Verify/Upload.

## Cấu hình mạng

1. Chép `secrets.h.example` thành `secrets.h`.
2. Điền SSID, mật khẩu Wi-Fi và device access token của ThingsBoard.
3. Giữ `MQTT_USE_TLS 1`, cổng 8883. Gói đã kèm một tập root CA công khai
   (xem `TRUSTED_ROOTS.txt`); cấu hình mặc định dùng tập này để xác minh TLS.
   Nếu endpoint của bạn dùng chuỗi CA khác, ghi đè `THINGSBOARD_ROOT_CA` bằng
   PEM từ nguồn chính thức của endpoint/nhà phát hành. Chuỗi chứng chỉ thực tế
   của endpoint chưa được kiểm thử trên thiết bị; không dùng `setInsecure()`.
4. ESP32 đồng bộ thời gian qua SNTP để xác minh thời hạn chứng chỉ. Nếu mạng chặn
   NTP, cấu hình một nguồn thời gian tin cậy có thể truy cập trong mạng.

Nếu thiếu SSID/token (hoặc chủ động đặt CA rỗng), code vẫn build và nạp được; task UART vẫn chạy nhưng
không thử kết nối ThingsBoard. Serial Monitor cho biết cần cấu hình. Không có
thông tin đăng nhập thật trong gói mã.

Để thử trên bàn với MQTT plaintext, cần chủ động chọn `MQTT_USE_TLS 0` và cổng
1883. Khi đó token và lệnh MQTT không được mã hóa. Tệp `secrets.h` được gitignore.

## RPC trên dashboard

Dùng device RPC như các method sau. `params` khi set phải là boolean JSON
`true`/`false`, không phải chuỗi và không phải số 1/0.

```json
{"method":"setRelay1","params":true}
{"method":"setRelay2","params":false}
{"method":"setRelay3","params":true}
{"method":"getRelay1","params":{}}
{"method":"getRelay2","params":{}}
{"method":"getRelay3","params":{}}
```

Kết quả thành công là boolean trạng thái được xác nhận; lỗi là object JSON có
`error`. Cấu hình widget switch dùng method set/get đúng từng máy con. Mỗi máy
con có thể có một lệnh đang chờ; yêu cầu khác tới cùng máy khi còn pending trả
`BUSY`. Các node khác được Master xếp lịch chung trên một radio half-duplex.

Các telemetry key chính: `node1_online`, `node1_relay_known`, `node1_relay`
(boolean), `node1_valid`, `node1_sht_valid`, `node1_soil_valid`,
`node1_temperature`, `node1_humidity`, `node1_soil`,
`node1_sample_age_ms`, `node1_last_seen_age_ms`, `node1_rssi`, `node1_snr`.
Tương tự cho `node2_...` và `node3_...`.

Có thêm `master_uart_online`, `master_lora_ok` và bộ đếm lỗi. Khi dữ liệu cảm
biến không hợp lệ/cũ, các giá trị số cảm biến bị bỏ khỏi bản tin mới; cờ valid
và tuổi dữ liệu vẫn được cập nhật. Relay khi offline là **trạng thái xác nhận
cuối**, không có nghĩa hệ thống vẫn biết trạng thái tức thời. Widget cần hiển
thị cờ online và relay_known cùng giá trị relay.

## Hành vi mất kết nối

- Mất Wi-Fi/MQTT/UART/RF không phát sinh lệnh OFF và không tự bật bơm.
- UART được task riêng phục vụ ngay cả khi DNS/TCP/TLS/MQTT đang chờ kết nối.
- UART quá 3,5 giây không có khung hợp lệ làm `master_uart_online=false`.
- Node RF hết tuổi 8 giây bị báo offline; dữ liệu cảm biến có giới hạn độ mới.
- Gửi lại UART dùng cùng session/id và phiên khởi động Master đã chụp lúc nhận
  RPC. Khi Master reset, lệnh cũ bị kết thúc `BOOT_CHANGED` và không tiếp tục
  gửi lại như một yêu cầu mới; relay tại máy con không bị đổi do việc này.
- Kết quả RPC được giữ khi publish thất bại, gửi lại kết quả khi reconnect;
  không gửi lại lệnh bơm chỉ để khôi phục phản hồi cloud.
- Cache chống trùng giữ tối đa 16 yêu cầu gần nhất, giới hạn 5 phút cho kết quả
  đã publish. Đây là cache RAM; reset ESP32 xóa cache. Session mới tách ID mới.
- PubSubClient publish là QoS 0. Publish thành công chưa chứng minh ThingsBoard
  đã xử lý; cache và đồng bộ trạng thái giảm mất phản hồi, không thay thế xác
  nhận ứng dụng đầu cuối nếu cần bảo đảm cao hơn.

Bộ này dùng giao thức UART `U1,...` mới; phải nạp Master và Slave cùng phiên
bản, không trộn với firmware cũ dùng `N1/TB/TBRES`.
