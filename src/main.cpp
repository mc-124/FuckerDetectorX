#include <Arduino.h>

#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "error.hpp"
#include "common.hpp"
#include "config.hpp"
#include "time.hpp"
#include "ble.hpp"
#include "serial_cmd.hpp"

#include <utility>

RTC_DATA_ATTR uint32_t chip_boot_counter = 0;

#define DEF_CMD_HANDLER(__funcname) void __funcname (size_t param_count, const CommandHandlerFuncParams& params)

namespace cmd_handler {
    DEF_CMD_HANDLER(exit){
        log_i("exit cli");
        esp_restart();
    }
#if FW_SERVER
    // 设置RTC模块的时间
    DEF_CMD_HANDLER(settime){

    }
#else

#endif
}

#undef DEF_CMD_HANDLER

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

/// @brief 进入DeepSleep
/// @param dev_pw 外设电源状态
/// @param sleep_time 睡眠时间（秒）
void begin_deepsleep(bool dev_pw, const DaySeconds& sleep_time){
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
    gpio_hold_en(static_cast<gpio_num_t>(FWPIN_EN_DEV));
    log_w("start deep-sleep. devpw=%hhu alarm=%d",static_cast<uint8_t>(dev_pw),static_cast<int>(sleep_time));
    // 刷新外设缓冲区
    Serial.flush();
    Wire.flush();
    // 配置唤醒源并入睡
    if (dev_pw){
        gpio_deep_sleep_wakeup_enable(static_cast<gpio_num_t>(FWPIN_FUNCT),GPIO_INTR_POSEDGE); // 上升沿唤醒
        esp_sleep_enable_gpio_wakeup();
    }
    else {
        gpio_deep_sleep_wakeup_disable(static_cast<gpio_num_t>(FWPIN_FUNCT));
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    }
    esp_sleep_enable_timer_wakeup(static_cast<int>(sleep_time)*1000000);
    esp_deep_sleep_start();
}

// 服务端不进入loop，在此函数完成相关操作后便进入睡眠
void server_setup(bool reset_cause_is_deepsleep){
    const DaySeconds now = get_dayseconds();
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
    SleepInterval* si = find_in_progress_interval(now);
    if (si){ // 找到了正在进行中的睡眠区间
        log_i("found in progress sleep interval");
        const DaySeconds sleep_time = si->end_sec - now;
        begin_deepsleep(false, sleep_time);
    }
    si = find_next_sleep_interval(now);
    if (si){ // 找到了下一个可用的睡眠区间
        log_i("found next sleep interval");
        const DaySeconds sleep_time = si->start_sec - now;
        begin_deepsleep(true, sleep_time);
    }
    // 未找到可用的睡眠区间
    log_w("avaliable sleep interval not found");
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
    led(0);
    pinMode(FWPIN_EN_VBAT, OUTPUT);
    pinMode(FWPIN_LED,OUTPUT);
    pinMode(FWPIN_BOOT,INPUT);
    pinMode(FWPIN_FUNCT,INPUT);
#if FW_SERVER
#pragma message("Server Firmware")
    esp_reset_reason_t reset_reason = esp_reset_reason();
    if (reset_reason==ESP_RST_DEEPSLEEP){
    }
    else {
        pinMode(FWPIN_EN_DEV,OUTPUT); // rtc gpio
    }
#else
#pragma message("Client Firmware")
    pinMode(FWPIN_EN_DEV,OUTPUT);
#endif
    // 命令行注册
#define SET_CMD_HANDLER(__name) command_handler_map[#__name]=cmd_handler::__name;
    // set command handlers
    SET_CMD_HANDLER(exit);
#if FW_SERVER
    SET_CMD_HANDLER(settime);
#else
#endif
#undef SET_CMD_HANDLER
    // 初始化外设
#if FW_SERVER
    init_rtc();
#endif
    Wire.begin(FWPIN_IIC_SDA, FWPIN_IIC_SCL);
    init_ble();
    load_sleep_intervals();
    log_i("setup completed");
#if FW_SERVER
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

void loop()
#if FW_SERVER
{}
#else
{

}
#endif