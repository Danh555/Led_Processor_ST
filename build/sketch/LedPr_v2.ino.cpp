#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\LedPr_v2.ino"
#include <Arduino.h>
#include "include/LedProcessor.h"
#include "include/app_license.h"
LedProcessor ledProcessor(SERIAL_RS232, RS232_RX_PIN, RS232_TX_PIN); //Serial1 
PacketHandler packetHandler(SERIAL); //Serial

LicenseManagerApp licenseManager;
#line 8 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\LedPr_v2.ino"
void setup();
#line 15 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\LedPr_v2.ino"
void loop();
#line 8 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\LedPr_v2.ino"
void setup()
{
    Serial.begin(115200);
    ledProcessor.begin();   
    licenseManager.begin();
}

void loop()
{
    licenseManager.handle();
    licenseManager.update();
    ledProcessor.process();
}

