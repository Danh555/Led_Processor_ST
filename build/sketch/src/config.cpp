#line 1 "D:\\LedProcesser\\LedPr_v2\\LedPr_v2\\src\\config.cpp"
#include "include/config.h"

// bool allowTransmission = false;
uint8_t licenseByte = 0x00;
uint32_t licenseDuration = 0;
uint8_t licenseMode_ = 1;
bool transferMode_ = false;
unsigned long licenseStartUnix_ = 0;
bool pendingLicenseBrightness50_ = false;
bool allowOneLicenseBrightnessUpdate_ = false;
