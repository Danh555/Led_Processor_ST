#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\include\\mac.h"
#ifndef MAC_H
#define MAC_H

#include <Arduino.h>
#include <WiFi.h> // Cần để sử dụng ESP.getEfuseMac() trên ESP32

/**
 * @brief Lấy địa chỉ MAC dưới dạng chuỗi định dạng XX:XX:XX:XX:XX:XX.
 * @return Chuỗi chứa địa chỉ MAC.
 */
String getMacString()
{
    uint64_t mac64 = ESP.getEfuseMac();
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            (uint8_t)(mac64 >> 40), (uint8_t)(mac64 >> 32),
            (uint8_t)(mac64 >> 24), (uint8_t)(mac64 >> 16),
            (uint8_t)(mac64 >> 8), (uint8_t)(mac64));
    return String(buf);
}

#endif // MAC_H