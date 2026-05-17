#include "common.hpp"

#if FW_SERVER
AT24C32 eeprom(eeprom_iic_address, Wire);
#endif

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
OneButton btn_funct(FWPIN_FUNCT,true,false);

#endif

static char hexdig[17] = "0123456789ABCDEF";

void int_to_string_buf(uint8_t u8, char **buf){
    *((*buf)++) = hexdig[(u8&0xf0)>>4];
    *((*buf)++) = hexdig[(u8&0xf)];
}

// LSB only

void int_to_string_buf(uint16_t u16, char **buf){
    int_to_string_buf(uint8_t(u16>>8), buf);
    int_to_string_buf(uint8_t(u16), buf);
}

void int_to_string_buf(uint32_t u32, char **buf){
    int_to_string_buf(uint8_t(u32>>24), buf);
    int_to_string_buf(uint8_t(u32>>16), buf);
    int_to_string_buf(uint8_t(u32>>8), buf);
    int_to_string_buf(uint8_t(u32), buf);
}