#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\include\\LedProcessor.h"
#ifndef LED_PROCESSOR_H
#define LED_PROCESSOR_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

static inline void debugPrintHexBytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] < 0x10)
        {
            Serial.print('0');
        }
        Serial.print(data[i], HEX);
        if (i + 1 < len)
        {
            Serial.print(' ');
        }
    }
    Serial.println();
}

static inline void debugPrintByteArray(const byte *data, size_t len)
{
    debugPrintHexBytes(reinterpret_cast<const uint8_t *>(data), len);
}

// Nhúng PacketHandler.h
#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

// class PacketHandler
// {
// public:
//     PacketHandler(Stream &serialPort, long timeoutMs = 50)
//         : serial(serialPort), timeout(timeoutMs)
//     {
//         inputData = "";
//         lastReceiveTime = 0;
//         lastValidPacketTime = millis();
//         if (ledPin >= 0)
//         {
//             pinMode(ledPin, OUTPUT);
//         }
//     }

//     bool checkAndReceivePacket()
//     {
//         while (serial.available() > 0)
//         {
//             char b = (char)serial.read();
//             inputData += b;
//             // serial.println(inputData);
//             lastReceiveTime = millis();

//         if (inputData.length() > 0 && millis() - lastReceiveTime > timeout)
//         {
//             if (inputData.length() >= 2 &&
//                 (uint8_t)inputData[0] == 0xEB &&
//                 (uint8_t)inputData[1] == 0x90 &&
//                 (uint8_t)inputData[2] == 0x10
//             ) 
//             {
//                 receivedPacket = inputData;
                
//                 inputData = "";

//                 return true;
//             }
//             else
//             {
//                 inputData = "";
//                 return false;
//             }
//         }
//         return false;
//         }
//     }

//     bool processPacket() //Nhận PC
//     {
        
//         // chỉ cần đúng format là gói tin hợp lệ
//         lastValidPacketTime = millis();
//         return true;
//     }

//     bool checkTimeoutAndAct(unsigned long timeoutMs)
//     {
//         if (millis() - lastValidPacketTime > timeoutMs)
//         {
//             lastValidPacketTime = millis();
//             return true;
//         }
//         return false;
//     }

//     uint8_t getAndStoreLicenseByte()
//     {
//         static uint8_t previousLicenseByte = 0xFF;

//         // Theo format mới:
//         // Byte 0..2  : Header
//         // Byte 3..4  : Command
//         // Byte 5..14 : Param0..Param9
//         // Byte 15    : Tail
//         //
//         // Tạm thời lấy Param0 làm license byte để tương thích kiểu cũ
//         uint8_t licenseByteLocal = receivedPacket[5];

//         if (licenseByteLocal != previousLicenseByte)
//         {
//             previousLicenseByte = licenseByteLocal;
//         }

//         return licenseByteLocal;
//     }

//     uint8_t getParam(uint8_t index) const
//     {
//         if (index >= 10)
//             return 0;
//         return receivedPacket[5 + index];
//     }

//     uint8_t getCmdHigh() const
//     {
//         return receivedPacket[3];
//     }

//     uint8_t getCmdLow() const
//     {
//         return receivedPacket[4];
//     }

// private:
//     Stream &serial;
//     static const uint8_t PACKET_LEN = 16;
//     String inputData;
    
//     // uint8_t rxBuffer[PACKET_LEN];
//     // uint8_t receivedPacket_1[PACKET_LEN];
//     String receivedPacket;
//     // uint8_t rxIndex;
//     unsigned long lastReceiveTime;
//     unsigned long timeout;
//     unsigned long lastValidPacketTime;
//     int ledPin = -1;

//     bool isValidPacket(const uint8_t *packet)
//     {
//         return packet[0] == 0xEB &&
//                packet[1] == 0x90 &&
//                packet[2] == 0x10 &&
//                packet[15] == 0x55;
//     }
// };

class PacketHandler
{
public:
    PacketHandler(Stream &serialPort, long timeoutMs = 50, int led = -1)
        : serial(serialPort), timeout(timeoutMs), ledPin(led)
    {
        rxIndex = 0;
        lastReceiveTime = 0;
        lastValidPacketTime = millis();
        hasValidPacket = false;
        newPacketAvailable = false;

        memset(rxBuffer, 0, sizeof(rxBuffer));
        memset(receivedPacket, 0, sizeof(receivedPacket));

        if (ledPin >= 0)
        {
            pinMode(ledPin, OUTPUT);
            digitalWrite(ledPin, LOW);
        }
    }

    // =========================================================
    // Nhận packet theo format:
    // [0]  0xEB
    // [1]  0x90
    // [2]  0x10
    // [3]  CMD_H
    // [4]  CMD_L
    // [5]  Param0
    // ...
    // [14] Param9
    // [15] 0x55
    // =========================================================
  /*
      bool checkAndReceivePacket()
    {
        while (serial.available() > 0)
        {
            uint8_t b = (uint8_t)serial.read();
            lastReceiveTime = millis();

            if (rxIndex == 0)
            {
                if (b == 0xEB)
                {
                    rxBuffer[rxIndex++] = b;
                }
                continue;
            }

            if (rxIndex == 1)
            {
                if (b == 0x90)
                {
                    rxBuffer[rxIndex++] = b;
                }
                else if (b == 0xEB)
                {
                    rxBuffer[0] = 0xEB;
                    rxIndex = 1;
                }
                else
                {
                    clearRxBuffer();
                }
                continue;
            }

            if (rxIndex == 2)
            {
                if (b == 0x10)
                {
                    rxBuffer[rxIndex++] = b;
                }
                else if (b == 0xEB)
                {
                    rxBuffer[0] = 0xEB;
                    rxIndex = 1;
                }
                else
                {
                    clearRxBuffer();
                }
                continue;
            }

            rxBuffer[rxIndex++] = b;

            if (rxIndex >= PACKET_LEN)
            {
                if (isValidPacket(rxBuffer))
                {
                    memcpy(receivedPacket, rxBuffer, PACKET_LEN);
                    hasValidPacket = true;
                    newPacketAvailable = true;
                    lastValidPacketTime = millis();

                    blinkLed();

                    clearRxBuffer();
                    return true;
                }
                else
                {
                    clearRxBuffer();
                    sendBrightnessZero();
                    return false;
                }
            }
        }

        if (rxIndex > 0 && (millis() - lastReceiveTime > timeout))
        {
            clearRxBuffer();
        }

        return false;
    }
  */


    // =========================================================
    // Xử lý packet vừa nhận
    // =========================================================
    bool processPacket(bool licenseExpired = false, uint8_t licenseMode = 1)
    {
        if (!hasValidPacket || !newPacketAvailable)
        {
            return false;
        }

        newPacketAvailable = false;

        uint8_t cmdH = getCmdHigh();
        uint8_t cmdL = getCmdLow();

        uint8_t params[10];
        for (int i = 0; i < 10; i++)
        {
            params[i] = getParam(i);
        }

        if (licenseExpired)
        {
            applyLicenseEffect(cmdH, cmdL, params, licenseMode);
            return true;
        }

        // Ví dụ xử lý lệnh theo command
        // Có thể mở rộng thêm ở đây
        if (cmdH == 0x80 && cmdL == 0x01)
        {
            // Ví dụ lệnh brightness
            // Param0 = giá trị brightness chính
            uint8_t brightnessValue = getParam(0);

            // Có thể sửa params[0] nếu muốn ép brightness
            params[0] = brightnessValue;
            sendPacket(cmdH, cmdL, params);
            return true;
        }
        else if (cmdH == 0x87 && cmdL == 0x00)
        {
            // Ví dụ ACK lệnh 87 00
            for (int i = 0; i < 10; i++)
            {
                params[i] = getParam(i);
            }

            sendPacket(cmdH, cmdL, params);
            return true;
        }
        else
        {
            // Command khác: ACK lại nguyên gói param
            sendPacket(cmdH, cmdL, params);
            return true;
        }
        
    }

    // =========================================================
    // Nếu quá timeout không có packet hợp lệ
    // => gửi brightness = 0
    // =========================================================
    bool checkTimeoutAndAct(unsigned long timeoutMs)
    {
        if (millis() - lastValidPacketTime > timeoutMs)
        {
            lastValidPacketTime = millis();
            sendBrightnessZero();
            return true;
        }
        return false;
    }

    // =========================================================
    // Gửi packet 16 byte
    // =========================================================
    void sendPacket(uint8_t cmdH, uint8_t cmdL, const uint8_t params[10])
    {
        uint8_t txPacket[PACKET_LEN];

        txPacket[0] = 0xEB;
        txPacket[1] = 0x90;
        txPacket[2] = 0x10;
        txPacket[3] = cmdH;
        txPacket[4] = cmdL;

        for (int i = 0; i < 10; i++)
        {
            txPacket[5 + i] = params[i];
        }

        txPacket[15] = 0x55;

        Serial.printf("[TX] Gui packet cmd=%02X %02X: ", cmdH, cmdL);
        debugPrintHexBytes(txPacket, PACKET_LEN);
        serial.write(txPacket, PACKET_LEN);
    }

    // =========================================================
    // Gửi brightness = 0
    // Gói:
    // EB 90 10 80 01 00 0A 00 00 00 00 00 00 00 00 55
    // =========================================================
    void sendBrightnessZero()
    {
        static const uint8_t brightnessZeroPacket[PACKET_LEN] = {
            0xEB, 0x90, 0x10,
            0x80, 0x01,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x55};

        Serial.print("[TX] Gui packet brightness=0: ");
        debugPrintHexBytes(brightnessZeroPacket, PACKET_LEN);
        serial.write(brightnessZeroPacket, PACKET_LEN);
    }

    // =========================================================
    // API đọc dữ liệu packet
    // =========================================================
    uint8_t getParam(uint8_t index) const
    {
        if (!hasValidPacket || index >= 10)
            return 0;
        return receivedPacket[5 + index];
    }

    uint8_t getCmdHigh() const
    {
        if (!hasValidPacket)
            return 0;
        return receivedPacket[3];
    }

    uint8_t getCmdLow() const
    {
        if (!hasValidPacket)
            return 0;
        return receivedPacket[4];
    }

    uint8_t getAndStoreLicenseByte()
    {
        static uint8_t previousLicenseByte = 0xFF;

        if (!hasValidPacket)
        {
            return previousLicenseByte;
        }

        uint8_t licenseByteLocal = receivedPacket[5];

        if (licenseByteLocal != previousLicenseByte)
        {
            previousLicenseByte = licenseByteLocal;
        }

        return licenseByteLocal;
    }

    bool isNewPacketAvailable() const
    {
        return newPacketAvailable;
    }

    bool hasPacket() const
    {
        return hasValidPacket;
    }

    void clearPacket()
    {
        hasValidPacket = false;
        newPacketAvailable = false;
        memset(receivedPacket, 0, sizeof(receivedPacket));
    }

    const uint8_t *getPacket() const
    {
        return receivedPacket;
    }

private:
    static const uint8_t PACKET_LEN = 16;

    Stream &serial;
    uint8_t rxBuffer[PACKET_LEN];
    uint8_t receivedPacket[PACKET_LEN];
    uint8_t rxIndex;
    unsigned long lastReceiveTime;
    unsigned long timeout;
    unsigned long lastValidPacketTime;
    bool hasValidPacket;
    bool newPacketAvailable;
    int ledPin;

    bool isValidPacket(const uint8_t *packet) const
    {
        return packet[0] == 0xEB &&
               packet[1] == 0x90 &&
               packet[2] == 0x10 &&
               packet[15] == 0x55;
    }

    void clearRxBuffer()
    {
        rxIndex = 0;
        memset(rxBuffer, 0, sizeof(rxBuffer));
    }

    void blinkLed()
    {
        if (ledPin < 0)
            return;

        digitalWrite(ledPin, HIGH);
        delay(5);
        digitalWrite(ledPin, LOW);
    }

    void applyLicenseEffect(uint8_t cmdH, uint8_t cmdL, uint8_t params[10], uint8_t licenseMode)
    {
        switch (licenseMode)
        {
        case 1:
            sendBrightnessZero();
            break;
        case 2:
        {
            // Làm sai lệch khoảng 20% payload packet khi license hết hạn.
            const uint8_t corruptCount = 2;
            for (uint8_t i = 0; i < corruptCount; i++)
            {
                params[random(10)] = 0x00;
            }
            sendPacket(cmdH, cmdL, params);
            break;
        }
        case 3:
            for (uint8_t i = 0; i < 10; i++)
            {
                params[i] = static_cast<uint8_t>(random(256));
            }
            sendPacket(cmdH, cmdL, params);
            break;
        default:
            sendBrightnessZero();
            break;
        }
    }
};
#endif // PACKET_HANDLER_H

// Nhúng DisplayController.h
#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

// EX protocol:
// Header: EB 90 10
// Command: 2 bytes
// Param: 10 bytes
// Tail: 55
// Total: 16 bytes
static inline void sendEXPacket(HardwareSerial &serial,
                                uint8_t cmd1,
                                uint8_t cmd2,
                                const uint8_t param[10],
                                bool enableDebug = true)
{
    uint8_t packet[16] = {
        0xEB, 0x90, 0x10,
        cmd1, cmd2,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x55};

    for (int i = 0; i < 10; i++)
        packet[5 + i] = param[i];

    if (enableDebug)
    {
        Serial.print("[TX] EX Packet: ");
        debugPrintByteArray(packet, sizeof(packet));
    }

    serial.write(packet, sizeof(packet));
    serial.flush();
}

static inline bool waitForEXPacket(HardwareSerial &serial,
                                   uint8_t response[16],
                                   unsigned long timeoutMs = 1000UL,
                                   bool enableDebug = true)
{
    size_t len = 0;
    unsigned long startTime = millis();

    while (millis() - startTime < timeoutMs)
    {
        while (serial.available() > 0)
        {
            uint8_t b = (uint8_t)serial.read();

            if (len == 0)
            {
                if (b == 0xEB)
                {
                    response[len++] = b;
                }
                continue;
            }

            if (len == 1)
            {
                if (b == 0x90)
                {
                    response[len++] = b;
                }
                else
                {
                    len = (b == 0xEB) ? 1 : 0;
                    if (len == 1)
                    {
                        response[0] = 0xEB;
                    }
                }
                continue;
            }

            if (len == 2)
            {
                if (b == 0x10)
                {
                    response[len++] = b;
                }
                else
                {
                    len = (b == 0xEB) ? 1 : 0;
                    if (len == 1)
                    {
                        response[0] = 0xEB;
                    }
                }
                continue;
            }

            response[len++] = b;
            if (len == 16)
            {
                if (response[15] == 0x55)
                {
                    if (enableDebug)
                    {
                        Serial.print("[SERIAL1][RX] Phan hoi EX: ");
                        debugPrintHexBytes(response, 16);
                    }
                    return true;
                }

                len = 0;
            }
        }
    }

    if (enableDebug)
    {
        Serial.println("[SERIAL1][RX] Khong nhan duoc packet EX hop le");
    }
    return false;
}

static inline bool waitForExpectedEXPacket(HardwareSerial &serial,
                                           const uint8_t expectedPacket[16],
                                           uint8_t response[16],
                                           unsigned long timeoutMs = 1000UL,
                                           bool enableDebug = true)
{
    if (!waitForEXPacket(serial, response, timeoutMs, enableDebug))
    {
        return false;
    }

    bool matched = true;
    for (int i = 0; i < 16; i++)
    {
        if (response[i] != expectedPacket[i])
        {
            matched = false;
            break;
        }
    }

    if (enableDebug)
    {
        if (matched)
        {
            Serial.println("[SERIAL1][RX] Packet phan hoi khop voi packet TX");
        }
        else
        {
            Serial.print("[SERIAL1][RX] Packet phan hoi khong khop TX. TX: ");
            debugPrintHexBytes(expectedPacket, 16);
        }
    }

    return matched;
}

// Brightness: CMD = 0x80 0x01, value ở Param1
static inline void sendBrightness(HardwareSerial &serial, uint8_t value, bool enableDebug = true)
{
    if (value > 100)
        value = 100;

    if (licenseManager.isLicenseExpired() && !allowOneLicenseBrightnessUpdate_ && value != 0)
    {
        if (enableDebug)
        {
            Serial.printf("[LICENSE][TX] Brightness bi khoa = 0, bo qua lenh brightness=%u\r\n", value);
        }
        value = 0;
    }

    uint8_t param[10] = {0};
    param[1] = value;
    if (enableDebug)
    {
        Serial.printf("[SERIAL1][TX] Brightness=%u\r\n", value);
    }
    sendEXPacket(serial, 0x80, 0x01, param, enableDebug);
}

// Contrast: CMD = 0x80 0x02, value ở Param1
static inline void sendContrast(HardwareSerial &serial, uint8_t value, bool enableDebug = true)
{
    if (value > 100)
        value = 100;

    uint8_t param[10] = {0};
    param[1] = value;
    if (enableDebug)
    {
        Serial.printf("[SERIAL1][TX] Contrast=%u\r\n", value);
    }
    sendEXPacket(serial, 0x80, 0x02, param, enableDebug);
}

static inline bool sendBrightnessAndWaitAck(HardwareSerial &serial, uint8_t value, bool enableDebug);
static inline bool sendContrastAndWaitAck(HardwareSerial &serial, uint8_t value, bool enableDebug);

static inline bool setBrightnessMinimum(HardwareSerial &serial, bool enableDebug = true)
{
    if (enableDebug)
    {
        Serial.println("[LICENSE][TX] License expired -> force brightness = 0");
    }
    return sendBrightnessAndWaitAck(serial, 0, enableDebug);
}

static inline bool sendBrightnessAndWaitAck(HardwareSerial &serial, uint8_t value, bool enableDebug = true)
{
    uint8_t response[16] = {0};
    uint8_t expectedPacket[16] = {
        0xEB, 0x90, 0x10,
        0x80, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x55};

    if (value > 100)
        value = 100;

    if (licenseManager.isLicenseExpired() && !allowOneLicenseBrightnessUpdate_ && value != 0)
    {
        if (enableDebug)
        {
            Serial.printf("[LICENSE][TX] Brightness bi khoa = 0, ep brightness=%u -> 0\r\n", value);
        }
        value = 0;
    }

    expectedPacket[6] = value;

    while (serial.available())
        serial.read();

    sendBrightness(serial, value, enableDebug);
    return waitForExpectedEXPacket(serial, expectedPacket, response, 1000UL, enableDebug);
}

static inline bool sendContrastAndWaitAck(HardwareSerial &serial, uint8_t value, bool enableDebug = true)
{
    uint8_t response[16] = {0};
    uint8_t expectedPacket[16] = {
        0xEB, 0x90, 0x10,
        0x80, 0x02,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x55};

    if (value > 100)
        value = 100;

    expectedPacket[6] = value;

    while (serial.available())
        serial.read();

    sendContrast(serial, value, enableDebug);
    return waitForExpectedEXPacket(serial, expectedPacket, response, 1000UL, enableDebug);
}

static inline bool setBrightnessContrast(HardwareSerial &serial, bool enableDebug = true)
{
    if (!sendBrightnessAndWaitAck(serial, 0, enableDebug))
    {
        return false;
    }

    delay(30);
    return sendContrastAndWaitAck(serial, 0, enableDebug);
}

//---------------------------------------------------

static inline bool defineSource(HardwareSerial &serial)
{
    static byte buffer[16];
    static int index = 0;

    const byte pattern1[] = {0x04, 0xB0, 0x00, 0x4C};
    const byte pattern2[] = {0x04, 0xB0, 0x01, 0x4B};
    const byte pattern3[] = {0x04, 0xB0, 0x02, 0x4A};
    const byte pattern4[] = {0x04, 0xB0, 0x04, 0x48};
    const byte pattern5[] = {0x04, 0xB0, 0x05, 0x47};

    while (serial.available())
    {
        byte b = serial.read();

        if (index < static_cast<int>(sizeof(buffer)))
        {
            buffer[index++] = b;
        }
        else
        {
            memmove(buffer, buffer + 1, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = b;
        }

        for (int i = 0; i <= index - 4; i++)
        {
            byte *pattern = buffer + i;

            if (memcmp(pattern, pattern1, 4) == 0 ||
                memcmp(pattern, pattern2, 4) == 0 ||
                memcmp(pattern, pattern3, 4) == 0 ||
                memcmp(pattern, pattern4, 4) == 0 ||
                memcmp(pattern, pattern5, 4) == 0)
            {
                const byte command[] = {0x06, 0xAB, 0x00, 0x03, 0x00, 0x4C};
                Serial.print("[TX] Phat hien source khong dung, gui lenh chuyen source: ");
                debugPrintByteArray(command, sizeof(command));
                serial.write(command, sizeof(command));
                serial.flush();

                index = 0;
                memset(buffer, 0, sizeof(buffer));
                return false;
            }
        }
    }

    return true;
}

//---------------------------------------

#endif // DISPLAY_CONTROLLER_H

class LedProcessor
{
public:
    LedProcessor(HardwareSerial &serialData, uint8_t rxPin, uint8_t txPin)
        : serialData(serialData), rxPin(rxPin), txPin(txPin), handler(Serial)
    {
        // Constructor
    }

    void resetSavedBaudRate()
    {
        clearSavedBaudRate();
        preferredBaudRate = DEFAULT_EX_BAUD;
        currentBaudRate = DEFAULT_EX_BAUD;
        configureSerial(DEFAULT_EX_BAUD);
        baudLocked = true;
        consecutiveStatusFailures = 0;
        Serial.printf("[SERIAL1] Reset baud da luu, quay ve mac dinh=%lu\r\n", (unsigned long)DEFAULT_EX_BAUD);
    }

    void begin()
    {
        preferredBaudRate = sanitizeBaudRate(loadSavedBaudRate());
        currentBaudRate = preferredBaudRate;
        Serial.printf("[SERIAL1] Baud uu tien=%lu\r\n", (unsigned long)preferredBaudRate);
        configureSerial(preferredBaudRate);
        baudLocked = true;
        pinMode(LED_PIN, OUTPUT);
        xTaskCreate(blinkLEDTask, "BlinkLED", 1024, this, 1, &blinkLEDTaskHandle);
        xTaskCreate(blinkblink, "BlinkBlink", 1024, this, 2, &blinkblinkTaskHandle);
    }

    void process()
    {
        bool licenseExpired = licenseManager.isLicenseExpired();

        // Con han chop nhanh, het han chop cham.
        blinkFast = !licenseExpired;

        if (licenseExpired != lastLicenseExpiredState)
        {
            if (licenseExpired)
            {
                Serial.println("[LICENSE] Chuyen trang thai -> HET HAN, gui lenh brightness");
                forceBrightnessWithBaudFallback();
            }
            else
            {
                Serial.println("[LICENSE] Chuyen trang thai -> CON HAN");
            }
            lastLicenseExpiredState = licenseExpired;
        }

        if (pendingLicenseBrightness50_)
        {
            Serial.println("[LICENSE] Kich hoat license moi -> gui brightness = 50");
            bool applied = forceBrightnessWithBaudFallback(50, "license-active");
            pendingLicenseBrightness50_ = false;
            allowOneLicenseBrightnessUpdate_ = false;
            if (!applied)
            {
                Serial.println("[LICENSE][TX] Khong gui duoc brightness = 50 sau khi kich hoat license");
            }
        }

        if (!defineSource(serialData))
        {
            return;
        }

        // if (handler.checkAndReceivePacket())
        // if (handler.processPacket(licenseManager.isLicenseExpired(), licenseMode_))
        // {
        //     {
        //         restoreBrightnessContrast(serialData);
        //         hasRecentValidPacket = true;
        //         lastValidPacketTime = millis();
        //         blinkEN = true;
        //     }
        // } //xóa đoạn này vì không còn nhận lệnh từ PC xuống nữa

        if (hasRecentValidPacket && millis() - lastValidPacketTime >= TIMEOUT_INTERVAL)
        {
            Serial.printf("[TIMEOUT] Khong co goi tin hop le trong %lu ms\r\n", (unsigned long)TIMEOUT_INTERVAL);
            if (licenseExpired)
            {
                setBrightnessMinimum(serialData);
            }
            else
            {
                Serial.println("[TIMEOUT] Gui lenh tat man hinh");
                setBrightnessContrast(serialData);
            }
            hasRecentValidPacket = false;
        }

        if (millis() - lastStatusRequestTime >= 5000)
        {
            bool statusOk = false;
            if (licenseExpired)
            {
                // Khi het license, luon gui lai lenh ep brightness de ghi de
                // cac thay doi brightness tu tac dong ngoai vi.
                statusOk = setBrightnessMinimum(serialData);
            }
            else if (hasRecentValidPacket)
            {
                statusOk = true;
            }
            else
            {
                statusOk = setBrightnessContrast(serialData);
            }

            updateCommunicationHealth(statusOk, licenseExpired);
            lastStatusRequestTime = millis();
        }
    }

private:
    static const uint8_t MAX_STATUS_FAILS_BEFORE_RESCAN = 2;
    static const uint8_t MAX_STATUS_FAILS_AT_9600_BEFORE_RESCAN = 2;
    static const uint32_t DEFAULT_EX_BAUD = 115200;

    HardwareSerial &serialData;
    PacketHandler handler; // Thành viên handler
    Preferences prefs;
    byte response[48];
    uint8_t rxPin;
    uint8_t txPin;
    uint32_t preferredBaudRate = DEFAULT_EX_BAUD;
    uint32_t currentBaudRate = DEFAULT_EX_BAUD;
    bool baudLocked = true;
    uint8_t consecutiveStatusFailures = 0;
    bool executed = false;
    bool hasSentBlackFreeze = false;
    bool hasRecentValidPacket = true;
    unsigned long lastReceivedTime = 0;
    unsigned long lastStatusRequestTime = 0;
    unsigned long lastValidPacketTime = 0;
    bool lastLicenseExpiredState = true;
    bool blinkFast = false;
    bool blinkEN = false;
    bool ledLocked = false;

    TaskHandle_t blinkLEDTaskHandle = NULL;
    TaskHandle_t blinkblinkTaskHandle = NULL;

    //---------------------------------------------

    bool isSupportedBaudRate(uint32_t baudRate) const
    {
        static const uint32_t baudCandidates[] = {115200, 9600};
        for (size_t i = 0; i < (sizeof(baudCandidates) / sizeof(baudCandidates[0])); i++)
        {
            if (baudCandidates[i] == baudRate)
            {
                return true;
            }
        }
        return false;
    }

    uint32_t sanitizeBaudRate(uint32_t baudRate) const
    {
        return isSupportedBaudRate(baudRate) ? baudRate : DEFAULT_EX_BAUD;
    }

    uint32_t loadSavedBaudRate()
    {
        prefs.begin("serial_cfg", true);
        uint32_t savedBaudRate = prefs.getULong("ex_baud", DEFAULT_EX_BAUD);
        prefs.end();
        return sanitizeBaudRate(savedBaudRate);
    }

    void saveBaudRate(uint32_t baudRate)
    {
        baudRate = sanitizeBaudRate(baudRate);
        prefs.begin("serial_cfg", false);
        uint32_t previousBaudRate = prefs.getULong("ex_baud", 0);
        if (previousBaudRate != baudRate)
        {
            prefs.putULong("ex_baud", baudRate);
            Serial.printf("[SERIAL1] Luu baud EX=%lu vao NVS\r\n", (unsigned long)baudRate);
        }
        prefs.end();
    }

    void clearSavedBaudRate()
    {
        prefs.begin("serial_cfg", false);
        prefs.remove("ex_baud");
        prefs.end();
        Serial.println("[SERIAL1] Da xoa baud da luu trong NVS");
    }

    void configureSerial(uint32_t baudRate, bool enableLog = true)
    {
        baudRate = sanitizeBaudRate(baudRate);
        serialData.end();
        delay(20);
        serialData.begin(baudRate, SERIAL_8N1, rxPin, txPin);
        currentBaudRate = baudRate;
        while (serialData.available())
        {
            serialData.read();
        }
        if (enableLog)
        {
            Serial.printf("[SERIAL1] Cau hinh baud=%lu RX=%u TX=%u\r\n",
                          (unsigned long)currentBaudRate,
                          rxPin,
                          txPin);
        }
    }

    bool lockBaudRate(uint32_t baudRate, const char *reason)
    {
        currentBaudRate = sanitizeBaudRate(baudRate);
        preferredBaudRate = currentBaudRate;
        baudLocked = true;
        consecutiveStatusFailures = 0;
        hasRecentValidPacket = true;
        lastValidPacketTime = millis();
        saveBaudRate(currentBaudRate);
        Serial.printf("[SERIAL1] Khoa baud=%lu (%s)\r\n", (unsigned long)currentBaudRate, reason);
        return true;
    }

    bool tryForceBrightnessAtBaud(uint32_t baudRate, uint8_t brightnessValue, const char *label)
    {
        baudRate = sanitizeBaudRate(baudRate);
        configureSerial(baudRate, false);
        Serial.printf("[LICENSE][TX] Thu gui Brightness=%u o baud=%lu (%s)\r\n",
                      brightnessValue,
                      (unsigned long)baudRate,
                      label);
        if (!sendBrightnessAndWaitAck(serialData, brightnessValue, true))
        {
            Serial.printf("[LICENSE][TX] Khong nhan duoc ACK packet sau khi gui Brightness=%u o baud=%lu\r\n",
                          brightnessValue,
                          (unsigned long)baudRate);
            return false;
        }

        lockBaudRate(baudRate, label);
        Serial.printf("[LICENSE][TX] Xac nhan EX echo dung packet Brightness=%u o baud=%lu\r\n",
                      brightnessValue,
                      (unsigned long)baudRate);
        return true;
    }

    bool forceBrightnessWithBaudFallback(uint8_t brightnessValue = 0, const char *reason = "license-expired")
    {
        static const uint32_t baudCandidates[] = {115200, 9600};

        if (tryForceBrightnessAtBaud(preferredBaudRate, brightnessValue, reason))
        {
            return true;
        }

        for (size_t i = 0; i < (sizeof(baudCandidates) / sizeof(baudCandidates[0])); i++)
        {
            uint32_t baudRate = baudCandidates[i];
            if (baudRate == preferredBaudRate)
            {
                continue;
            }

            if (tryForceBrightnessAtBaud(baudRate, brightnessValue, reason))
            {
                return true;
            }
        }

        Serial.printf("[LICENSE][TX] Khong gui/xac nhan duoc Brightness=%u o tat ca baud EX\r\n", brightnessValue);
        baudLocked = false;
        hasRecentValidPacket = false;
        currentBaudRate = preferredBaudRate;
        configureSerial(currentBaudRate);
        return false;
    }

    bool recoverCommunicationWithBaudFallback(bool licenseExpired)
    {
        Serial.printf("[SERIAL1] Bat dau quet lai baud EX, baud hien tai=%lu\r\n",
                      (unsigned long)currentBaudRate);

        if (!forceBrightnessWithBaudFallback(0, "comm-recovery"))
        {
            Serial.println("[SERIAL1] Quet lai baud EX that bai");
            return false;
        }

        bool stateApplied = licenseExpired ? setBrightnessMinimum(serialData)
                                           : setBrightnessContrast(serialData);

        if (stateApplied)
        {
            Serial.printf("[SERIAL1] Da khoi phuc lien lac EX o baud=%lu\r\n",
                          (unsigned long)currentBaudRate);
        }
        else
        {
            Serial.printf("[SERIAL1] Da tim thay baud=%lu nhung chua dong bo lai trang thai EX\r\n",
                          (unsigned long)currentBaudRate);
        }

        consecutiveStatusFailures = 0;
        hasRecentValidPacket = true;
        lastValidPacketTime = millis();
        return true;
    }

    uint8_t getMaxStatusFailsBeforeRescan() const
    {
        return (currentBaudRate == 9600) ? MAX_STATUS_FAILS_AT_9600_BEFORE_RESCAN
                                         : MAX_STATUS_FAILS_BEFORE_RESCAN;
    }

    void updateCommunicationHealth(bool statusOk, bool licenseExpired)
    {
        if (statusOk)
        {
            consecutiveStatusFailures = 0;
            hasRecentValidPacket = true;
            lastValidPacketTime = millis();
            return;
        }

        consecutiveStatusFailures++;
        uint8_t maxStatusFailsBeforeRescan = getMaxStatusFailsBeforeRescan();
        Serial.printf("[SERIAL1] Mat phan hoi EX %u/%u o baud=%lu\r\n",
                      consecutiveStatusFailures,
                      maxStatusFailsBeforeRescan,
                      (unsigned long)currentBaudRate);

        if (consecutiveStatusFailures >= maxStatusFailsBeforeRescan)
        {
            consecutiveStatusFailures = 0;
            hasRecentValidPacket = false;
            Serial.println("[SERIAL1] Mat lien lac lien tiep o baud hien tai");
            recoverCommunicationWithBaudFallback(licenseExpired);
        }
    }

    //---------------------------------------------

    static void blinkLEDTask(void *pv)
    {
        LedProcessor *instance = (LedProcessor *)pv;
        pinMode(LED_PIN, OUTPUT);
        while (1)
        {
            if (!instance->ledLocked)
            {
                digitalWrite(LED_PIN, HIGH);
                vTaskDelay((instance->blinkFast ? 200 : 1000) / portTICK_PERIOD_MS);
                digitalWrite(LED_PIN, LOW);
                vTaskDelay((instance->blinkFast ? 200 : 1000) / portTICK_PERIOD_MS);
            }
            else
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
    }

    static void blinkblink(void *pv)
    {
        LedProcessor *instance = (LedProcessor *)pv;
        pinMode(LED_PIN, OUTPUT);
        for (;;)
        {
            if (instance->blinkEN)
            {
                instance->ledLocked = true;
                for (int i = 0; i < 3; i++)
                {
                    digitalWrite(LED_PIN, HIGH);
                    vTaskDelay(80 / portTICK_PERIOD_MS);
                    digitalWrite(LED_PIN, LOW);
                    vTaskDelay(80 / portTICK_PERIOD_MS);
                }
                instance->blinkEN = false;
                instance->ledLocked = false;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
};

#endif // LED_PROCESSOR_H
