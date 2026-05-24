#include "client_main.hpp"
#if !FW_SERVER

SemaphoreHandle_t lock_ble;
SemaphoreHandle_t lock_devlst;
SemaphoreHandle_t lock_devcli;
QueueHandle_t mail_diupdate;
TaskHandle_t th_alarm;
TaskHandle_t th_scanner;
TaskHandle_t th_ui;
TaskHandle_t th_diupdate;

std::atomic<bool> thisround_is_founded = false;

AdDeviceInfo devsrv_info[devsrv_lst_len];
AdDeviceInfo devcli_info;

void tf_alarm(void*){
    log_i("task is running: tf_alarm");
    auto alarming = [](bool state){        
        digitalWrite(FWPIN_EN_DEV, state);
        led(state);
    };
    for (;;){
        xTaskNotifyWait(0,0,NULL,portMAX_DELAY);
        log_i("alarming");
        for (uint8_t i=0;i<3;i++){
            alarming(1);
            delay(600);
            alarming(0);
            delay(200);
        }
        delay(600); // total: 30000s
        thisround_is_founded = false;
    }
}

void tf_scanner(void*){
    log_i("task is running: tf_scanner");
    for (;;){
        xSemaphoreTake(lock_ble, portMAX_DELAY);
        log_i("scanning");
        p_blescan->start(0);
        log_i("scan stopped");
        xSemaphoreGive(lock_ble);
        delay(1);
    }
}

void tf_ui(void*){
    log_i("task is running: tf_ui");
    for (;;){
        xTaskNotifyWait(0,0,NULL,60*1000);
        log_i("update ui");
        float host_vbat = read_battery_voltage_cached();
        char mac_str_buf[5];
        mac_str_buf[4] = 0;
        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.printf("FDX-" STRINGIFY(FIRMWARE_VERSION)"   % 4.2fV\r\n", host_vbat);
        xSemaphoreTake(lock_devlst, portMAX_DELAY);
        for (auto& info:devsrv_info){
            if (info.valid&&0<=int(info.time)&&int(info.time)>86400){
                char* p = mac_str_buf;
                int_to_string_buf(uint8_t(info.mac_0), &p);
                int_to_string_buf(uint8_t(info.mac_1), &p);
                oled.printf("%s [%02hhu:%02hhu]   %4.2fV\r\n", mac_str_buf, info.mac_0, info.mac_1,
                    decode_battery_voltage(info.coded_vbat)
                );
            }
            else {
                Serial.println();
            }
        }
        xSemaphoreGive(lock_devlst);
        xSemaphoreTake(lock_devcli,portMAX_DELAY);
        if (devcli_info.valid){
            char* p = mac_str_buf;
            int_to_string_buf(uint8_t(devcli_info.mac_0), &p);
            int_to_string_buf(uint8_t(devcli_info.mac_1), &p);
            oled.printf("%s         % 3hhddBm", mac_str_buf, -devcli_info.rssi);
        }
        xSemaphoreGive(lock_devlst);
        oled.display();
    }
}

void tf_diupdate(void*){
    log_i("task is running: tf_diupdate");
    for (;;){
        AdDeviceInfo devinfo;
        xQueueReceive(mail_diupdate, &devinfo, portMAX_DELAY);
        if (devinfo.valid){
            if (devinfo.is_server){
                xSemaphoreTake(lock_devlst, portMAX_DELAY);
                for (auto& info:devsrv_info){
                    if (!info.valid){ // empty
                        memcpy(&info, &devinfo, sizeof(AdDeviceInfo));
                        break;
                    }
                }
                xSemaphoreGive(lock_devlst);
            }
            else {
                xSemaphoreTake(lock_devcli, portMAX_DELAY);
                memcpy(&devcli_info, &devinfo, sizeof(AdDeviceInfo));
                xSemaphoreGive(lock_devcli);
            }
            update_ui();
        }
    }
}

void update_ui(){
    xTaskNotify(th_ui, NULL, eNoAction);
}

void active_alarm(){
    xTaskNotify(th_alarm, NULL, eNoAction);
}

void update_devs(const AdDeviceInfo& info){
    xQueueSend(mail_diupdate, &info, 0);
}

void AdvertisingDevCallbacks::onResult(BLEAdvertisedDevice dev){
    if (thisround_is_founded){
        return;
    }
    if (dev.haveName()&&dev.haveManufacturerData()&&dev.haveTXPower()&&!dev.haveAppearance()&&!dev.haveServiceUUID()){
        String name = dev.getName();
        String rawdata = dev.getManufacturerData();
        if (rawdata.length()==sizeof(AdvertisingData)
            &&rawdata[0]==lowByte(adv_company_id)&&rawdata[1]==highByte(adv_company_id)
            &&(name==ble_server_name||name==ble_client_name)
        ){
            const AdvertisingData* data = reinterpret_cast<const AdvertisingData*>(rawdata.c_str());
            uint8_t mac_buf[6];
            memcpy(mac_buf, dev.getAddress().getNative(), 6);
            int l_rssi = dev.haveRSSI()?dev.getRSSI():-127;
            char rssi;
            if (l_rssi>127){
                rssi = 127;
            }
            else if (l_rssi<-127){
                rssi = -127;
            }
            else {
                rssi = l_rssi;
            }
            AdDeviceInfo info = {
                true,
                (data->type==AdvertisingType::SERVER_ALARM),
                data->battery_voltage,
                rssi,
                mac_buf[4],
                mac_buf[5]
            };
            info.time = data->now;
            update_devs(info);
            thisround_is_founded = true;
            active_alarm();
        }
    }
}

void init_client_io(){

}

void init_client_devices(){
    log_i("init device");
    if (!oled.begin(SSD1306_SWITCHCAPVCC, ssd1306_i2c_address)){
        log_e("init SSD1306 failed");
        ESP_ERROR_CHECK(EXC_INIT_OLED_FAILED);
    }
    btn_funct.attachDoubleClick(funct_doubleclick);
}

void init_client_data(){
    for (auto& i:devsrv_info){
        i.valid = false;
    }
    devcli_info.valid = false;
    log_i("create queue and mutex");
    lock_ble = xSemaphoreCreateMutex();
    lock_devlst = xSemaphoreCreateMutex();
    lock_devcli = xSemaphoreCreateMutex();
    mail_diupdate = xQueueCreate(2, sizeof(AdDeviceInfo));
}

void init_client_tasks(){
    BaseType_t status;
    auto errchk = [status](const char* task_name){
        if (status!=pdPASS){
            log_e("create task \"%s\" failed: %d", task_name, task_name);
            ESP_ERROR_CHECK(EXC_CREATE_TASK_FAILED);
        }
        else {
            log_i("task created: %s", task_name);
        }
    };
    log_i("create task");
    status = xTaskCreate(tf_alarm, "alarm", alarm_task_stack_size, NULL, 1, &th_alarm);
    errchk("alarm");
    status = xTaskCreate(tf_scanner, "scanner", scanner_task_stack_size, NULL, 1, &th_scanner);
    errchk("scanner");
    status = xTaskCreate(tf_ui, "ui", ui_task_stack_size, NULL, 1, &th_ui);
    errchk("ui");
    status = xTaskCreate(tf_diupdate, "diupdate", diupdate_task_stack_size, NULL, 1, &th_diupdate);
    errchk("diupdate");
}

void client_loop(){
    btn_funct.tick();
}

void funct_doubleclick(){
    transmit_advertising(AdvertisingType::CLIENT_ALARM);
}

#endif