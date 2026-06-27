#define __CONFIGHPP_PRINT_INFO

#include <Arduino.h>

#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>

#include "public.hpp"
#include "server_main.hpp"
#include "client_main.hpp"

#include <utility>
#include <atomic>
#include <cstring>
#include <cerrno>

static_assert(ARDUHAL_LOG_LEVEL!=ARDUHAL_LOG_LEVEL_NONE,"log level error");

RTC_DATA_ATTR uint32_t chip_boot_counter = 0;

void setup(){
    setCpuFrequencyMhz(cpu_freq_mhz);
    chip_boot_counter++;
    Serial.begin(115200);
    delay(5);
    Serial.println("FIRMWARE_VERSION: "STRINGIFY(FIRMWARE_VERSION));
    Serial.printf("BOOT_COUNTER: %u\r\n", chip_boot_counter);
    Serial.println("DEBUG_LEVEL: "FW_LOG_LEVEL);
    init_pub_io();
    init_pub_devices();
    init_cli();
#if FW_SERVER
    init_server_io();
    init_server_devices();
    init_server_task();
#else
    init_client_io();
    init_client_data();
    init_client_devices();
    init_client_tasks();
#endif
    log_i("setup returned");
}

// loop() 被移到 client_main 和 server_main