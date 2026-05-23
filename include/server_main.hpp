#pragma once
#include "config.hpp"
#if FW_SERVER

#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <Arduino.h>
#include <at24c32.h>

#include "error.hpp"
#include "common.hpp"
#include "time.hpp"
#include "serial_cmd.hpp"
#include "ble.hpp"
#include "public.hpp"

// 把服务器主函数和loopTask分离，避免阻塞loopTask导致蓝牙栈错误

extern TaskHandle_t th_server_main;

void tf_server_main(void*);
void server_begin_sleep(bool dev_enabled, const DaySeconds sleep_time);

void init_server_io();
void init_server_devices();
void init_server_task();

//void server_loop();

#endif