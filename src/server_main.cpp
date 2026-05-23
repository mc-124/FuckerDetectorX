#include "server_main.hpp"
#if FW_SERVER

TaskHandle_t th_server_main;

void tf_server_main(void*){
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    log_i("wakeup_cause=%d", wakeup_cause);
    if (wakeup_cause==ESP_SLEEP_WAKEUP_GPIO){
        // 雷达触发
        log_i("wakeup by LD1040");
        transmit_advertising(AdvertisingType::SERVER_ALARM);
    }

    // 计算睡眠时间

    DaySeconds sleep_time = 0;
    bool dev_enabled = false;
    const DaySeconds now = get_dayseconds();
    if (!digitalRead(FWPIN_SW_ALWAY_ENABLE)){
        log_i("alway enable");
        dev_enabled = true;
        sleep_time = DaySeconds(30) - now;
    }
    else {
        SleepInterval* si = find_in_progress_interval(now);
        if (si){
            dev_enabled = true;
            sleep_time = si->end_sec - now;
            log_i("found in progress interval: (%d, %d)", si->start_sec, si->end_sec);
        }
        else {
            si = find_next_sleep_interval(now);
            if (si){
                sleep_time = si->start_sec - now;
                log_i("found next interval: (%d, %d)", si->start_sec, si->end_sec);
            }
            else {
                log_w("avaliable interval not found");
                sleep_time = DaySeconds(30) - now;
                dev_enabled = true;
            }
        }
    }
    log_i("sleep_time=%d", sleep_time);
    server_begin_sleep(dev_enabled, sleep_time);
    vTaskDelete(NULL);
}

void server_begin_sleep(bool dev_enabled, const DaySeconds sleep_time){
    deinit_ble();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    gpio_hold_dis(static_cast<gpio_num_t>(FWPIN_EN_DEV));
    if (dev_enabled){
        if (!digitalRead(FWPIN_EN_DEV)){
            log_i("enable dev");
            digitalWrite(FWPIN_EN_DEV, 1);
            esp_sleep_enable_timer_wakeup(ld1040_init_time);
            gpio_hold_en(static_cast<gpio_num_t>(FWPIN_EN_DEV));
            esp_light_sleep_start();
            gpio_hold_dis(static_cast<gpio_num_t>(FWPIN_EN_DEV));
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        }
        esp_deep_sleep_enable_gpio_wakeup(1ULL<<uint32_t(FWPIN_FUNCT), ESP_GPIO_WAKEUP_GPIO_HIGH);
    }
    else {
        digitalWrite(FWPIN_EN_DEV, 0);
    }
    Serial.flush();
    Serial.end();
    Wire.flush();
    Wire.end();
    gpio_hold_en(static_cast<gpio_num_t>(FWPIN_EN_DEV));
    esp_sleep_enable_timer_wakeup(static_cast<int>(sleep_time)*1000000);
    esp_deep_sleep_start();        
}

void init_server_io(){
    esp_reset_reason_t reset_reason = esp_reset_reason();
    gpio_hold_dis(static_cast<gpio_num_t>(FWPIN_EN_DEV));
    if (reset_reason==ESP_RST_DEEPSLEEP){
        log_i("reset_reason: DEEPSLEEP");
    }
    else {
        log_i("reset_reason: %d", reset_reason);
        pinMode(FWPIN_EN_DEV, OUTPUT);
        gpio_deep_sleep_hold_en();
        load_sleep_intervals();
    }
}

void init_server_devices(){
    init_rtc();
}

void init_server_task(){
    auto status = xTaskCreate(tf_server_main, "srvmain", srvmain_task_stack_size, NULL, 1, &th_server_main);
    if (status!=pdPASS){
        log_e("create task \"srvmain\" failed: %d", status);
        ESP_ERROR_CHECK(EXC_CREATE_TASK_FAILED);
    }
}

//void server_loop(){}

#endif