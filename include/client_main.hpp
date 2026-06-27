#pragma once
#include "config.hpp"
#if !FW_SERVER

#include <Arduino.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <Adafruit_SSD1306.h>
#include <OneButton.h>

#include <atomic>
#include <cstring>

#include "error.hpp"
#include "common.hpp"
#include "time.hpp"
#include "ble.hpp"
#include "serial_cmd.hpp"
#include "public.hpp"

struct AdvDeviceInfo {
    bool valid;
    bool is_server;
    char rssi;
    uint16_t mac_hash;
    DaySeconds now_time;
    float decoded_vbat;
};

extern Adafruit_SSD1306 oled;
extern OneButton btn_funct;

extern std::atomic_bool scanner_thisround_found;
extern std::atomic_bool thisloop_transmitadv;
extern std::atomic<uint8_t> dst_last_update_ui;
extern AdvDeviceInfo found_dev_lst[found_dev_lst_size];

extern TaskHandle_t task_buttonloop;
extern TaskHandle_t task_alarmloop;

extern AdvDeviceInfo scan_result;

void tf_buttonloop(void*);
void tf_alarmloop(void*);
void active_alarm();

void init_client_io();
void init_client_data();
void init_client_devices();
void init_client_tasks();

void update_ui();
void funct_doubleclick();
void loop();

#endif