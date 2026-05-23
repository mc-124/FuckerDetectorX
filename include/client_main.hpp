#pragma once
#include "config.hpp"
#if !FW_SERVER

#include <Arduino.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_system.h>

#include <atomic>
#include <cstring>

#include "error.hpp"
#include "common.h"
#include "time.hpp"
#include "ble.hpp"
#include "serial_cmd.hpp"
#include "public.hpp"

struct AdDeviceInfo {
    // byte 0:4
    bool valid;
    bool is_server;
    char coded_vbat;
    char rssi;
    // byte 4:8
    uint8_t mac_0; // xx:xx:xx:xx:XX:xx
    uint8_t mac_1; // xx:xx:xx:xx:xx:XX
    char __padding[2];
    // byte 8:12
    DaySeconds time;
};

static_assert(sizeof(AdDeviceInfo)==12, "struct AdDeviceInfo size error");

extern SemaphoreHandle_t lock_ble;
extern SemaphoreHandle_t lock_devlst;
extern SemaphoreHandle_t lock_devcli;
extern QueueHandle_t mail_diupdate;
extern TaskHandle_t th_alarm;       // tf_alarm
extern TaskHandle_t th_scanner;     // tf_scanner
extern TaskHandle_t th_ui;          // tf_ui
extern TaskHandle_t th_diupdate;    // tf_diupdate

extern std::atomic<bool> thisround_is_founded;

extern AdDeviceInfo devsrv_info[devsrv_lst_len];
extern AdDeviceInfo devcli_info;

void tf_alarm(void*);
void tf_scanner(void*);
void tf_ui(void*);
void tf_diupdate(void*);

void update_ui();
void active_alarm();
void update_devs(const AdDeviceInfo& info);

void init_client_io();
void init_client_data();
void init_client_devices();
void init_client_tasks();

void client_loop();

void funct_doubleclick();

#endif