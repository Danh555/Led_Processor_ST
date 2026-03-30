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
    static const uint32_t LICENSE_TICK_MS = 1000UL;
    static const uint32_t LICENSE_PERSIST_INTERVAL_MS = 60000UL;
    static const size_t MAX_WS_TEXT_SIZE = 512;

    WebServer server;
    WebSocketsServer webSocket = WebSocketsServer(81);
    unsigned long licenseStartTime;
    bool licenseActive;
    unsigned long licenseEndTime_ = 0;
    uint32_t remainingSeconds_ = 0;
    uint8_t licenseMinutes_ = 0;
    unsigned long lastUpdateTime_ = 0;
    bool licenseExpired_ = false;
    bool otaInProgress_ = false;
    bool otaPendingReboot_ = false;
    uint8_t otaClientId_ = 0xFF;
    uint32_t otaExpectedSize_ = 0;
    uint32_t otaReceivedSize_ = 0;
    unsigned long otaRebootAt_ = 0;
    Preferences preferences;

    void handleRoot();
    void handleOtaPage();
    void handleConfig();
    void persistRemainingSeconds(bool force);
    void expireLicense();
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    void broadcastRemainingTime();
    void activateTestLicense(uint8_t mode, uint32_t durationSeconds); //chức năng test license
};

#endif
