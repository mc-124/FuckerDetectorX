#pragma once

#include "config.hpp"

#include <Arduino.h>
#include <Wire.h>

#define BITMASK(__mask) (1ULL<<static_cast<uint32_t>(__mask))

#if FW_SERVER
#include <at24c32.h>
extern AT24C32 eeprom;
#endif

inline void led(bool state){
    digitalWrite(FWPIN_LED, !state);
}

float read_battery_voltage();

#if FW_SERVER
#else

#include <Adafruit_SSD1306.h>
#include <OneButton.h>

extern Adafruit_SSD1306 oled;
extern OneButton btn_funct;

#endif

void int_to_string_buf(uint8_t u8,char **buf);
void int_to_string_buf(uint16_t u16,char **buf);
void int_to_string_buf(uint32_t u32,char **buf);
