#include "common.hpp"

#include "adcplus.hpp"

float read_battery_voltage(){
    //digitalWrite(FWPIN_EN_VBAT, 1); // 使能电池正极上拉电阻
    //delay(1);                       // 电线电容充电
    //uint16_t rawvbat = analogRead(FWPIN_VBAT);
    ////digitalWrite(FWPIN_EN_VBAT, 0); // 关闭电池正极上拉电阻
    //float f32vbat = rawvbat*(3.3/4095)*2;
    //if constexpr(vbat_mul) {
    //    f32vbat *= vbat_mul;
    //}
    //if constexpr(vbat_add) {
    //    f32vbat += vbat_add;
    //}
    //return f32vbat; // 1/2分压
    float vb = read_adc_voltage(static_cast<gpio_num_t>(FWPIN_VBAT))*2;
    if constexpr(vbat_mul){
        vb *= vbat_mul;
    }
    return vb;
}

static char hexdig[17] = "0123456789ABCDEF";

void int_to_string_buf(uint8_t u8, char **buf){
    *((*buf)++) = hexdig[(u8&0xf0)>>4];
    *((*buf)++) = hexdig[(u8&0xf)];
}

// LSB only

//void int_to_string_buf(uint16_t u16, char **buf){
//    int_to_string_buf(uint8_t(u16>>8), buf);
//    int_to_string_buf(uint8_t(u16), buf);
//}
//
//void int_to_string_buf(uint32_t u32, char **buf){
//    int_to_string_buf(uint8_t(u32>>24), buf);
//    int_to_string_buf(uint8_t(u32>>16), buf);
//    int_to_string_buf(uint8_t(u32>>8), buf);
//    int_to_string_buf(uint8_t(u32), buf);
//}