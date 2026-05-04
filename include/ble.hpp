#pragma once

#include "config.hpp"
#include "common.hpp"

#include <esp_mac.h>

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <BLEAdvertisedDevice.h>

#include "time.hpp"

enum class AdvertisingType:uint8_t {
    SERVER_ALARM,
    CLIENT_ALARM
};

struct AdvertisingData {
    AdvertisingType type;
    float battery_voltage;
    DaySeconds now;
};

#if FW_SERVER


#else

class AdvertisingDevCallbacks:public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override;
};
extern AdvertisingDevCallbacks advdev_callbacks;

#endif

void init_ble();
void set_advertising_data(const AdvertisingData& data);
void start_advertising();
void stop_advertising();