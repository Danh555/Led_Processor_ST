#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\include\\app_license.h"
#ifndef APP_LICENSE_H
#define APP_LICENSE_H

#include <Preferences.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

class LicenseManagerApp
{
public:
    LicenseManagerApp();
    void begin();
    void handle();
    void update();
    unsigned long getRemainingSeconds();
    bool isLicenseExpired();

private:
    WebServer server;
    WebSocketsServer webSocket = WebSocketsServer(81);
    unsigned long licenseStartTime;
    bool licenseActive;
    unsigned long licenseEndTime_ = 0;
    uint32_t remainingSeconds_ = 0;
    uint8_t licenseMinutes_ = 0;
    unsigned long lastUpdateTime_ = 0;
    bool licenseExpired_ = false;
    Preferences preferences;

    void handleRoot();
    void handleConfig();
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    void broadcastRemainingTime();
};

#endif
