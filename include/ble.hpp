#pragma once

#include "config.hpp"

#include <esp_mac.h>

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include <BLEAdvertisedDevice.h>

#include "common.hpp"
#include "time.hpp"

enum class AdvertisingType:uint8_t {
    SERVER_ALARM,
    CLIENT_ALARM
};

struct AdvertisingData {
    uint16_t company_id;
    AdvertisingType type;   // enum class:uint8_t
    uint8_t battery_voltage;
    DaySeconds now;         // 4b
};

constexpr float decode_battery_voltage(uint8_t cvbat){
    return (cvbat/100.0)+2.0;
}

static_assert(sizeof(AdvertisingData)==8, "data error");

#if FW_SERVER
#else

class AdvertisingDevCallbacks:public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override;
};
extern BLEScan* p_blescan;
extern AdvertisingDevCallbacks advdev_callbacks;

#endif

extern uint16_t adv_max_interval;
extern uint16_t adv_min_interval;

extern BLEServer* p_bleserver;
extern BLEAdvertising* p_bleadvertising;

static_assert(
    (0
        +sizeof(ble_server_name)-1+2    // Name
        +sizeof(AdvertisingData)+2      // MaufacturerData
        +3                              // TxPower
    )<=31,
    "server advertising data too big"
);

static_assert(
    (0
        +sizeof(ble_client_name)-1+2    // Name
        +sizeof(AdvertisingData)+2      // MaufacturerData
        +3                              // TxPower
    )<=31,
    "client advertising data too big"
);

void init_ble();
void deinit_ble();
void set_advertising_data(const AdvertisingData& data);
void start_advertising();
void stop_advertising();
void init_advertising_data(AdvertisingData& data, const AdvertisingType type);
void bleaddr_tostrbuf(uint8_t* bytes, char* buf); // buf: char[18]