#include "ble.hpp"




#if FW_SERVER




#else

BLEScan* p_blescan = nullptr;
AdvertisingDevCallbacks advdev_callbacks + AdvertisingDevCallbacks();

#endif

uint16_t adv_max_interval = 0;
uint16_t adv_min_interval = 0;

BLEServer* p_bleserver = nullptr;
BLEAdvertising* p_bleadvertising = nullptr;


static void generate_advertising_intervals(){
    uint8_t macbuf[6];
    esp_read_mac(macbuf, ESP_MAC_BT);
    adv_max_interval = 128+(macbuf[1]%64);
    adv_min_interval = 64+(macbuf[2]%64);
}

void init_ble(){
    log_i("Init BLE");
    BLEDevice::init(ble_device_name);
    BLEDevice::setPower(ESP_PWR_LVL_P20); // 最大发射功率 +21被废弃
    p_bleserver = BLEDevice::createServer();
    p_bleadvertising = p_bleserver->getAdvertising();
    p_bleadvertising->setName(ble_device_name);
    generate_advertising_intervals();
    p_bleadvertising->setMaxInterval(adv_max_interval);
    p_bleadvertising->setMinInterval(adv_min_interval);
#if FW_SERVER
#else
    p_scan = BLEDevice::getScan();
#endif
}

void deinit_ble(){
    log_i("Deinit BLE");
    BLEDevice::deinit(true);
    p_bleserver = nullptr;
    p_bleadvertising = nullptr;
#if !FW_SERVER
    p_blescan = nullptr;
#endif
}

void set_advertising_data(const AdvertisingData& data){
    if (!p_bleadvertising){
        log_e("set_advertising_data: p_bleadvertising is nullptr");
        return;
    }
    const char* buf = reinterpret_cast<const char*>(&data);
    BLEAdvertisementData bleadvdata;
    bleadvdata.setName(ble_device_name);
    bleadvdata.addData(String(buf,sizeof(AdvertisingData)));
    p_bleadvertising->setAdvertisementData(bleadvdata);
}

void start_advertising(){
    if (!p_bleadvertising){
        log_e("start_advertising: p_bleadvertising is nullptr");
        return;
    }
    p_bleadvertising->start();
}

void stop_advertising(){
    if (!p_bleadvertising){
        log_e("stop_advertising: p_bleadvertising is nullptr");
        return;
    }
    p_bleadvertising->stop();
}

#if FW_SERVER
#else

#endif