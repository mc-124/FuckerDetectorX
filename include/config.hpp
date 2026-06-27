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

#if __IN_VSCODE
#warning __IN_VSCODE

//////////////////////////////
#define FW_SERVER 0
//////////////////////////////

#else
#ifndef FW_SERVER
#error FW_SERVER is undefined
#endif
#endif

#if FW_SERVER
#define FW_TYPE_STRING "Server"
#else
#define FW_TYPE_STRING "Client"
#endif

#define SSD1306_NO_SPLASH 1

static_assert(sizeof(int)==4,"arch error");

// Year[2] Month[2] Day[2] SubVersion[2]
#define FIRMWARE_VERSION 26061903

#define FW_REPO_URL "https://github.com/mc-124/FuckerDetectorX"

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
    FWPIN_FUNCT=0,  // WAKEUP SOURCE
    FWPIN_VBAT=1,   // ADC
    // pin 2: strapping
    #if FW_SERVER
    FWPIN_SW_ALWAY_ENABLE=3,
    #else
    #endif
    FWPIN_EN_DEV=5, // HOLD
    FWPIN_IIC_SDA=6,
    FWPIN_IIC_SCL=7,
    FWPIN_LED=8,
    FWPIN_BOOT=9,
    FWPIN_BTN_BEGIN_CLI=10,    // 需要 10k 上拉电阻防止误触发
    //FWPIN_EN_VBAT=10,    
    FWPIN_UART_RX=20,
    FWPIN_UART_TX=21,
};

constexpr char ble_server_name[] = "FuckerDetectorX";
//constexpr char ble_client_name[] = "FuckerDetectorX";
#define ble_client_name ble_server_name

constexpr float vbat_mul = 1.033;

#if FW_SERVER

constexpr uint8_t sleep_interval_array_size = 16;
constexpr uint8_t eeprom_iic_address = 0b1010111;
constexpr uint16_t sleep_interval_eeprom_address = 0xeff;
constexpr auto ble_device_name = ble_server_name;
constexpr uint32_t ld1040_init_time = 7500; // ms
constexpr char datetime_format[] = "YYYY-MM-DD hh:mm:ss";
constexpr uint32_t srvmain_task_stack_size = 8192*4;

static_assert(
    sleep_interval_eeprom_address+(sleep_interval_array_size*8)<4096,
    "eeprom address error"
);

constexpr uint32_t cpu_freq_mhz = 80;

#else

constexpr uint32_t buttonloop_delay = 10;
constexpr uint32_t vibration_freq = 72;
constexpr uint32_t vibration_en_time = 1;
constexpr uint32_t vibration_dis_time = 4;
constexpr uint32_t vibration_timeout = 96;
constexpr uint8_t vibration_loop_number = 3;

constexpr auto ble_device_name = ble_client_name;
constexpr uint8_t ssd1306_i2c_address = 0x3c;
constexpr uint32_t scan_interval = 120;
constexpr uint32_t scan_window = 100;
constexpr uint32_t scanresult_queue_length = 4;
constexpr uint8_t found_dev_lst_size = 7;

constexpr uint32_t buttonloop_task_stack_size = 512*4;
constexpr uint32_t alarmloop_task_stack_size = 512*4;

constexpr uint8_t update_ui_max_dst = 16;
constexpr uint32_t alarm_timeout = 5;

constexpr uint32_t cpu_freq_mhz = 80;
// 不知道为什么改成 40MHz 会卡住

#endif

constexpr size_t adv_company_id = 0xff23;
constexpr size_t cmd_operator_bufsize = 15;
constexpr size_t cmd_paramlist_bufsize = 4;
constexpr size_t cmd_param_bufsize = 15;
constexpr size_t cmd_handler_maxcount = 16;
constexpr uint32_t advertising_timeout = 2500; // ms
constexpr uint32_t battery_voltage_cache_timeout = 15; // s