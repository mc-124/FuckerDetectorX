#pragma once

#define __PreCompile_Stringify(__x) #__x
#define STRINGIFY(__s) __PreCompile_Stringify(__s)

#include <cstdint>
#include <cassert>
#include <Arduino.h>

#if CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_NONE
#define FW_LOG_LEVEL "NONE"
#elif CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_ERROR
#define FW_LOG_LEVEL "ERROR"
#elif CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_WARN
#define FW_LOG_LEVEL "WARN"
#elif CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_INFO
#define FW_LOG_LEVEL "INFO"
#elif CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_DEBUG
#define FW_LOG_LEVEL "DEBUG"
#elif CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_VERBOSE
#define FW_LOG_LEVEL "VERBOSE"
#else
#error Unknown debug level
#endif

#ifdef __CONFIGHPP_PRINT_INFO
#pragma message("__cplusplus: " STRINGIFY(__cplusplus))
#pragma message("CORE_DEBUG_LEVEL: " STRINGIFY(CORE_DEBUG_LEVEL))
#pragma message("ARDUINO: " STRINGIFY(ARDUINO))
#pragma message("ESP_IDF_VERSION: " STRINGIFY(ESP_IDF_VERSION_MAJOR) "." STRINGIFY(ESP_IDF_VERSION_MINOR) "." STRINGIFY(ESP_IDF_VERSION_PATCH))

#pragma message("FW_LOG_LEVEL: " FW_LOG_LEVEL)

#endif

#if !defined(__cplusplus)||__cplusplus<202300
#error __cplusplus must >= c++23
#endif

#ifndef FW_SERVER
#warning FW_SERVER is undefined
#define FW_SERVER 1
#endif

#if FW_SERVER
#define FW_TYPE_STRING "Server"
#else
#define FW_TYPE_STRING "Client"
#endif

static_assert(sizeof(int)==4,"arch error");

// Year[2] Month[2] Day[2] SubVersion[2]
#define FIRMWARE_VERSION 26051700

/* Board: ESP32-C3 SuperMini/ProMini
   _____________
  |IO5       5V |
  |IO6       GND|
  |IO7       3V3|
  |IO8       IO4|
  |IO9       IO3|
  |IO10      IO2|
  |IO20      IO1|
  |IO21      IO0|
   ^^^^^^^^^^^^^
*/

enum {
    FWPIN_VBAT=0,   // ADC
    FWPIN_FUNCT=1,  // WAKEUP SOURCE
    // pin 2: strapping
    FWPIN_BTN_BEGIN_CLI=3,    // 需要 10k 上拉电阻防止误触发
#if FW_SERVER
    FWPIN_SW_ALWAY_ENABLE=4,
#else
#endif
    FWPIN_EN_DEV=5, // HOLD
    FWPIN_IIC_SDA=6,
    FWPIN_IIC_SCL=7,
    FWPIN_LED=8,
    FWPIN_BOOT=9,
    FWPIN_EN_VBAT=10,    
    FWPIN_UART_RX=20,
    FWPIN_UART_TX=21,
};

constexpr char ble_server_name[] = "FuckerDetectorX";
//constexpr char ble_client_name[] = "FuckerDetectorX";
#define ble_client_name ble_server_name

#if FW_SERVER

constexpr uint8_t sleep_interval_array_size = 16;
constexpr uint8_t eeprom_iic_address = 0x50;
constexpr uint16_t sleep_interval_eeprom_address = 0xeff;
constexpr auto ble_device_name = ble_server_name;
constexpr uint32_t ld1040_init_time = 7500; // ms
constexpr char datetime_format[] = "YYYY-MM-DD hh:mm:ss";

static_assert(
    sleep_interval_eeprom_address+(sleep_interval_array_size*8)<4096,
    "eeprom address error"
);

#else

constexpr auto ble_device_name = ble_client_name;
constexpr uint8_t ssd1306_i2c_address = 0x3c;
constexpr uint32_t scan_interval = 48;
constexpr uint32_t scanner_task_stack_size = 8192; // word
constexpr uint32_t alarm_task_stack_size = 384;
constexpr uint8_t found_ble_device_array_length = 6;
constexpr uint32_t ui_task_stack_size = 2560;

#endif

constexpr size_t adv_company_id = 0xff23;
constexpr size_t cmd_operator_bufsize = 15;
constexpr size_t cmd_paramlist_bufsize = 4;
constexpr size_t cmd_param_bufsize = 15;
constexpr size_t cmd_handler_maxcount = 16;
constexpr uint32_t rtos_task_stack_size = 1280; // 单位word
constexpr uint32_t advertising_timeout = 2500; // ms
