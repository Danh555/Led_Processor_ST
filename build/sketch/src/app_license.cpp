#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\src\\app_license.cpp"
#include "include/app_license.h"
#include "include/config.h"
#include "WiFi.h"
#include <ArduinoJson.h>
#include <Update.h>

const char *ssid = "LVP_APP_ALTA";
const char *password = "12345678";

LicenseManagerApp::LicenseManagerApp() : server(80)
{
    licenseStartTime = 0;
    licenseActive = false;
    remainingSeconds_ = 0;
    licenseExpired_ = true;
}

void LicenseManagerApp::persistRemainingSeconds(bool force)
{
    static uint32_t lastPersistedSeconds = 0xFFFFFFFFUL;
    static unsigned long lastPersistAt = 0;

    unsigned long now = millis();
    if (!force)
    {
        if (remainingSeconds_ == lastPersistedSeconds)
        {
            return;
        }

        if ((now - lastPersistAt) < LICENSE_PERSIST_INTERVAL_MS)
        {
            return;
        }
    }

    preferences.begin("lic", false);
    preferences.putUInt("license_timer", remainingSeconds_);
    preferences.end();

    lastPersistedSeconds = remainingSeconds_;
    lastPersistAt = now;
}

void LicenseManagerApp::expireLicense()
{
    remainingSeconds_ = 0;
    licenseEndTime_ = 0;
    licenseExpired_ = true;
    licenseActive = false;
}

void LicenseManagerApp::begin()
{
    preferences.begin("lic", false);
    licenseMode_ = preferences.getUChar("license_mode", 1);
    licenseDuration = preferences.getInt("license_dur", 0);
    licenseMinutes_ = preferences.getUChar("license_min", 0);
    remainingSeconds_ = preferences.getUInt("license_timer", 0);
    // transferMode_ = preferences.getBool("transfer_mode", false);

    licenseExpired_ = (remainingSeconds_ == 0);

    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP(ssid, password);
    WiFi.setTxPower(WIFI_POWER_15dBm);

    if (remainingSeconds_ > 0 && !licenseExpired_)
    {
        licenseEndTime_ = 0;
        licenseActive = true;
    }
    else
    {
        expireLicense();
        preferences.putUInt("license_timer", 0);
    }

    lastUpdateTime_ = millis();
    preferences.end();

    Serial.printf("[LICENSE] Khoi tao mode=%u duration=%luh %um remaining=%lus expired=%s\r\n",
                  licenseMode_,
                  (unsigned long)licenseDuration,
                  licenseMinutes_,
                  (unsigned long)remainingSeconds_,
                  licenseExpired_ ? "YES" : "NO");

    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
                      { webSocketEvent(num, type, payload, length); });

    server.on("/", [this]()
              { handleRoot(); });
    server.on("/OTA", [this]()
              { handleOtaPage(); });
    server.on("/ota", [this]()
              { handleOtaPage(); });
    server.on("/config", HTTP_POST, [this]()
              { handleConfig(); });
    server.begin();
}

void LicenseManagerApp::activateTestLicense(uint8_t mode, uint32_t durationSeconds)
{
    if (mode < 1 || mode > 3)
    {
        mode = 1;
    }

    preferences.begin("lic", false);
    licenseMode_ = mode;
    licenseDuration = 0;
    licenseMinutes_ = 0;
    remainingSeconds_ = durationSeconds;
    licenseEndTime_ = 0;
    licenseExpired_ = false;
    licenseActive = true;
    pendingLicenseBrightness50_ = true;
    allowOneLicenseBrightnessUpdate_ = true;

    preferences.putUChar("license_mode", licenseMode_);
    preferences.putInt("license_dur", licenseDuration);
    preferences.putUChar("license_min", licenseMinutes_);
    preferences.putUInt("license_timer", remainingSeconds_);
    preferences.end();

    licenseStartTime = millis();
    lastUpdateTime_ = millis();
    persistRemainingSeconds(true);

    Serial.printf("[LICENSE][TEST] Kich hoat test license %lus, mode=%u\r\n",
                  (unsigned long)remainingSeconds_,
                  licenseMode_);
}

void LicenseManagerApp::webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        if (otaInProgress_ && num == otaClientId_)
        {
            Update.abort();
            otaInProgress_ = false;
            otaClientId_ = 0xFF;
            otaExpectedSize_ = 0;
            otaReceivedSize_ = 0;
            Serial.println("[OTA] Huy OTA do mat ket noi WebSocket");
        }
        break;
    case WStype_CONNECTED:
        broadcastRemainingTime();
        break;
    case WStype_TEXT:
    {
        if (length == 0 || length > MAX_WS_TEXT_SIZE)
        {
            DynamicJsonDocument response(160);
            response["status"] = "error";
            response["message"] = "payload_too_large";
            response["max"] = MAX_WS_TEXT_SIZE;
            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            Serial.printf("[WS] Tu choi TEXT payload size=%u\r\n", static_cast<unsigned>(length));
            break;
        }

        char dataStr[MAX_WS_TEXT_SIZE + 1];
        memcpy(dataStr, payload, length);
        dataStr[length] = '\0';

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, dataStr);
        if (error)
        {
            Serial.println("[LICENSE] Loi parse JSON tu WebSocket");
            break;
        }

        const char *action = doc["action"] | "";
        if (strcmp(action, "ota_begin") == 0)
        {
            uint32_t firmwareSize = doc["size"].as<uint32_t>();
            DynamicJsonDocument response(192);

            if (firmwareSize == 0)
            {
                response["status"] = "error";
                response["message"] = "ota_invalid_size";
            }
            else if (otaInProgress_)
            {
                response["status"] = "error";
                response["message"] = "ota_busy";
            }
            else if (!Update.begin(firmwareSize))
            {
                response["status"] = "error";
                response["message"] = "ota_begin_failed";
                response["detail"] = Update.errorString();
                Serial.printf("[OTA] Khong the bat dau OTA, size=%lu, err=%s\r\n",
                              (unsigned long)firmwareSize,
                              Update.errorString());
            }
            else
            {
                otaInProgress_ = true;
                otaClientId_ = num;
                otaExpectedSize_ = firmwareSize;
                otaReceivedSize_ = 0;
                response["status"] = "ok";
                response["message"] = "ota_begin_ok";
                response["expected"] = otaExpectedSize_;
                Serial.printf("[OTA] Bat dau OTA qua WebSocket, size=%lu\r\n",
                              (unsigned long)otaExpectedSize_);
            }

            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            break;
        }

        if (strcmp(action, "ota_end") == 0)
        {
            DynamicJsonDocument response(224);

            if (!otaInProgress_ || num != otaClientId_)
            {
                response["status"] = "error";
                response["message"] = "ota_not_active";
            }
            else if (otaReceivedSize_ != otaExpectedSize_)
            {
                uint32_t receivedSize = otaReceivedSize_;
                uint32_t expectedSize = otaExpectedSize_;
                Update.abort();
                otaInProgress_ = false;
                otaClientId_ = 0xFF;
                otaExpectedSize_ = 0;
                otaReceivedSize_ = 0;
                response["status"] = "error";
                response["message"] = "ota_size_mismatch";
                response["received"] = receivedSize;
                response["expected"] = expectedSize;
                Serial.printf("[OTA] OTA size mismatch, received=%lu expected=%lu\r\n",
                              (unsigned long)receivedSize,
                              (unsigned long)expectedSize);
            }
            else if (!Update.end(true))
            {
                response["status"] = "error";
                response["message"] = "ota_end_failed";
                response["detail"] = Update.errorString();
                otaInProgress_ = false;
                otaClientId_ = 0xFF;
                otaExpectedSize_ = 0;
                otaReceivedSize_ = 0;
                Serial.printf("[OTA] Ket thuc OTA that bai, err=%s\r\n", Update.errorString());
            }
            else
            {
                otaInProgress_ = false;
                otaPendingReboot_ = true;
                otaRebootAt_ = millis() + 1500UL;
                response["status"] = "ok";
                response["message"] = "ota_success";
                response["received"] = otaReceivedSize_;
                Serial.printf("[OTA] OTA thanh cong, received=%lu. Se khoi dong lai.\r\n",
                              (unsigned long)otaReceivedSize_);
            }

            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            break;
        }

        if (strcmp(action, "test_license") == 0)
        {
            uint8_t requestedMode = doc["mode"].is<int>() ? static_cast<uint8_t>(doc["mode"].as<int>()) : licenseMode_;
            activateTestLicense(requestedMode, 20UL);  //test license 20s

            DynamicJsonDocument response(160);
            response["status"] = "ok";
            response["message"] = "test_license_started";
            response["remaining"] = remainingSeconds_;
            response["expired"] = licenseExpired_;
            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            broadcastRemainingTime();
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
        // bool newTransferMode = doc["transfer"].as<bool>();

        Serial.printf("[LICENSE] Nhan config WS mode=%d duration=%dh %dm\r\n",
                      newLicenseMode,
                      newLicenseDuration,
                      newLicenseMinutes);

        preferences.begin("lic", false);
        licenseMode_ = newLicenseMode;
        licenseDuration = newLicenseDuration;
        licenseMinutes_ = static_cast<uint8_t>(newLicenseMinutes);

        preferences.putUChar("license_mode", licenseMode_);
        preferences.putInt("license_dur", licenseDuration);
        preferences.putUChar("license_min", licenseMinutes_);

        unsigned long totalDurationSeconds = (static_cast<unsigned long>(licenseDuration) * 3600UL) +
                                             (static_cast<unsigned long>(licenseMinutes_) * 60UL);

        if (totalDurationSeconds == 0)
        {
            expireLicense();
            preferences.putUInt("license_timer", 0);
            Serial.println("[LICENSE] WS config dat duration=0, license het han");
        }
        else
        {
            remainingSeconds_ = totalDurationSeconds;
            licenseEndTime_ = 0;
            licenseExpired_ = false;
            licenseActive = true;
            pendingLicenseBrightness50_ = true;
            allowOneLicenseBrightnessUpdate_ = true;
            Serial.printf("[LICENSE] Kich hoat license moi: remaining=%lus mode=%u (%luh %um)\r\n",
                          (unsigned long)remainingSeconds_,
                          licenseMode_,
                          (unsigned long)licenseDuration,
                          licenseMinutes_);
        }

        preferences.end();
        licenseStartTime = millis();
        lastUpdateTime_ = millis();
        persistRemainingSeconds(true);

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
    case WStype_BIN:
    {
        if (!otaInProgress_ || num != otaClientId_)
        {
            break;
        }

        size_t written = Update.write(payload, length);
        if (written != length)
        {
            DynamicJsonDocument response(224);
            response["status"] = "error";
            response["message"] = "ota_write_failed";
            response["detail"] = Update.errorString();
            Update.abort();
            otaInProgress_ = false;
            otaClientId_ = 0xFF;
            otaExpectedSize_ = 0;
            otaReceivedSize_ = 0;
            Serial.printf("[OTA] Ghi chunk OTA that bai, err=%s\r\n", Update.errorString());

            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            break;
        }

        otaReceivedSize_ += static_cast<uint32_t>(written);
        DynamicJsonDocument progress(160);
        progress["type"] = "ota_progress";
        progress["received"] = otaReceivedSize_;
        progress["expected"] = otaExpectedSize_;
        String jsonProgress;
        serializeJson(progress, jsonProgress);
        webSocket.sendTXT(num, jsonProgress);
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
        let testButton = null;

        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.type === 'update_remaining') {
                if (data.expired) {
                    statusDiv.innerHTML = 'Mode: )RAW" +
                      String(licenseMode_) + R"RAW( | License het han';
                    statusDiv.style.color = '#ff0000';
                } else {
                    const hours = Math.floor(data.remaining / 3600);
                    const minutes = Math.floor((data.remaining % 3600) / 60);
                    const seconds = data.remaining % 60;
                    const timeDisplay = hours + ' gio ' + (minutes < 10 ? '0' : '') + minutes + ' phut ' + (seconds < 10 ? '0' : '') + seconds + ' giay';
                    statusDiv.innerHTML = 'Mode: )RAW" +
                      String(licenseMode_) + R"RAW( | Thoi gian con lai: ' + timeDisplay;
                    statusDiv.style.color = '#333333';
                }
            } else if (data.status === 'ok') {
                if (data.message === 'test_license_started') {
                    alert('Da cap test license 20 giay');
                } else {
                    alert('Cap nhat license thanh cong');
                }
            }
        };

        document.querySelector('form').onsubmit = function(e) {
            e.preventDefault();
            const formData = new FormData(this);
            const config = {
                mode: parseInt(formData.get('mode')),
                duration: parseInt(formData.get('duration')),
                duration_minutes: parseInt(formData.get('duration_minutes')) || 0
            };
            ws.send(JSON.stringify(config));
            return false;
        };

        window.addEventListener('load', function() {
            testButton = document.getElementById('test-license-btn');
            if (testButton) {
                testButton.onclick = function() {
                    const modeValue = parseInt(document.querySelector('select[name=\"mode\"]').value) || 1;
                    ws.send(JSON.stringify({
                        action: 'test_license',
                        mode: modeValue
                    }));
                };
            }
        });
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
button {
    width: 100%;
    padding: 12px;
    background: linear-gradient(45deg, #2d8cff, #75c3ff);
    border: none;
    border-radius: 8px;
    color: #ffffff;
    font-size: 1.1rem;
    cursor: pointer;
    margin-top: 12px;
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
    color: )RAW" + String(licenseExpired_ ? "#ff0000" : "#333333") +
                  R"RAW(;
    font-weight: bold;
}
</style>
</head>
<body>
  <h1>Cau hinh License cho ESP32-C3</h1>
  <form>
    <label>Mode License:</label><br>
    <select name="mode">
      <option value="1" )RAW" +
                  String(licenseMode_ == 1 ? "selected" : "") + R"RAW(>Mode 1: Tat LED</option>
      <option value="2" )RAW" +
                  String(licenseMode_ == 2 ? "selected" : "") + R"RAW(>Mode 2: Hong 20% LED</option>
      <option value="3" )RAW" +
                  String(licenseMode_ == 3 ? "selected" : "") + R"RAW(>Mode 3: Nhay RGB ngau nhien</option>
    </select><br><br>

   <label>Thoi gian License:</label><br>
<div style="display: flex; align-items: center; gap: 5px;">        
  <input type="number" name="duration" min="0" max="525600"
         value=")RAW" +
                  String(licenseDuration) + R"RAW(" style="width: 80px;">

  <span>:</span>

  <input type="number" name="duration_minutes" min="0" max="59"
         value=")RAW" +
                  String(licenseMinutes_) + R"RAW(" style="width: 60px;">
</div><br><br>

    <!--
    <label>Cho phep xuat du lieu:</label><br>
    <label class="switch">
      <input type="checkbox" name="transfer">
      <span class="slider"></span>
    </label><br><br>
    -->

    <input type="submit" value="Apply">
    <button type="button" id="test-license-btn">Test License</button>
  </form>

  <h2>Trang thai hien tai:</h2>
  <div id="status">Mode: )RAW" +
                  String(licenseMode_) + R"RAW( | )RAW" + statusText + R"RAW(</div>

  <div style="max-width: 400px; margin: 12px auto;">
    <a href="/OTA" style="display:block; padding:12px; border-radius:8px; text-decoration:none; color:#ffffff; background:linear-gradient(45deg, #2d8cff, #75c3ff);">Mo Trang OTA</a>
  </div>

  )RAW" + wsScript +
                  R"RAW(
</body></html>
)RAW";

    server.send(200, "text/html", html);
}

void LicenseManagerApp::handleOtaPage()
{
    String html = R"RAW(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 OTA</title>
<meta charset="UTF-8">
<style>
body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: linear-gradient(135deg, #c6f1ff, #fef6d8);
    background-repeat: no-repeat;
    color: #223344;
    margin: 0;
    padding: 20px;
    box-sizing: border-box;
    text-align: center;
}
.panel {
    background: rgba(255, 255, 255, 0.82);
    backdrop-filter: blur(8px);
    padding: 30px;
    border-radius: 12px;
    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.12);
    max-width: 420px;
    margin: 0 auto;
}
h1 {
    margin-bottom: 10px;
    color: #1d6fa5;
}
input[type="file"], button, a {
    width: 100%;
    box-sizing: border-box;
    margin-top: 12px;
    padding: 12px;
    border-radius: 8px;
    font-size: 1rem;
}
button {
    border: none;
    color: #ffffff;
    cursor: pointer;
    background: linear-gradient(45deg, #1976d2, #55b6ff);
}
a {
    display: block;
    text-decoration: none;
    color: #ffffff;
    background: linear-gradient(45deg, #ff8447, #ffb36c);
}
#ota-status {
    margin-top: 14px;
    min-height: 24px;
    font-weight: bold;
    color: #1d4f7a;
}
</style>
</head>
<body>
  <div class="panel">
    <h1>OTA Firmware</h1>
    <p>Trang nay danh rieng cho cap nhat firmware qua WebSocket.</p>
    <input type="file" id="ota-file" accept=".bin">
    <button type="button" id="ota-upload-btn">OTA Firmware</button>
    <div id="ota-status"></div>
    <a href="/">Quay Lai Trang License</a>
  </div>

  <script>
    const ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    const otaButton = document.getElementById('ota-upload-btn');
    const otaFileInput = document.getElementById('ota-file');
    const otaStatusDiv = document.getElementById('ota-status');

    async function uploadFirmwareViaWebSocket(file) {
        const chunkSize = 4096;
        otaStatusDiv.textContent = 'Dang bat dau OTA...';

        await new Promise((resolve, reject) => {
            const timeoutId = setTimeout(() => {
                ws.removeEventListener('message', beginHandler);
                reject(new Error('ota_begin_timeout'));
            }, 5000);

            const beginHandler = function(event) {
                const data = JSON.parse(event.data);
                if (data.message === 'ota_begin_ok') {
                    clearTimeout(timeoutId);
                    ws.removeEventListener('message', beginHandler);
                    resolve();
                } else if (data.message === 'ota_begin_failed' || data.message === 'ota_invalid_size' || data.message === 'ota_busy') {
                    clearTimeout(timeoutId);
                    ws.removeEventListener('message', beginHandler);
                    reject(new Error(data.detail || data.message));
                }
            };
            ws.addEventListener('message', beginHandler);

            ws.send(JSON.stringify({
                action: 'ota_begin',
                size: file.size
            }));
        });

        for (let offset = 0; offset < file.size; offset += chunkSize) {
            const chunk = file.slice(offset, offset + chunkSize);
            const buffer = await chunk.arrayBuffer();
            ws.send(buffer);
        }

        ws.send(JSON.stringify({ action: 'ota_end' }));
    }

    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        if (data.type === 'ota_progress') {
            const percent = data.expected > 0 ? Math.floor((data.received * 100) / data.expected) : 0;
            otaStatusDiv.textContent = 'Dang OTA: ' + percent + '% (' + data.received + '/' + data.expected + ' bytes)';
        } else if (data.status === 'ok' && data.message === 'ota_success') {
            otaStatusDiv.textContent = 'OTA thanh cong. Thiet bi se khoi dong lai.';
        } else if (data.status === 'error' && data.message && data.message.startsWith('ota_')) {
            otaStatusDiv.textContent = 'OTA loi: ' + (data.detail || data.message);
        }
    };

    otaButton.onclick = async function() {
        const file = otaFileInput.files[0];
        if (!file) {
            otaStatusDiv.textContent = 'Chua chon file firmware .bin';
            return;
        }
        try {
            await uploadFirmwareViaWebSocket(file);
        } catch (error) {
            otaStatusDiv.textContent = 'OTA loi: ' + error.message;
        }
    };
  </script>
</body>
</html>
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
    // bool newTransferMode = server.hasArg("transfer");

    Serial.printf("[LICENSE] Nhan config HTTP mode=%d duration=%dh %dm\r\n",
                  newLicenseMode,
                  newLicenseDuration,
                  newLicenseMinutes);

    licenseMode_ = newLicenseMode;
    licenseDuration = newLicenseDuration;
    licenseMinutes_ = static_cast<uint8_t>(newLicenseMinutes);

    preferences.putUChar("license_mode", licenseMode_);
    preferences.putInt("license_dur", licenseDuration);
    preferences.putUChar("license_min", licenseMinutes_);

    unsigned long totalDurationSeconds = (static_cast<unsigned long>(licenseDuration) * 3600UL) +
                                         (static_cast<unsigned long>(licenseMinutes_) * 60UL);

    if (totalDurationSeconds == 0)
    {
        expireLicense();
        preferences.putUInt("license_timer", 0);
        Serial.println("[LICENSE] HTTP config dat duration=0, license het han");
    }
    else
    {
        remainingSeconds_ = totalDurationSeconds;
        licenseEndTime_ = 0;
        licenseExpired_ = false;
        licenseActive = true;
        pendingLicenseBrightness50_ = true;
        allowOneLicenseBrightnessUpdate_ = true;
        Serial.printf("[LICENSE] Kich hoat license moi: remaining=%lus mode=%u (%luh %um)\r\n",
                      (unsigned long)remainingSeconds_,
                      licenseMode_,
                      (unsigned long)licenseDuration,
                      licenseMinutes_);
    }

    preferences.end();
    licenseStartTime = millis();
    lastUpdateTime_ = millis();
    persistRemainingSeconds(true);

    server.sendHeader("Location", "/");
    server.send(302);
}

void LicenseManagerApp::update()
{
    static bool lastExpiredState = true;

    if (otaPendingReboot_ && millis() >= otaRebootAt_)
    {
        Serial.println("[OTA] Khoi dong lai de hoan tat cap nhat firmware");
        ESP.restart();
    }

    if (licenseActive)
    {
        unsigned long currentTime = millis();
        unsigned long elapsedMs = currentTime - lastUpdateTime_;

        if (elapsedMs >= LICENSE_TICK_MS)
        {
            uint32_t elapsedSeconds = elapsedMs / LICENSE_TICK_MS;
            lastUpdateTime_ += elapsedSeconds * LICENSE_TICK_MS;

            if (elapsedSeconds >= remainingSeconds_)
            {
                expireLicense();
                persistRemainingSeconds(true);
                Serial.println("[LICENSE] Da het han va reset bo dem");
            }
            else
            {
                remainingSeconds_ -= elapsedSeconds;
                persistRemainingSeconds(false);
            }
        }

        if (remainingSeconds_ == 0 && !licenseExpired_)
        {
            expireLicense();
            persistRemainingSeconds(true);
            Serial.println("[LICENSE] Da het han va reset bo dem");
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
    }

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
