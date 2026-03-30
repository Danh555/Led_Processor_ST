#line 1 "D:\\Alta\\LedPr_v2\\LedPr_v2\\README.md"
# LedPr_v2

Firmware cho ESP32-C3 dung de:

- Phat Wi-Fi AP cau hinh noi bo.
- Cung cap Web UI va WebSocket de cau hinh license theo thoi gian thuc.
- Nhan firmware OTA qua WebSocket.
- Dieu khien thiet bi LED/EX Series qua `Serial1` (RS232).

## Chuc nang tong quat cua code

Code duoc chia thanh 2 khoi chinh:

- `LicenseManagerApp`: quan ly Wi-Fi AP, Web UI, WebSocket, OTA va bo dem thoi gian license.
- `LedProcessor`: giao tiep voi thiet bi EX/LED qua `Serial1`, gui lenh brightness/contrast, giam sat timeout va tu dong thu lai baud khi mat ket noi.

Luong chay chinh:

1. Khi khoi dong, ESP32 nap cau hinh license va baud da luu trong NVS.
2. Bat Wi-Fi AP, HTTP server va WebSocket server de cho phep cau hinh tu trinh duyet.
3. Dinh ky cap nhat thoi gian license con lai va broadcast trang thai cho client.
4. Ap trang thai license len thiet bi LED qua `Serial1`.
5. Neu can, nhan firmware moi va cap nhat OTA qua WebSocket.

Khi license het han, firmware se ap che do xu ly theo `mode` da cau hinh. Trong implementation hien tai, he thong uu tien viec ep trang thai hien thi cua thiet bi thong qua lenh brightness/contrast va duy tri giao tiep on dinh voi thiet bi EX.

## Cac ham chinh

### Trong `LedPr_v2.ino`

- `setup()`: khoi tao `Serial`, `ledProcessor` va `licenseManager`.
- `loop()`: vong lap chinh, goi `licenseManager.handle()`, `licenseManager.update()` va `ledProcessor.process()`.

### Trong `LicenseManagerApp`

- `begin()`: doc cau hinh license tu NVS, bat Wi-Fi AP, khoi tao HTTP server va WebSocket server.
- `handle()`: xu ly request HTTP va su kien WebSocket trong moi vong lap.
- `update()`: cap nhat bo dem license theo thoi gian, broadcast trang thai dinh ky va reboot sau OTA neu can.
- `webSocketEvent(...)`: xu ly message WebSocket, gom cau hinh license, `test_license`, `ota_begin`, `ota_end` va du lieu OTA binary.
- `broadcastRemainingTime()`: gui JSON thong bao `remaining` va `expired` cho cac client dang ket noi.
- `handleRoot()`: tra trang Web UI chinh de cau hinh license.
- `handleOtaPage()`: tra trang Web UI OTA de upload firmware `.bin`.
- `handleConfig()`: nhan cau hinh license qua HTTP POST.
- `getRemainingSeconds()`: tra ve so giay license con lai.
- `isLicenseExpired()`: cho biet license da het han hay chua.

### Trong `LedProcessor`

- `begin()`: khoi tao `Serial1`, nap baud da luu va tao task LED bao trang thai.
- `process()`: ham xu ly chinh cua khoi dieu khien LED, kiem tra license, gui lenh xuong thiet bi va giam sat mat ket noi.
- `resetSavedBaudRate()`: xoa baud da luu trong NVS va quay ve baud mac dinh.
- `forceBrightnessWithBaudFallback(...)`: thu gui lenh brightness tren baud uu tien, neu that bai thi thu lai o baud khac.
- `recoverCommunicationWithBaudFallback(...)`: quet lai baud va dong bo lai trang thai khi thiet bi mat phan hoi.
- `updateCommunicationHealth(...)`: dem so lan mat phan hoi lien tiep va kich hoat co che khoi phuc ket noi.

## Tong quan giao tiep

- HTTP server: cong `80`
- WebSocket server: cong `81`
- Wi-Fi AP:
  - SSID: `LVP_APP_ALTA`
  - Password: `12345678`
- Trang web chinh: `http://192.168.4.1/`
- Trang OTA: `http://192.168.4.1/OTA`

Ghi chu: dia chi `192.168.4.1` la IP mac dinh cua `WiFi.softAP()` tren ESP32.

## WebSocket dung de lam gi

WebSocket trong du an nay co 2 chuc nang chinh:

- Cap nhat license realtime ma khong can reload trang.
- Truyen file firmware `.bin` de OTA.

Firmware broadcast trang thai license dinh ky cho moi client dang ket noi. Khi co thay doi cau hinh, thiet bi tra phan hoi JSON ngay tren chinh socket do.

## Cac ban tin tu thiet bi

### 1. Broadcast thoi gian con lai

Thiet bi gui dinh ky khoang 1 giay/lan khi co client ket noi:

```json
{
  "type": "update_remaining",
  "remaining": 3661,
  "expired": false
}
```

Y nghia:

- `type`: loai ban tin
- `remaining`: so giay license con lai
- `expired`: `true` neu license da het han

## Luu y khi dung WebSocket

- Payload text toi da la `512` byte.
- JSON sai dinh dang se bi bo qua.
- Chi mot phien OTA duoc phep chay tai mot thoi diem.
- Neu client dang OTA bi ngat ket noi, firmware se huy OTA.
- Broadcast `update_remaining` chi gui khi co it nhat mot client dang ket noi.

## Luong su dung thuc te

1. Cap nguon cho ESP32-C3.
2. Ket noi Wi-Fi toi AP `LVP_APP_ALTA`.
3. Mo `http://192.168.4.1/`.
4. Trang web se tu mo WebSocket toi cong `81`.
5. Chon `mode`, nhap gio/phut, bam `Apply`.
6. Theo doi trang thai license realtime ngay tren web.
7. Neu can cap nhat firmware, mo `/OTA` va upload file `.bin`.

## File lien quan

- [`src/app_license.cpp`](D:/LedProcessor/LedPr_v2/LedPr_v2/src/app_license.cpp): xu ly WebSocket, Web UI, OTA
- [`include/app_license.h`](D:/LedProcessor/LedPr_v2/LedPr_v2/include/app_license.h): khai bao `WebServer` va `WebSocketsServer`
- [`include/LedProcessor.h`](D:/LedProcessor/LedPr_v2/LedPr_v2/include/LedProcessor.h): xu ly dieu khien thiet bi EX/LED qua `Serial1`
