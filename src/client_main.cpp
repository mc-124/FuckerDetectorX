#include "client_main.hpp"
#if !FW_SERVER

Adafruit_SSD1306 oled(128,64,&Wire);
OneButton btn_funct(FWPIN_FUNCT,true,false);

std::atomic_bool scanner_thisround_found = false;
std::atomic_bool thisloop_transmitadv = false;
std::atomic<uint8_t> dst_last_update_ui = 255;
AdvDeviceInfo found_dev_lst[found_dev_lst_size];

TaskHandle_t task_buttonloop;
TaskHandle_t task_alarmloop;

AdvDeviceInfo scan_result;

void tf_buttonloop(void*){
    for(;;){
        btn_funct.tick();
        delay(buttonloop_delay);
    }
}

void tf_alarmloop(void*){
    static uint32_t last_alarm = seconds();
    for(;;){
        xTaskNotifyWait(0,0,NULL,portMAX_DELAY);
        uint32_t now = seconds();
        if (now-last_alarm<alarm_timeout){
            continue;
        }
        else {
            last_alarm = now;
        }
        for(uint8_t i=0;i<vibration_loop_number;i++){
            digitalWrite(FWPIN_EN_DEV, 1);
            led(1);
            delay(vibration_en_time);
            digitalWrite(FWPIN_EN_DEV, 0);
            led(0);
            delay(vibration_dis_time);
        }
    }
}

void active_alarm(){
    xTaskNotifyGive(task_alarmloop);
}

void AdvertisingDevCallbacks::onResult(BLEAdvertisedDevice dev){
    if (!scanner_thisround_found
        &&dev.haveName()&&dev.haveManufacturerData()
        &&(dev.getName()==ble_client_name||dev.getName()==ble_server_name)
       ){
        String raw_manfacture_data = dev.getManufacturerData();
        if (raw_manfacture_data.length()==sizeof(AdvertisingData)){
            AdvertisingData data;
            memcpy(&data, raw_manfacture_data.c_str(), sizeof(AdvertisingData));
            if (data.company_id==adv_company_id){
                uint64_t mac = 0;
                auto raw_mac = dev.getAddress();
                memcpy(&mac, raw_mac.getNative(), 6);
                scan_result.valid = true;
                scan_result.is_server = data.type==AdvertisingType::SERVER_ALARM;
                scan_result.now_time = int(data.now)%86400;
                if (int(scan_result.now_time)<0){
                    scan_result.now_time = DaySeconds(int(scan_result.now_time)+86400);
                }
                scan_result.mac_hash = mac%0xffff;
                scan_result.decoded_vbat = decode_battery_voltage(data.battery_voltage);
                if (dev.haveRSSI()){
                    int raw_rssi = dev.getRSSI();
                    if (raw_rssi>127){
                        scan_result.rssi = 127;
                    }
                    else if (raw_rssi<-128){
                        scan_result.rssi = -128;
                    }
                    else {
                        scan_result.rssi = static_cast<char>(raw_rssi);
                    }
                }
                else {
                    scan_result.rssi = -128;
                }
                active_alarm();
                scanner_thisround_found = true;
                return;
            }
        }
    }
}

void init_client_io(){
    led(0);
    pinMode(FWPIN_EN_DEV, OUTPUT);
    if (!Wire.begin(FWPIN_IIC_SDA, FWPIN_IIC_SCL)){
        log_e("init iic failed");
        ESP_ERROR_CHECK(EXC_INIT_I2C_FAILED);
    }
}

void init_client_devices(){
    log_i("init device");
    if (!oled.begin(SSD1306_SWITCHCAPVCC, ssd1306_i2c_address)){
        log_e("init SSD1306 failed");
        ESP_ERROR_CHECK(EXC_INIT_OLED_FAILED);
    }
    btn_funct.attachDoubleClick(funct_doubleclick);
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(10,20);
    oled.println("FuckerDetectorX");
    oled.setCursor(10,oled.getCursorY());
    oled.println("V: " STRINGIFY(FIRMWARE_VERSION));
    oled.invertDisplay(true);
    oled.display();
    delay(3000);
    oled.invertDisplay(false);
    oled.clearDisplay();
    oled.setCursor(0,0);
    oled.println("LOADING...");
    oled.display();
    //oled.clearDisplay();
    //oled.setCursor(0,0);
    //oled.setTextColor(WHITE);
    //oled.println("Test");
    //oled.display();
}

void init_client_data(){
    memset(found_dev_lst, 0, sizeof(found_dev_lst));
}

void init_client_tasks(){
    xTaskCreate(tf_buttonloop, "btnloop", buttonloop_task_stack_size, NULL, NULL, &task_buttonloop);
    xTaskCreate(tf_alarmloop, "almloop", alarmloop_task_stack_size, NULL, NULL, &task_alarmloop);
}

void update_ui(){
    log_i("updating ui");
    auto get_rssi_level_char = [](char rssi){
        if      (rssi>-40){return '9';}
        else if (rssi>-50){return '8';}
        else if (rssi>-60){return '7';}
        else if (rssi>-65){return '6';}
        else if (rssi>-70){return '5';}
        else if (rssi>-75){return '4';}
        else if (rssi>-80){return '3';}
        else if (rssi>-85){return '2';}
        else if (rssi>-90){return '1';}
        else return '0';
    };
    int cur = -1;
    oled.clearDisplay();
    oled.setCursor(0,0);
    //oled.println("Test");
    oled.printf("["STRINGIFY(FIRMWARE_VERSION)"] BAT:% 4.2fV\r\n", read_battery_voltage());
    if (scanner_thisround_found){
        int min = -1;
        for (uint8_t i=0;i<found_dev_lst_size;i++){
            AdvDeviceInfo& dev = found_dev_lst[i];
            if (!dev.valid) continue;
            if (scan_result.mac_hash==dev.mac_hash){
                memcpy(&dev, &scan_result, sizeof(AdvDeviceInfo));
                cur = i;
                break;
            }
            else if (min==-1||(found_dev_lst+min)->now_time>dev.now_time){
                min = i;
            }
        }
        if (cur==-1){
            min = (min==-1)?0:min;
            memcpy(found_dev_lst+min, &scan_result, sizeof(AdvDeviceInfo));
            cur = min;
        }
        scanner_thisround_found = false;
    }
    for (uint8_t i=0;i<found_dev_lst_size;i++){
        AdvDeviceInfo& dev = found_dev_lst[i];
        if (!dev.valid){
            oled.println();
            continue;
        }
        if (cur==i){
            oled.setTextColor(SSD1306_BLACK,SSD1306_WHITE);
        }
        if (dev.is_server){
            oled.printf("<%04x> %02hhu:%02hhu % 4.2fV %c", 
                dev.mac_hash, 
                dev.now_time.hours(), 
                dev.now_time.minute(), 
                dev.decoded_vbat, 
                get_rssi_level_char(dev.rssi)
            );
        }
        else {
            oled.printf("<%04x>CLIENT % 4hhddBm",
                dev.mac_hash,
                dev.rssi
            );
        }
        if (cur==i){
            oled.setTextColor(SSD1306_WHITE,SSD1306_BLACK);
        }
        oled.println();
    }
    oled.display();
}

void funct_doubleclick(){
    log_i("btn_funct dbclk");
    thisloop_transmitadv = true;
}

void loop(){
    if (thisloop_transmitadv){
        log_i("transmitting adv");
        led(1);
        oled.clearDisplay();
        oled.setCursor(5, 24);
        oled.println("TRANSMITTING ALARM");
        oled.display();
        transmit_advertising(AdvertisingType::CLIENT_ALARM);
        delay(500);
        update_ui();
        led(0);
        dst_last_update_ui = 0;
        scanner_thisround_found = false;
        thisloop_transmitadv = false;
    }
    else {
        p_blescan->start(1);
        if (scanner_thisround_found||dst_last_update_ui>update_ui_max_dst){
            dst_last_update_ui = 0;
            update_ui();
            scanner_thisround_found = false;
        }
        else {
            dst_last_update_ui++;
        }
    }
}

#endif