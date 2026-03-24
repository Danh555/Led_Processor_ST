#include <Arduino.h> 
#include "app_license.h"
#ifndef CONFIG_H 
#define CONFIG_H

// Định nghĩa các chân GPIO cho giao tiếp RS485
// #define RS485_RX_PIN 20 // Chân nhận dữ liệu (RX) cho cổng RS485 (Serial0)
// #define RS485_TX_PIN 21 // Chân truyền dữ liệu (TX) cho cổng RS485 (Serial0) - 

// Định nghĩa các chân GPIO cho cổng Serial1
#define RS232_RX_PIN 8 // Chân nhận dữ liệu (RX) cho cổng RS232 (Serial1)
#define RS232_TX_PIN 7 // Chân truyền dữ liệu (TX) cho cổng RS232 (Serial1) 

#define SEND_DATA_NODE 2000 // Khoảng thời gian gửi dữ liệu qua RS485 

// #define SERIAL_HEX Serial   // Cổng Serial chính giao tiếp gói tin với Pc
#define SERIAL Serial   // Cổng Serial chính giao tiếp gói tin với Pc
// #define RS485 Serial0       // Cổng Serial0 dùng cho giao tiếp RS485 với module (tạm thời không sử dụng)
// #define SERIAL_DATA Serial1 // Cổng Serial1 dùng để truyền dữ liệu với thiết bị khác
#define SERIAL_RS232 Serial1 // Cổng Serial1 dùng để truyền dữ liệu với thiết bị khác

#define TIMEOUT_INTERVAL 20000 // Thời gian timeout  cho các hoạt động giao tiếp

#define LED_PIN 2 // Chân GPIO điều khiển LED (thường dùng để báo trạng thái)

// extern bool allowTransmission; // Biến toàn cục để bật/tắt truyền dữ liệu (khởi tạo trong config.cpp)
extern uint8_t licenseByte;
extern uint32_t licenseDuration;
extern uint8_t licenseMode_;
extern bool transferMode_ ;
extern unsigned long licenseStartUnix_ ;
extern bool pendingLicenseBrightness50_;
extern bool allowOneLicenseBrightnessUpdate_;
extern LicenseManagerApp licenseManager;
#endif // CONFIG_H
