# Bộ firmware ba máy con điều khiển bơm

Bộ mã đồng bộ gồm **STM32 Master**, **STM32 Slave** (build ba ID) và **ESP32 ThingsBoard**. Đã biên dịch thực tế cả ARM Cortex-M3 và Xtensa ESP32; xem `release/VALIDATION.md`. Chưa thử trên mạch thực tế.

## 1. Hành vi đã chọn

- Ba máy con có ID **1, 2, 3**, mỗi máy điều khiển một relay active-low.
- Mất RF, UART, Wi-Fi hoặc MQTT: **relay giữ nguyên trạng thái**, không tự bật/tắt.
- Reset/mất điện rồi có điện: Slave khởi động **OFF**. Đây là chính sách riêng với mất liên lạc.
- Master **PB0 chọn Node 1→2→3**, **PB1 bật/tắt node đang chọn**. Nút toggle sử dụng trạng thái vừa hỏi lại, không dựa vào màn hình cũ. Node chưa online thì chưa nhận lệnh.
- Slave **PA0** bật/tắt tại chỗ. Nếu thao tác tại chỗ làm thay đổi trạng thái giữa lúc nhận lệnh, kiểm tra revision ngăn lệnh cũ ghi đè; gửi lại cùng giao dịch không thực thi lần nữa.
- Master hỏi trạng thái từng node theo lịch; Slave không tự phát bản tin định kỳ để giảm va chạm. Đợi vài giây sau bật nguồn để Master thấy đủ ba node.
- Khi Master reset, lệnh UART thuộc phiên khởi động cũ bị từ chối, không tự chạy lại.

**Phải dùng cả ba loại firmware trong gói này.** Giao thức RF nhị phân 34 byte và UART `U1,...` mới không tương thích firmware cũ `T=.../C,...` và `N1/TB/TBRES`.

## 2. Sơ đồ chân theo hai ảnh IOC

Cả hai STM32 là **STM32F103C8Tx**, Flash 64 KiB, RAM 20 KiB, HSE 8 MHz → CPU 72 MHz như dự án gốc; debug SWD giữ PA13/PA14.

| Chức năng | Master | Slave |
|---|---|---|
| SPI1 SCK / MISO / MOSI | PA5 / PA6 / PA7 | PA5 / PA6 / PA7 |
| LoRa NSS / RESET / DIO0 | PB12 / PB13 / PB14 | PB0 / PB1 / PB10 |
| I2C1 SCL / SDA | PB6 / PB7 | PB6 / PB7 |
| Nút | PB0 chọn node, PB1 điều khiển | PA0 điều khiển tại chỗ |
| Relay | Không nối tại Master | PA1, LOW=ON / HIGH=OFF |
| Cảm biến đất | Không dùng | PA2 / ADC1_IN2 |
| UART tới ESP32 | PA9 TX, PA10 RX | Không dùng |

UART: **Master PA9 → ESP32 GPIO16**, **Master PA10 ← ESP32 GPIO17**, chung GND, logic 3,3 V, 115200 8N1. ESP32 target đã build là **DOIT ESP32 DEVKIT V1 / WROOM-32, Flash 4 MB**, không phải ESP32-C3/S3.

LoRa RA-02/SX1278: 433 MHz, SF7, BW125 kHz, CR4/5, preamble 8, PHY CRC bật. Slave PB10 dùng polling thay vì phụ thuộc ngắt EXTI, giữ nguyên dây nối trên ảnh. Master DIO0 PB14 có EXTI và polling dự phòng.

LCD Master 20×4, Slave 16×2; tự dò địa chỉ PCF8574 0x27/0x3F. SHT30 mặc định 0x44 (`relay32/Core/Inc/sht30.h`). LCD lỗi bị tách khỏi đường relay/ACK; cảm biến lỗi có cờ valid riêng.

Bản sửa LCD: Slave chờ BUSY tối đa 3 ms để kết thúc STOP thay vì vô hiệu hóa màn hình ngay; Master vẫn vẽ LCD khi radio chưa sẵn sàng. Cả hai chờ nguồn 50 ms trước khi dò địa chỉ. File HEX mới có trong `release/STM32_LCD_Fix.zip`. Đèn nền sáng nhưng không có chữ vẫn cần kiểm tra biến trở tương phản, dây PB6/PB7 và địa chỉ backpack thực tế.

## 3. Nạp STM32 ngay từ file đã build

Dùng **STM32CubeProgrammer + ST-Link/SWD**; BOOT0=0. Nối SWDIO PA13, SWCLK PA14, GND và nguồn tham chiếu 3,3 V phù hợp với mạch.

| Mạch | File Intel HEX |
|---|---|
| Master | `release/Master/Master.hex` |
| Máy con 1 | `release/Slave1/Slave1.hex` |
| Máy con 2 | `release/Slave2/Slave2.hex` |
| Máy con 3 | `release/Slave3/Slave3.hex` |

Chọn đúng file, Download, Verify rồi Reset. **Không nạp cùng Slave1.hex cho cả ba mạch**, vì sẽ trùng ID. HEX tự chứa địa chỉ; nếu chọn `.bin`, nhập địa chỉ **0x08000000**.

Ví dụ CLI, chạy từ thư mục `firmware`:

```bash
STM32_Programmer_CLI -c port=SWD -w release/Master/Master.hex -v -rst
STM32_Programmer_CLI -c port=SWD -w release/Slave1/Slave1.hex -v -rst
```

Đổi file tương ứng khi nối ST-Link sang máy con 2/3. Nạp xong cả bộ rồi khởi động lại ESP32/Master/Slave và đợi đồng bộ; không trộn một mạch firmware cũ với mạch mới.

## 4. Build lại STM32

Mã nguồn đầy đủ nằm trong `Master/` và `relay32/`, gồm HAL/CMSIS/startup/linker script. Không chỉ thay mỗi `main.c` vào dự án cũ.

### STM32CubeIDE

1. File → Import → Existing Projects into Workspace, chọn `Master/` và `relay32/`.
2. Giữ đúng `STM32F103C8TX_FLASH.ld` đã kèm, vùng FLASH ứng dụng **62K**.
3. Với Slave, đặt `NODE_ID` trong `relay32/Core/Inc/app_config.h` thành 1/2/3, hoặc thêm symbol `NODE_ID=2` trong C/C++ Build Settings. Clean/Build lại sau mỗi ID.
4. File output CubeIDE có tên dự án `Master` hoặc `relay32`; nhớ ghi rõ ID khi lưu firmware Slave.

Các `.ioc` được giữ để đối chiếu phần cứng. Main và driver đã được viết lại; Generate Code từ CubeMX có thể ghi đè. Nếu cần sinh mã lại, tích hợp lại phần ứng dụng, driver, IRQ và linker **62K**, không dùng main mới sinh thay trực tiếp.

### Build từ dòng lệnh

Cần Python **3.9+** và GNU Arm Embedded GCC. Bản này đã kiểm tra bằng **GCC 7.3.1 20180622**. Thêm thư mục compiler `bin` vào PATH, rồi:

```bash
python tools/build_stm32.py
```

Hoặc chỉ định compiler trên Windows, ví dụ thay đường dẫn theo nơi cài của bạn:

```powershell
python tools/build_stm32.py --gcc-bin "C:\ArmGCC\bin"
python tools/build_stm32.py --target Slave2 --gcc-bin "C:\ArmGCC\bin"
```

Output `build/Master`, `build/Slave1`, `build/Slave2`, `build/Slave3` có ELF/BIN/HEX/map. Script tự gán NODE_ID đúng cho từng bản. Common headers có bản sao trong mỗi project để CubeIDE dùng độc lập; script kiểm tra chúng khớp `common/`.

**Vùng Flash phiên khởi động:** 2 KiB cuối (0x0800F800–0x0800FFFF) dành cho boot counter dạng journal; linker ứng dụng đã giới hạn 62 KiB. Không lưu trạng thái ON của bơm vào đây. Normal reset giữ được số phiên. Mass erase xóa journal, vì vậy sau full erase/nạp lại bộ cần khởi động lại gateway và đồng bộ lại, không giữ hàng đợi điều khiển cũ. Lỗi ghi journal làm radio control không được khởi động, trạng thái boot vẫn OFF.

## 5. Build và nạp ESP32

### Cấu hình Wi-Fi và ThingsBoard

1. Chép `ESP32_ThingsBoard/secrets.h.example` thành `secrets.h` trong cùng thư mục.
2. Điền `THINGSBOARD_TOKEN`; SSID/mật khẩu Wi-Fi được cấu hình bằng WiFiManager, không ghi trong mã.
3. Mặc định dùng TLS cổng 8883 và tập public root CA đi kèm. Nếu endpoint dùng CA khác, đặt PEM tin cậy trong `THINGSBOARD_ROOT_CA`; không tắt xác minh TLS. ESP32 cần truy cập SNTP để đồng bộ thời gian.

Nút cấu hình Wi-Fi: nối **D27 (GPIO27) với GND** qua nút nhấn, dùng kéo lên nội.
Lần đầu chưa có mạng lưu, hoặc khi **giữ D27 trong 5 giây**, ESP32 mở AP
**`Pump-Setup-xxxxxx`**, mật khẩu mặc định **`PumpSetup27`**. Kết nối AP rồi mở
**http://192.168.4.1** → Configure WiFi → nhập mạng/mật khẩu → Save. ESP32 lưu
thông tin Wi-Fi và tự dùng lại sau khởi động; portal hết khoảng 180 giây sẽ đóng.
Giữ nút không xóa cấu hình, không reset thiết bị và không đổi relay.

Không có mật khẩu/token thật trong gói. File ESP32 build sẵn cấu hình Wi-Fi
được bằng D27 và vẫn xử lý UART; để gửi ThingsBoard, điền token vào `secrets.h`
rồi build lại. Wi-Fi không còn cần nhập trong mã.

### Arduino IDE

Mở **`ESP32_ThingsBoard/ESP32_ThingsBoard.ino`**, giữ nguyên `gateway.cpp`, `config.h`, `trusted_roots.h`, `wifi_button.h` và các tệp trong cùng thư mục. `.ino` gọi phần ứng dụng trong `gateway.cpp`; không chỉ chép nội dung `.ino` riêng lẻ.

- Board package: **esp32 by Espressif Systems 2.0.17**.
- Board: **DOIT ESP32 DEVKIT V1**; Flash 4 MB, mặc định partition có app khoảng 1,3 MB; DIO 40 MHz.
- Library Manager: **PubSubClient 2.8**, **ArduinoJson 7.3.1**, **WiFiManager by tzapu 2.0.17**.
- Chọn cổng USB, Verify rồi Upload. Serial Monitor 115200.

### PlatformIO

Tại thư mục firmware, cấu hình đã pin phiên bản:

```bash
pio run -e devkitv1
pio run -e devkitv1 -t upload --upload-port COM5
```

Đổi COM5 theo cổng thực tế; Linux có thể là `/dev/ttyUSB0`. Bản build được kiểm tra bằng PlatformIO **6.1.18**.

Trong môi trường cloud này, registry PlatformIO bị chính sách mạng từ chối; build thực tế dùng các gói GitHub chính thức đã kiểm tra SHA-256. Có sẵn đường build tái lập cho Linux x86_64/Python 3.12+:

```bash
python tools/build_esp32_cloud.py --cache /tmp/pump-esp32-tools --jobs 4
```

Script không kết nối ThingsBoard và không tự nạp thiết bị; nó tạo `release/ESP32_ThingsBoard` với manifest và `factory.bin` đã ghép.

### Nạp ESP32 từ file ghép

`release/ESP32_ThingsBoard/factory.bin` nạp tại **0x0**. Đây là ảnh ghép bootloader, partition, boot_app0 và app, không dùng địa chỉ ứng dụng STM32.

```bash
python -m pip install esptool==4.5.1
python -m esptool --chip esp32 --port COM5 --baud 460800 write_flash 0x0 release/ESP32_ThingsBoard/factory.bin
```

Nếu công cụ nạp từng phần: bootloader 0x1000, partitions 0x8000, boot_app0 0xe000, firmware 0x10000. Không nạp `firmware.bin` của ESP32 tại 0x0. Với cấu hình cá nhân, build lại trước rồi dùng artifact mới.

## 6. ThingsBoard dashboard

Widget mỗi bơm dùng method set/get tương ứng; **params là boolean**, không phải chuỗi hoặc số:

```json
{"method":"setRelay1","params":true}
{"method":"setRelay2","params":false}
{"method":"setRelay3","params":true}
{"method":"getRelay1","params":{}}
```

Tương tự `getRelay2/getRelay3`. Thành công trả boolean relay được xác nhận; lỗi trả object `error`, như OFFLINE, BUSY, CONFLICT, BOOT_CHANGED hoặc ACK_TIMEOUT. Timeout là kết quả chưa xác nhận, không phải bằng chứng bơm chưa thực thi.

Telemetry: `node1_relay` (boolean), `node1_online`, `node1_relay_known`, `node1_valid`, `node1_temperature`, `node1_humidity`, `node1_soil`, các tuổi dữ liệu và trạng thái cảm biến riêng; tương tự node2/node3. Thêm `master_uart_online` và `master_lora_ok`. Khi offline, relay là trạng thái xác nhận gần nhất; widget cần hiển thị online/tuổi dữ liệu cùng giá trị relay.

Xem thêm `ESP32_ThingsBoard/README_VI.md`. Cache RPC RAM giữ tối đa 16 yêu cầu; publish PubSubClient QoS 0 chưa chứng minh ThingsBoard đã xử lý. Không có xác thực mật mã tầng LoRa trong bản P2P này; CRC và session chống lỗi/gửi trùng vận hành, không thay thế xác thực người gửi.

## 7. Kiểm tra sau nạp

1. Xác nhận cả ba Slave khởi động relay OFF, LCD hiện đúng N1/N2/N3.
2. Đợi Master online đủ ba máy; PB0 chọn node, PB1 chỉ tác động relay node đó.
3. Thử bật/tắt riêng từng bơm qua ThingsBoard, kiểm tra relay thật và telemetry đúng ID.
4. Khi relay đang ON rồi OFF, lần lượt ngắt RF/ESP32/Wi-Fi: đầu ra phải giữ nguyên. Kết nối lại phải đồng bộ trạng thái, không tự chạy lệnh cũ.
5. Thử nút tại Slave khi một lệnh đang chờ; kết quả có thể CONFLICT/OVERRIDDEN, không thực thi lại lệnh cũ để ghi đè thao tác tại chỗ.
6. Reset Master giữa lệnh: lệnh cũ không được tự gửi lại sang phiên mới. Reset Slave vẫn đi qua boot OFF theo thiết kế.
7. Giữ D27 dưới 5 giây không mở portal; giữ đủ 5 giây mở đúng một lần, UART vẫn cập nhật. Lưu Wi-Fi và reset ESP32 để kiểm tra dùng lại mạng đã lưu.
8. Rút SHT30/LCD: relay/heartbeat vẫn hoạt động, dữ liệu lỗi có cờ valid; ADC lỗi không biến thành đất ẩm 100%.

Kiểm tra logic trên host (cần gcc/g++, Python; boot journal test dùng Linux):

```bash
python tools/test_all.py
```

Đây là kiểm tra parser, giao dịch, driver với HAL giả và journal Flash, không phải phép đo radio, tiếp điểm relay hoặc bơm thực tế. `release/VALIDATION.md` phân biệt các kết quả đã chạy và phần còn cần mạch.
