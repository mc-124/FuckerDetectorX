#include "common.hpp"

AT24C32 eeprom(eeprom_iic_address, Wire);

float read_battery_voltage(){
    digitalWrite(FWPIN_EN_VBAT, 1); // 使能电池正极上拉电阻
    delay(1);                       // 电线电容充电
    uint16_t rawvbat = analogRead(FWPIN_VBAT);
    digitalWrite(FWPIN_EN_VBAT, 0); // 关闭电池正极上拉电阻
    return rawvbat*(3.3/4095)/2;
}

#if FW_SERVER
#else

Adafruit_SSD1306 oled(128,64,&Wire);

#endif