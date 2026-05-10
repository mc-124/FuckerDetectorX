#include <Arduino.h>

#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>

#include "error.hpp"
#include "common.hpp"
#include "config.hpp"
#include "time.hpp"
#include "ble.hpp"
#include "serial_cmd.hpp"

#include <utility>
#include <cstring>
#include <cerrno>

RTC_DATA_ATTR uint32_t chip_boot_counter = 0;

#define DEF_CMD_HANDLER(__funcname) void __funcname (size_t param_count, const CommandHandlerFuncParams& params)
#define CMD_CHK_LEN(__len) if(param_count!=__len){log_e("syntax error: param count error");return;}
#define CMD_PARSE_INT_PARAM(__pindex) \
    char* param##__pindex##_str = command_paramlist_buffer[__pindex];\
    char* param##__pindex##_end;\
    int param##__pindex = std::strtol(param##__pindex##_str,&param##__pindex##_str,10);\
    if (param##__pindex##_str==param##__pindex##_str){\
        log_e("syntax error: invalid param");\
    }

namespace cmd_handler {
    // params: param_count, params

    DEF_CMD_HANDLER(exit){
        log_i("exit cli");
        esp_restart();
    }
    DEF_CMD_HANDLER(help){
        Serial.println(
            "/exit\r\n"
            "/help\r\n"
            "/getvbat\r\n"
#if FW_SERVER
            "/setdate <year> <month> <day>\r\n"
            "/settime <hour> <minute> <second>\r\n"
            "/getdatetime\r\n"
            "/lssleep\r\n"
            "/addsleep <begin_sec> <end_sec>\r\n"
            "/delsleep <index>\r\n"
            "/savesleep\r\n"
#else
#endif
        );
    }
    DEF_CMD_HANDLER(getvbat){
        CMD_CHK_LEN(0);
        float vbat = read_battery_voltage();
        Serial.printf("battery voltage: %.2f\r\n", vbat);
    }
#if FW_SERVER
    char __datetime_buffer[32] = {0};

    DEF_CMD_HANDLER(setdate){
        CMD_CHK_LEN(3);
        if (command_paramlist_length[0]&&command_paramlist_length[1]&&command_paramlist_length[2]){
            CMD_PARSE_INT_PARAM(0);
            CMD_PARSE_INT_PARAM(1);
            CMD_PARSE_INT_PARAM(2);
            DateTime now = rtc.now();
            DateTime tm2 = DateTime(param0,param1,param2,now.hour(),now.minute(),now.second());
            if (tm2.isValid()){
                rtc.adjust(tm2);
            }
            else {
                log_e("invalid date");
            }
        }
        else {
            log_e("syntax error: param length error");
        }
    }
    DEF_CMD_HANDLER(settime){
        CMD_CHK_LEN(3);
        if (command_paramlist_length[0]&&command_paramlist_length[1]&&command_paramlist_buffer[2]){
            CMD_PARSE_INT_PARAM(0);
            CMD_PARSE_INT_PARAM(1);
            CMD_PARSE_INT_PARAM(2);
            DateTime now = rtc.now();
            DateTime tm2 = DateTime(now.year(),now.month(),now.day(),param0,param1,param2);
            if (tm2.isValid()){
                rtc.adjust(tm2);
            }
            else {
                log_e("invalid time");
            }
        }
        else {
            log_e("syntax error: param length error");
        }
    }
    DEF_CMD_HANDLER(getdatetime){
        CMD_CHK_LEN(0);
        DateTime now = rtc.now();
        memcpy(__datetime_buffer, datetime_format, sizeof(datetime_format));
        Serial.println(now.toString(__datetime_buffer));
    }
    DEF_CMD_HANDLER(lssleep){
        CMD_CHK_LEN(0);
        const DaySeconds now = get_dayseconds();
        Serial.printf("now: %d",int(now));
        for(uint8_t index=0;index<sleep_interval_array_size;index++){
            const auto& si = sleep_interval_array[index];
            if (si.is_available()){
                uint8_t start_hour = si.start_sec/3600;
                uint8_t start_minute = (si.start_sec%3600)/60;
                uint8_t start_second = si.start_sec%60;
                uint8_t end_hour = si.end_sec/3600;
                uint8_t end_minute = (si.end_sec%3600)/60;
                uint8_t end_second = si.end_sec%60;
                constexpr char* fmt1 = "[%hhu](%d, %d) %hhu:%hhu:%hhu -> %hhu:%hhu:%hhu\r\n";
                constexpr char* fmt2 = "[%hhu](%d, %d) %hhu:%hhu:%hhu -> %hhu:%hhu:%hhu [in_progress]\r\n";
                Serial.printf(si.in_progress(now)?fmt2:fmt1,
                    index,int(si.start_sec),int(si.end_sec),
                    start_hour,start_minute,start_second,
                    end_hour,end_minute,end_second    
                );
            }
            else {
                Serial.printf("[%u](%d, %d) unavailable\r\n", index, int(si.start_sec), int(si.end_sec));
            }
        }
    }
    DEF_CMD_HANDLER(addsleep){
        CMD_CHK_LEN(2);
        CMD_PARSE_INT_PARAM(0);
        CMD_PARSE_INT_PARAM(1);
        SleepInterval* free_slot = nullptr;
        for (SleepInterval& si:sleep_interval_array){
            if (!si.is_available()){
                free_slot = &si;
                break;
            }
        }
        if (!free_slot){
            log_e("sleep interval array is full");
            return;
        }
        if (0<=param0&&param0<=86400&&0<=param1&&param1<=86400&&param0!=param1){
            free_slot->start_sec = param0;
            free_slot->end_sec = param1;
            uint8_t start_hour = param0/3600;
            uint8_t start_minute = (param0%3600)/60;
            uint8_t start_second = param0%60;
            uint8_t end_hour = param1/3600;
            uint8_t end_minute = (param1%3600)/60;
            uint8_t end_second = param1%60;
            Serial.printf("(%d, %d) %hhu:%hhu:%hhu -> %hhu:%hhu:%hhu\r\n",
                param0,param1,
                start_hour,start_minute,start_second,
                end_hour,end_minute,end_second
            );
        }
        else {
            log_e("invalid sleep interval");
        }
    }
    DEF_CMD_HANDLER(delsleep){
        CMD_CHK_LEN(1);
        CMD_PARSE_INT_PARAM(0);
        if (0<=param0&&param0<sleep_interval_array_size){
            auto& si = sleep_interval_array[param0];
            si.start_sec = 0;
            si.end_sec = 0;
        }
        else {
            log_e("value out of range");
        }
    }
    DEF_CMD_HANDLER(savesleep){
        CMD_CHK_LEN(0);
        store_sleep_intervals();
    }
#else

#endif
}

#undef DEF_CMD_HANDLER
#undef CMD_CHK_LEN
#undef CMD_PARSE_INT_PARAM

void transmit_advertising(AdvertisingType type, DaySeconds now=get_dayseconds()){
    AdvertisingData data = {
        .type = type,
        .battery_voltage = read_battery_voltage(),
        .now = now
    };
    set_advertising_data(data);
    start_advertising();
    delay(advertising_timeout);
    stop_advertising();
}

#if FW_SERVER

/// @brief 服务端进入DeepSleep
/// @param dev_pw 外设电源状态
/// @param sleep_time 睡眠时间（秒）
void begin_deepsleep(bool dev_pw, const DaySeconds& sleep_time){
    deinit_ble();
    // 配置外设电源
    if (dev_pw){
        if (!digitalRead(FWPIN_EN_DEV)){
            log_i("set ld1040 power on");
            digitalWrite(FWPIN_EN_DEV,1);
            delay(ld1040_init_time);
        }
        digitalWrite(FWPIN_EN_DEV,1);
    }
    else {
        digitalWrite(FWPIN_EN_DEV,0);
    }
    log_i("start deep-sleep. devpw=%hhu alarm=%d",static_cast<uint8_t>(dev_pw),static_cast<int>(sleep_time));
    // 配置唤醒源并入睡
    if (dev_pw){
        esp_deep_sleep_enable_gpio_wakeup(
            BITMASK(FWPIN_FUNCT),
            ESP_GPIO_WAKEUP_GPIO_HIGH
        );
    }
    else {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    }
    // 刷新外设缓冲区
    Serial.flush();
    Serial.end();
    Wire.flush();
    Wire.end();
    // 入睡
    gpio_hold_en(static_cast<gpio_num_t>(FWPIN_SW_ALWAY_ENABLE));
    gpio_deep_sleep_hold_en();
    esp_sleep_enable_timer_wakeup(static_cast<int>(sleep_time)*1000000);
    esp_deep_sleep_start();
}

// 服务端不进入loop，在此函数完成相关操作后便进入睡眠
void server_setup(bool reset_cause_is_deepsleep){
    const DaySeconds now = get_dayseconds();
    log_i("begin sleep. time=%d", static_cast<int>(now));
    if (reset_cause_is_deepsleep){
        // 检查是闹钟唤醒还是雷达唤醒
        esp_sleep_wakeup_cause_t wkupcause = esp_sleep_get_wakeup_cause();
        if (wkupcause==ESP_SLEEP_WAKEUP_GPIO){
            // GPIO唤醒（雷达触发）
            log_i("wakeup by gpio");
            transmit_advertising(AdvertisingType::SERVER_ALARM,now);
        }
        else {
            // 未定义情况或RTC闹钟
            log_i("wakeup cause: %d", static_cast<int>(wkupcause));
        }
    }
    else {
        log_i("power on");
    }
    if (digitalRead(FWPIN_SW_ALWAY_ENABLE)){
        SleepInterval* si = find_in_progress_interval(now);
        if (si){ // 找到了正在进行中的睡眠区间
            log_i("found in progress sleep interval");
            const DaySeconds sleep_time = si->end_sec - now;
            begin_deepsleep(false, sleep_time);
            return;
        }
        si = find_next_sleep_interval(now);
        if (si){ // 找到了下一个可用的睡眠区间
            log_i("found next sleep interval");
            const DaySeconds sleep_time = si->start_sec - now;
            begin_deepsleep(true, sleep_time);
            return;
        }
        log_w("avaliable sleep interval not found");
    }
    else {
        log_i("alway enable");
    }
    // 未找到可用的睡眠区间或启用了alway enable
    const DaySeconds sleep_time = now>DaySeconds(30)?DaySeconds(30):DaySeconds(0);
    begin_deepsleep(true, sleep_time);
}

#else
#endif

void setup(){
    Serial.begin(115200);
    delay(10);
    log_i("FuckerDetectorX");
    log_i("VERSION: " STRINGIFY(FIRMWARE_VERSION));
    log_i("REBOOT COUNT: %u", chip_boot_counter++);
    pinMode(FWPIN_EN_VBAT, OUTPUT);
    pinMode(FWPIN_LED,OUTPUT);
    pinMode(FWPIN_BOOT,INPUT);
    pinMode(FWPIN_FUNCT,INPUT);
    led(0);
    // 选择性初始化
#if FW_SERVER
#pragma message("Server Firmware")
    // RTC gpio
    esp_reset_reason_t reset_reason = esp_reset_reason();
    if (reset_reason==ESP_RST_DEEPSLEEP){
    }
    else {
        pinMode(FWPIN_EN_DEV,OUTPUT);
    }
    pinMode(FWPIN_SW_ALWAY_ENABLE, INPUT);
    pinMode(FWPIN_BTN_BEGIN_CLI, INPUT);
#else
#pragma message("Client Firmware")
    pinMode(FWPIN_EN_DEV,OUTPUT);
#endif
    // 命令行注册
#define SET_CMD_HANDLER(__name) command_handler_map[#__name]=cmd_handler::__name;
    // set command handlers
    SET_CMD_HANDLER(exit);
    SET_CMD_HANDLER(help);
    SET_CMD_HANDLER(getvbat);
#if FW_SERVER
    SET_CMD_HANDLER(setdate);
    SET_CMD_HANDLER(settime);
    SET_CMD_HANDLER(getdatetime);
    SET_CMD_HANDLER(lssleep);
    SET_CMD_HANDLER(addsleep);
    SET_CMD_HANDLER(delsleep);
    SET_CMD_HANDLER(savesleep);
#else
#endif
#undef SET_CMD_HANDLER
    // 初始化外设
    Wire.begin(FWPIN_IIC_SDA, FWPIN_IIC_SCL);
#if FW_SERVER
    init_rtc();
    load_sleep_intervals();
#endif
    init_ble();
    log_i("setup completed");
    // 服务器特殊逻辑
#if FW_SERVER
    log_i("reset_reason: %d", static_cast<int>(reset_reason));
    log_i("wakeup_cause: %d", static_cast<int>(esp_sleep_get_wakeup_cause()));
    log_i("wakeup_cause bitmask: %d", static_cast<int>(esp_sleep_get_wakeup_causes()));
    if (reset_reason==ESP_RST_DEEPSLEEP){
        server_setup(true);
    }
    else if (!digitalRead(FWPIN_BTN_BEGIN_CLI)) {
        reset_command_receiver();
        for(;;){
            command_receiver();
        }
        // 只能通过输入命令 exit 退出
    }
    else {
        server_setup(false);
    }
#endif
}

#if FW_SERVER
void loop(){}
#else
void loop(){

}
#endif