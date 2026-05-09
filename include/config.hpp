#pragma once

#define __PreCompile_Stringify(__x) #__x
#define STRINGIFY(__s) __PreCompile_Stringify(__s)

#pragma message("__cplusplus: " STRINGIFY(__cplusplus))
#pragma message("CORE_DEBUG_LEVEL: " STRINGIFY(CORE_DEBUG_LEVEL))

#if !defined(__cplusplus)||__cplusplus<202300
#error __cplusplus must >= c++23
#endif

#include <cstdint>
#include <cassert>

#ifndef FW_SERVER
#define FW_SERVER 1
#endif

static_assert(sizeof(int)==4,"arch error");

#define FIRMWARE_VERSION 26050902

/* Board: ESP32-C3 SuperMini/ProMini
   _____________
  |5V       IO5 |
  |GND      IO6 |
  |3V3      IO7 |
  |IO4      IO8 |
  |IO3      IO9 |
  |IO2      IO10|
  |IO1      IO20|
  |IO0      IO21|
   ^^^^^^^^^^^^^
*/

enum {
    FWPIN_VBAT=0,   // ADC
    FWPIN_EN_DEV=1, // HOLD
    FWPIN_FUNCT=2,  // WAKEUP

#if FW_SERVER
    FWPIN_SW_ALWAY_ENABLE=4,
    FWPIN_BTN_BEGIN_CLI=5,    // 需要 10k 上拉电阻防止误触发
#else
#endif
    FWPIN_IIC_SDA=6,
    FWPIN_IIC_SCL=7,
    FWPIN_LED=8,
    FWPIN_BOOT=9,
    FWPIN_EN_VBAT=10,    
    FWPIN_UART_RX=20,
    FWPIN_UART_TX=21,
};

#if FW_SERVER

constexpr uint8_t sleep_interval_array_size = 16;
constexpr uint8_t eeprom_iic_address = 0x50;
constexpr uint16_t sleep_interval_eeprom_address = 0xeff;
constexpr auto ble_device_name = "FuckerDetectorX Server";

static_assert(
    sleep_interval_eeprom_address+(sleep_interval_array_size*8)<4096,
    "eeprom address error"
);

#else

constexpr auto ble_device_name = "FuckerDetectorX Client";
constexpr uint8_t ssd1306_i2c_address = 0x3c;

#endif

constexpr size_t cmd_operator_bufsize = 15;
constexpr size_t cmd_paramlist_bufsize = 4;
constexpr size_t cmd_param_bufsize = 15;
constexpr size_t cmd_handler_maxcount = 16;
constexpr uint32_t rtos_task_stack_size = 1280; // 单位word
constexpr uint32_t advertising_timeout = 2500; // ms
constexpr uint32_t ld1040_init_time = 7500; // ms