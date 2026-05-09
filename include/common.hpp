#pragma once

#include "config.hpp"

#include <Arduino.h>
#include <at24c32.h>
#include <Wire.h>

#define BITMASK(__mask) (1ULL<<static_cast<uint32_t>(__mask))

extern AT24C32 eeprom;

inline void led(bool state){
    digitalWrite(FWPIN_LED, !state);
}

float read_battery_voltage();

#if FW_SERVER
#else

#include <Adafruit_SSD1306.h>
#include <OneButton.h>

extern Adafruit_SSD1306 oled;
extern OneButton btn_;


#endif