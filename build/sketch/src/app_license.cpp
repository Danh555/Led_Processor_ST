#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\src\\app_license.cpp"
#include "include/app_license.h"
#include "include/config.h"
#include "WiFi.h"
#include <ArduinoJson.h>

const char *ssid = "LVP_APP_ALTA";
const char *password = "12345678";

WebSocketsServer webSocket = WebSocketsServer(81);

LicenseManagerApp::LicenseManagerApp() : server(80)
{
    licenseStartTime = 0;
    licenseActive = false;
    remainingSeconds_ = 0;
    licenseExpired_ = true;
}

void LicenseManagerApp::begin()
{
    preferences.begin("lic", false);
    licenseMode_ = preferences.getUChar("license_mode", 1);
    licenseDuration = preferences.getInt("license_dur", 0);
    licenseMinutes_ = preferences.getUChar("license_min", 0);
    remainingSeconds_ = preferences.getUInt("license_timer", 0);
    transferMode_ = preferences.getBool("transfer_mode", false);

    licenseExpired_ = (remainingSeconds_ == 0);

    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP(ssid, password);
    WiFi.setTxPower(WIFI_POWER_15dBm);

    if (remainingSeconds_ > 0 && !licenseExpired_)
    {
        licenseEndTime_ = millis() + (remainingSeconds_ * 1000UL);
        licenseActive = true;
    }
    else
    {
        licenseExpired_ = true;
        licenseActive = false;
    }

    lastUpdateTime_ = millis();
    preferences.end();

    Serial.printf("[LICENSE] Khoi tao mode=%u duration=%luh %um remaining=%lus transfer=%s expired=%s\r\n",
                  licenseMode_,
                  (unsigned long)licenseDuration,
                  licenseMinutes_,
                  (unsigned long)remainingSeconds_,
                  transferMode_ ? "ON" : "OFF",
                  licenseExpired_ ? "YES" : "NO");

    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
                      { webSocketEvent(num, type, payload, length); });

    server.on("/", [this]()
              { handleRoot(); });
    server.on("/config", HTTP_POST, [this]()
              { handleConfig(); });
    server.begin();
}

void LicenseManagerApp::webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        break;
    case WStype_CONNECTED:
        broadcastRemainingTime();
        break;
    case WStype_TEXT:
    {
        char dataStr[length + 1];
        memcpy(dataStr, payload, length);
        dataStr[length] = '\0';

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, dataStr);
        if (error)
        {
            Serial.println("[LICENSE] Loi parse JSON tu WebSocket");
            break;
        }

        int newLicenseMode = doc["mode"].as<int>();
        if (newLicenseMode < 1 || newLicenseMode > 3)
        {
            newLicenseMode = 1;
        }
        int newLicenseDuration = doc["duration"].as<int>();
        if (newLicenseDuration < 0)
        {
            newLicenseDuration = 0;
        }
        int newLicenseMinutes = doc["duration_minutes"].as<int>();
        if (newLicenseMinutes < 0)
        {
            newLicenseMinutes = 0;
        }
        if (newLicenseMinutes > 59)
        {
            newLicenseMinutes = 59;
        }
        bool newTransferMode = doc["transfer"].as<bool>();

        Serial.printf("[LICENSE] Nhan config WS mode=%d duration=%dh %dm transfer=%s\r\n",
                      newLicenseMode,
                      newLicenseDuration,
                      newLicenseMinutes,
                      newTransferMode ? "ON" : "OFF");

        preferences.begin("lic", false);
        licenseMode_ = newLicenseMode;
        transferMode_ = newTransferMode;
        licenseDuration = newLicenseDuration;
        licenseMinutes_ = static_cast<uint8_t>(newLicenseMinutes);

        preferences.putUChar("license_mode", licenseMode_);
        preferences.putBool("transfer_mode", transferMode_);
        preferences.putInt("license_dur", licenseDuration);
        preferences.putUChar("license_min", licenseMinutes_);

        unsigned long totalDurationSeconds = (static_cast<unsigned long>(licenseDuration) * 3600UL) +
                                             (static_cast<unsigned long>(licenseMinutes_) * 60UL);

        if (totalDurationSeconds == 0)
        {
            remainingSeconds_ = 0;
            licenseEndTime_ = 0;
            licenseExpired_ = true;
            licenseActive = false;
            transferMode_ = false;
            preferences.putBool("transfer_mode", false);
            preferences.putUInt("license_timer", 0);
            Serial.println("[LICENSE] WS config dat duration=0, license het han");
        }
        else
        {
            remainingSeconds_ = totalDurationSeconds;
            licenseEndTime_ = millis() + (remainingSeconds_ * 1000UL);
            licenseExpired_ = false;
            licenseActive = true;
            pendingLicenseBrightness50_ = true;
            allowOneLicenseBrightnessUpdate_ = true;
            preferences.putUInt("license_timer", remainingSeconds_);
            Serial.printf("[LICENSE] Kich hoat license moi: remaining=%lus mode=%u transfer=%s (%luh %um)\r\n",
                          (unsigned long)remainingSeconds_,
                          licenseMode_,
                          transferMode_ ? "ON" : "OFF",
                          (unsigned long)licenseDuration,
                          licenseMinutes_);
        }

        preferences.end();
        licenseStartTime = millis();
        lastUpdateTime_ = millis();

        DynamicJsonDocument response(160);
        response["status"] = "ok";
        response["remaining"] = remainingSeconds_;
        response["expired"] = licenseExpired_;
        String jsonResponse;
        serializeJson(response, jsonResponse);
        webSocket.sendTXT(num, jsonResponse);
        broadcastRemainingTime();
        break;
    }
    default:
        break;
    }
}

void LicenseManagerApp::broadcastRemainingTime()
{
    if (webSocket.connectedClients() > 0)
    {
        DynamicJsonDocument broadcast(96);
        broadcast["type"] = "update_remaining";
        broadcast["remaining"] = getRemainingSeconds();
        broadcast["expired"] = licenseExpired_;
        String jsonBroadcast;
        serializeJson(broadcast, jsonBroadcast);
        webSocket.broadcastTXT(jsonBroadcast);
    }
}

void LicenseManagerApp::handle()
{
    server.handleClient();
    webSocket.loop();
}

void LicenseManagerApp::handleRoot()
{
    unsigned long currentRemaining = getRemainingSeconds();
    unsigned long hours = currentRemaining / 3600UL;
    unsigned long minutes = (currentRemaining % 3600UL) / 60UL;
    unsigned long seconds = currentRemaining % 60UL;

    String timeDisplay =
        String(hours) + " gio " +
        (minutes < 10 ? "0" : "") + String(minutes) + " phut " +
        (seconds < 10 ? "0" : "") + String(seconds) + " giay";

    String statusText = licenseExpired_
                            ? "License het han"
                            : "Thoi gian con lai: " + timeDisplay;

    String wsScript = R"RAW(
    <script>
        const ws = new WebSocket('ws://' + window.location.hostname + ':81/');
        let statusDiv = document.getElementById('status');

        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.type === 'update_remaining') {
                if (data.expired) {
                    statusDiv.innerHTML = 'Mode: )RAW" + String(licenseMode_) + R"RAW( | License het han | Chuyen doi: )RAW" + String(transferMode_ ? "Bat" : "Tat") + R"RAW(';
                    statusDiv.style.color = '#ff0000';
                } else {
                    const hours = Math.floor(data.remaining / 3600);
                    const minutes = Math.floor((data.remaining % 3600) / 60);
                    const seconds = data.remaining % 60;
                    const timeDisplay = hours + ' gio ' + (minutes < 10 ? '0' : '') + minutes + ' phut ' + (seconds < 10 ? '0' : '') + seconds + ' giay';
                    statusDiv.innerHTML = 'Mode: )RAW" + String(licenseMode_) + R"RAW( | Thoi gian con lai: ' + timeDisplay + ' | Chuyen doi: )RAW" + String(transferMode_ ? "Bat" : "Tat") + R"RAW(';
                    statusDiv.style.color = '#333333';
                }
            } else if (data.status === 'ok') {
                alert('Cap nhat license thanh cong');
            }
        };

        document.querySelector('form').onsubmit = function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const config = {
                mode: parseInt(formData.get('mode')),
                duration: parseInt(formData.get('duration')),
                duration_minutes: parseInt(formData.get('duration_minutes')) || 0,
                transfer: formData.get('transfer') ? true : false
            };
            ws.send(JSON.stringify(config));
            return false;
        };
    </script>
    )RAW";

    String html = R"RAW(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 License Config</title>
<meta charset="UTF-8">
<style>
body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: linear-gradient(135deg, #ff9e5e, #ffd1dc);
    background-repeat: no-repeat;
    color: #333333;
    margin: 0;
    padding: 20px;
    box-sizing: border-box;
    text-align: center;
}
h1 {
    font-size: 2.5rem;
    margin-bottom: 20px;
    color: #ff6b33;
}
form {
    background: rgba(255, 255, 255, 0.7);
    backdrop-filter: blur(8px);
    padding: 30px;
    border-radius: 12px;
    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.15);
    max-width: 400px;
    margin: 0 auto;
}
label {
    font-size: 1.1rem;
    margin-bottom: 10px;
    display: block;
    text-align: left;
    padding: 5px;
    color: #555555;
}
select, input[type="number"] {
    width: 100%;
    padding: 12px;
    margin-bottom: 20px;
    border: 1px solid #ffe6cc;
    border-radius: 8px;
    background: rgba(255, 245, 235, 0.9);
    color: #333333;
    font-size: 1rem;
}
input[type="submit"] {
    width: 100%;
    padding: 12px;
    background: linear-gradient(45deg, #ff6b33, #ffadad);
    border: none;
    border-radius: 8px;
    color: #ffffff;
    font-size: 1.1rem;
    cursor: pointer;
}
.switch {
  font-size: 17px;
  position: relative;
  display: inline-block;
  width: 3.5em;
  height: 1.93em;
}
.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}
.slider {
  position: absolute;
  cursor: pointer;
  inset: 0;
  background: #e7d0b9ff;
  border-radius: 50px;
}
.slider:before {
  position: absolute;
  content: "";
  height: 2em;
  width: 2em;
  left: 0.3em;
  bottom: 0.3em;
  background-color: white;
  border-radius: 50px;
  box-shadow: 0 0px 20px rgba(0,0,0,0.4);
  transition: all 0.3s ease;
}
.switch input:checked + .slider {
  background: #f07115ff;
}
.switch input:checked + .slider:before {
  transform: translateX(1.6em);
}
#status {
    background: rgba(255, 255, 255, 0.7);
    padding: 15px;
    border-radius: 8px;
    max-width: 400px;
    margin: 20px auto;
    font-size: 1rem;
    box-shadow: 0 4px 15px rgba(0, 0, 0, 0.15);
    color: )RAW" + String(licenseExpired_ ? "#ff0000" : "#333333") + R"RAW(;
    font-weight: bold;
}
</style>
</head>
<body>
  <h1>Cau hinh License cho ESP32-C3</h1>
  <form>
    <label>Mode License:</label><br>
    <select name="mode">
      <option value="1" )RAW" + String(licenseMode_ == 1 ? "selected" : "") + R"RAW(>Mode 1: Tat LED</option>
      <option value="2" )RAW" + String(licenseMode_ == 2 ? "selected" : "") + R"RAW(>Mode 2: Hong 20% LED</option>
      <option value="3" )RAW" + String(licenseMode_ == 3 ? "selected" : "") + R"RAW(>Mode 3: Nhay RGB ngau nhien</option>
    </select><br><br>

    <label>Thoi gian License (gio):</label><br>
    <input type="number" name="duration" min="0" max="525600" value=")RAW" + String(licenseDuration) + R"RAW("><br><br>

    <label>Thoi gian License (phut):</label><br>
    <input type="number" name="duration_minutes" min="0" max="59" value=")RAW" + String(licenseMinutes_) + R"RAW("><br><br>

    <label>Cho phep xuat du lieu:</label><br>
    <label class="switch">
      <input type="checkbox" name="transfer" )RAW" + String(transferMode_ ? "checked" : "") + R"RAW(>
      <span class="slider"></span>
    </label><br><br>

    <input type="submit" value="Ap dung Realtime (WebSocket)">
  </form>

  <h2>Trang thai hien tai:</h2>
  <div id="status">Mode: )RAW" + String(licenseMode_) + R"RAW( | )RAW" + statusText + R"RAW( | Chuyen doi duong truyen: )RAW" + String(transferMode_ ? "Bat" : "Tat") + R"RAW(</div>

  )RAW" + wsScript + R"RAW(
</body></html>
)RAW";

    server.send(200, "text/html", html);
}

void LicenseManagerApp::handleConfig()
{
    preferences.begin("lic", false);

    int newLicenseMode = server.arg("mode").toInt();
    if (newLicenseMode < 1 || newLicenseMode > 3)
    {
        newLicenseMode = 1;
    }
    int newLicenseDuration = server.arg("duration").toInt();
    if (newLicenseDuration < 0)
    {
        newLicenseDuration = 0;
    }
    int newLicenseMinutes = server.arg("duration_minutes").toInt();
    if (newLicenseMinutes < 0)
    {
        newLicenseMinutes = 0;
    }
    if (newLicenseMinutes > 59)
    {
        newLicenseMinutes = 59;
    }
    bool newTransferMode = server.hasArg("transfer");

    Serial.printf("[LICENSE] Nhan config HTTP mode=%d duration=%dh %dm transfer=%s\r\n",
                  newLicenseMode,
                  newLicenseDuration,
                  newLicenseMinutes,
                  newTransferMode ? "ON" : "OFF");

    licenseMode_ = newLicenseMode;
    transferMode_ = newTransferMode;
    licenseDuration = newLicenseDuration;
    licenseMinutes_ = static_cast<uint8_t>(newLicenseMinutes);

    preferences.putUChar("license_mode", licenseMode_);
    preferences.putBool("transfer_mode", transferMode_);
    preferences.putInt("license_dur", licenseDuration);
    preferences.putUChar("license_min", licenseMinutes_);

    unsigned long totalDurationSeconds = (static_cast<unsigned long>(licenseDuration) * 3600UL) +
                                         (static_cast<unsigned long>(licenseMinutes_) * 60UL);

    if (totalDurationSeconds == 0)
    {
        remainingSeconds_ = 0;
        licenseEndTime_ = 0;
        licenseExpired_ = true;
        licenseActive = false;
        transferMode_ = false;
        preferences.putBool("transfer_mode", false);
        preferences.putUInt("license_timer", 0);
        Serial.println("[LICENSE] HTTP config dat duration=0, license het han");
    }
    else
    {
        remainingSeconds_ = totalDurationSeconds;
        licenseEndTime_ = millis() + (remainingSeconds_ * 1000UL);
        licenseExpired_ = false;
        licenseActive = true;
        pendingLicenseBrightness50_ = true;
        allowOneLicenseBrightnessUpdate_ = true;
        preferences.putUInt("license_timer", remainingSeconds_);
        Serial.printf("[LICENSE] Kich hoat license moi: remaining=%lus mode=%u transfer=%s (%luh %um)\r\n",
                      (unsigned long)remainingSeconds_,
                      licenseMode_,
                      transferMode_ ? "ON" : "OFF",
                      (unsigned long)licenseDuration,
                      licenseMinutes_);
    }

    preferences.end();
    licenseStartTime = millis();
    lastUpdateTime_ = millis();

    server.sendHeader("Location", "/");
    server.send(302);
}

void LicenseManagerApp::update()
{
    static unsigned long lastLicenseDebugLog = 0;
    static bool lastExpiredState = true;

    if (licenseActive && millis() >= licenseEndTime_)
    {
        remainingSeconds_ = 0;
        licenseEndTime_ = 0;
        licenseExpired_ = true;
        licenseActive = false;
        preferences.begin("lic", false);
        preferences.putUInt("license_timer", 0);
        preferences.putBool("transfer_mode", false);
        transferMode_ = false;
        preferences.end();
        Serial.println("[LICENSE] Da het han, tat transfer_mode va reset bo dem");
    }
    else if (licenseActive)
    {
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime_ >= 1000UL)
        {
            remainingSeconds_ = (licenseEndTime_ - currentTime) / 1000UL;
            lastUpdateTime_ = currentTime;
            if (remainingSeconds_ > 0)
            {
                preferences.begin("lic", false);
                preferences.putUInt("license_timer", remainingSeconds_);
                preferences.end();
            }
            else
            {
                licenseActive = false;
                licenseExpired_ = true;
                preferences.begin("lic", false);
                preferences.putUInt("license_timer", 0);
                preferences.end();
            }
        }
    }

    if (licenseExpired_ != lastExpiredState)
    {
        if (licenseExpired_)
        {
            Serial.println("[LICENSE] Trang thai: HET LICENSE");
        }
        else
        {
            Serial.printf("[LICENSE] Trang thai: CON LICENSE, remaining=%lus (%luh %lum %lus)\r\n",
                          (unsigned long)remainingSeconds_,
                          (unsigned long)(remainingSeconds_ / 3600UL),
                          (unsigned long)((remainingSeconds_ % 3600UL) / 60UL),
                          (unsigned long)(remainingSeconds_ % 60UL));
        }
        lastExpiredState = licenseExpired_;
        lastLicenseDebugLog = millis();
    }

    // if (!licenseExpired_ && millis() - lastLicenseDebugLog >= 10000UL)
    // {
    //     Serial.printf("[LICENSE] CON LICENSE, remaining=%lus (%luh %lum %lus), mode=%u, transfer=%s\r\n",
    //                   (unsigned long)remainingSeconds_,
    //                   (unsigned long)(remainingSeconds_ / 3600UL),
    //                   (unsigned long)((remainingSeconds_ % 3600UL) / 60UL),
    //                   (unsigned long)(remainingSeconds_ % 60UL),
    //                   licenseMode_,
    //                   transferMode_ ? "ON" : "OFF");
    //     lastLicenseDebugLog = millis();
    // }

    static unsigned long lastBroadcast = 0;
    if (millis() - lastBroadcast >= 1000UL && webSocket.connectedClients() > 0)
    {
        broadcastRemainingTime();
        lastBroadcast = millis();
    }
}

unsigned long LicenseManagerApp::getRemainingSeconds()
{
    return remainingSeconds_;
}

bool LicenseManagerApp::isLicenseExpired()
{
    return licenseExpired_;
}
