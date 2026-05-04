#ifndef FW_SERVER
#warning FW_SERVER is undefined
#define FW_SERVER 1 
#endif
#include <BLEDevice.h>
#include <at24c32.h>
#if FW_SERVER
#include <RTClib.h>
#else
#include <Adafruit_SSD1306.h>
#include <OneButton.h>
#endif
#include <Arduino.h>
extern void setup(void);
extern void loop(void);