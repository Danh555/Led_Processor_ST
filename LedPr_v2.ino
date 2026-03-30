#include <Arduino.h>
#include "include/LedProcessor.h"
#include "include/app_license.h"
LedProcessor ledProcessor(SERIAL_RS232, RS232_RX_PIN, RS232_TX_PIN); //Serial1 
PacketHandler packetHandler(SERIAL); //Serial

LicenseManagerApp licenseManager;
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
