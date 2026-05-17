#define __CONFIGHPP_PRINT_INFO

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

static_assert(ARDUHAL_LOG_LEVEL!=ARDUHAL_LOG_LEVEL_NONE,"log level error");

RTC_DATA_ATTR uint32_t chip_boot_counter = 0;

#pragma region Commands

#define DEF_CMD_HANDLER(__funcname) static void __funcname (size_t param_count, const CommandHandlerFuncParams& params)
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
        CMD_CHK_LEN(0);
        Serial.println(
            "/exit\r\n"
            "/help\r\n"
            "/info\r\n"
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
    DEF_CMD_HANDLER(info){
        CMD_CHK_LEN(0);
        if constexpr(FW_SERVER){
            Serial.println("type: server");
        }
        else {
            Serial.println("type: client");
        }
        Serial.println("version: " STRINGIFY(FIRMWARE_VERSION));
        Serial.println("log level: " FW_LOG_LEVEL);
        Serial.println("compile time: " __TIMESTAMP__);
        float vbat = read_battery_voltage();
        Serial.printf("battery voltage: %.2f\r\n", vbat);
        Serial.println("arduino version: " STRINGIFY(ARDUINO));
        Serial.println("sdk version: " 
            STRINGIFY(ESP_IDF_VERSION_MAJOR) "."
            STRINGIFY(ESP_IDF_VERSION_MINOR) "."
            STRINGIFY(ESP_IDF_VERSION_PATCH)
        );
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

#pragma endregion

#if !FW_SERVER
SemaphoreHandle_t ble_lock;
SemaphoreHandle_t devlst_lock;
SemaphoreHandle_t devcli_lock;
TaskHandle_t task_alarm;
TaskHandle_t task_scanner;
TaskHandle_t task_ui;

struct ServerInfo {
    uint8_t mac_end[2];
    AdvertisingType type;
    char rssi;
    float battery_voltage;
    bool empty;
    DaySeconds time;

    /// @brief 初始化结构体
    inline void clear(){
        this->mac_end[0] = 0;
        this->mac_end[1] = 0;
        this->type = AdvertisingType::SERVER_ALARM;
        this->rssi = 0;
        this->battery_voltage = 0.0;
        this->empty = true;
        this->time = 0;
    }
};

ServerInfo found_ble_devices[found_ble_device_array_length]; // devlst_lock
ServerInfo found_ble_client_device; // devcli_lock
bool round_founded = false; // 扫描冷却

#endif

/// @brief 发送广告
/// @param type 广告类型
/// @param now 时间
void transmit_advertising(AdvertisingType type, DaySeconds now=get_dayseconds()){
    AdvertisingData data;
    init_advertising_data(data, type);

#if !FW_SERVER
    p_blescan->stop();
    xSemaphoreTake(ble_lock, portMAX_DELAY);
    log_i("start advertising");
#endif

    set_advertising_data(data);
    start_advertising();
    delay(advertising_timeout);
    stop_advertising();

#if !FW_SERVER
    log_i("stop advertising");
    xSemaphoreGive(ble_lock);
#endif
}

#pragma region SpecialFuncs
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

/// @brief 响铃任务
/// @param p 无参数
void tfunc_alarm(void* p){
    log_i("alarm task created");
    for (;;){
        xTaskNotifyWait(0,0,NULL,portMAX_DELAY);
        log_i("alarming");
        for (uint8_t i=0;i<3;i++){
            digitalWrite(FWPIN_EN_DEV,1);
            led(1);
            delay(600);
            digitalWrite(FWPIN_EN_DEV,0);
            led(0);
            delay(200);
        }
        delay(600); // 30000 - 3*(200+600)
        round_founded = false;
    }
}

/// @brief 扫描任务
/// @param p 无参数
void tfunc_scanner(void* p){
    log_i("scanner task created");
    for (;;){
        xSemaphoreTake(ble_lock, portMAX_DELAY);
        log_i("scanner running");
        p_blescan->start(0); // scan forever
        xSemaphoreGive(ble_lock);
        log_i("scanner stopped");
        delay(2); // 两个时间片，确保锁被抢走
    }
}

/// @brief UI绘图任务
/// @param p 无参数
void tfunc_ui(void* p){
    log_i("ui task created");
    for (;;){
        xTaskNotifyWait(0,0,nullptr,60*1000);
        // update ui
        float vbat = read_battery_voltage();
        /*
            |FDX-26060100   4.14V|
            |007F [12:05]   3.84V|
            |1120 [10:01]   3.51V|
            |                    |
            |                    |
            |                    |
            |                    |
            |1145         -127dBm|
         */
        oled.setCursor(0,0);
        oled.printf("FDX-" STRINGIFY(FIRMWARE_VERSION)"   %.2fV\r\n", vbat);
        xSemaphoreTake(devlst_lock,portMAX_DELAY);
        for (auto& di:found_ble_devices){
            if (di.empty){
                oled.println();
            }
            else {
                char buf[12];
                char* ptr = buf;
                int_to_string_buf(di.mac_end[0], &ptr); // 2
                int_to_string_buf(di.mac_end[1], &ptr); // 4
                *(ptr++) = ' '; // 5
                *(ptr++) = '['; // 6
                uint8_t hour = int(di.time)/3600;
                uint8_t minute = (int(di.time)%3600)/60;
                int_to_string_buf(hour, &ptr); // 8
                int_to_string_buf(minute, &ptr); // 10
                *ptr = 0; // 11
                oled.printf("%s]   %.2fV\r\n", buf, vbat);
            }
        }
        xSemaphoreGive(devlst_lock);
        xSemaphoreTake(devcli_lock,portMAX_DELAY);
        if (!found_ble_client_device.empty){
            char buf[10];
            char* ptr = buf;
            int_to_string_buf(found_ble_client_device.mac_end[0],&ptr);
            int_to_string_buf(found_ble_client_device.mac_end[1],&ptr);
            *ptr = 0;
            oled.print(buf);
            oled.printf("         %hhddBm", found_ble_client_device.battery_voltage);
        }
        xSemaphoreGive(devcli_lock);
        oled.display();
    }
}

/// @brief 激活闹钟
inline void activate_alarm(){
    xTaskNotify(task_alarm, NULL, eNoAction);
}

/// @brief 触发更新UI
inline void update_ui(){
    xTaskNotify(task_ui,NULL,eNoAction);
}

/// @brief 扫描回调
/// @param dev 扫到的设备
void AdvertisingDevCallbacks::onResult(BLEAdvertisedDevice dev) {
    if (round_founded){ // 扫描冷却
        return;
    }
    else if (dev.haveName()&&dev.haveManufacturerData()&&dev.haveTXPower()){
        auto name = dev.getName();
        auto data = dev.getManufacturerData();
        if ((name==ble_client_name||name==ble_server_name)&&data.length()==sizeof(AdvertisingData)){
            log_i("found transmitter");
            AdvertisingData advdata;
            uint8_t* p_advdata = reinterpret_cast<uint8_t*>(&advdata);
            // copy
            for (uint8_t byte:data) *(p_advdata++) = byte;
            // check
            if (advdata.company_id!=adv_company_id){
                return;
            }
            auto addr = dev.getAddress();
            auto addr_arr = addr.getNative();
            char stringbuf[18];
            bleaddr_tostrbuf(addr, stringbuf);
            log_i("transmitter: mac=%s vbat=%.2f type=%hhu", stringbuf, advdata.battery_voltage, static_cast<uint8_t>(advdata.type));
            round_founded = true;
            activate_alarm();
            char rssi = 0;
            if (dev.haveRSSI()){
                auto l_rssi = dev.getRSSI();
                if (l_rssi<=-127){
                    rssi = l_rssi;
                }
            }
            if (advdata.type==AdvertisingType::CLIENT_ALARM){
                xSemaphoreTake(devcli_lock, portMAX_DELAY);
                found_ble_client_device.empty = false;
                found_ble_client_device.battery_voltage = decode_battery_voltage(advdata.battery_voltage);
                found_ble_client_device.mac_end[0] = addr_arr[5];
                found_ble_client_device.mac_end[1] = addr_arr[6];
                found_ble_client_device.rssi = rssi;
                found_ble_client_device.type = advdata.type;
                found_ble_client_device.time = 0<=int(advdata.now)&&int(advdata.now)<=86400?advdata.now:DaySeconds(0);
                xSemaphoreGive(devcli_lock);
            }
            else {
                xSemaphoreTake(devlst_lock, portMAX_DELAY);
                for (ServerInfo& devinf:found_ble_devices){
                    if (devinf.empty){
                        devinf.empty = false;
                        devinf.mac_end[0] = addr_arr[5];
                        devinf.mac_end[1] = addr_arr[6];
                        devinf.rssi = rssi;
                        devinf.type = advdata.type;
                        devinf.battery_voltage = decode_battery_voltage(advdata.battery_voltage);
                        devinf.time = 0<=int(advdata.now)&&int(advdata.now)<=86400?advdata.now:DaySeconds(0);
                        break;
                    }
                }
                xSemaphoreGive(devlst_lock);
            }
        }
    }
}

/// @brief FUNCT按钮双击
void funct_button_ondclick(){
    transmit_advertising(AdvertisingType::CLIENT_ALARM);
}

#endif
#pragma endregion

void setup(){
    setCpuFrequencyMhz(80);
    Serial.begin(115200);
    delay(5);
    log_i("FuckerDetectorX " FW_TYPE_STRING);
    log_i("VERSION: " STRINGIFY(FIRMWARE_VERSION));
    log_i("REBOOT COUNT: %u", chip_boot_counter++);
    pinMode(FWPIN_EN_VBAT, OUTPUT);
    pinMode(FWPIN_LED,OUTPUT);
    pinMode(FWPIN_BOOT,INPUT);
    pinMode(FWPIN_BTN_BEGIN_CLI, INPUT);
    led(0);
    esp_reset_reason_t reset_reason = esp_reset_reason();
    if (!(reset_reason==ESP_RST_DEEPSLEEP&&FW_SERVER)){
        pinMode(FWPIN_EN_DEV,OUTPUT);
    }
    Wire.begin(FWPIN_IIC_SDA, FWPIN_IIC_SCL);
#if FW_SERVER
    pinMode(FWPIN_FUNCT,INPUT);
    init_rtc();
    load_sleep_intervals();
    pinMode(FWPIN_SW_ALWAY_ENABLE, INPUT);
#else
    btn_funct.attachDoubleClick(funct_button_ondclick);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, ssd1306_i2c_address)){
        ESP_ERROR_CHECK(EXC_INIT_OLED_FAILED);
    }
#endif
    init_ble();
    log_i("setup completed");

#pragma region BeginCLI
    if (!digitalRead(FWPIN_BTN_BEGIN_CLI)) {
        log_i("beginning cli");
        // 命令行注册
#define SET_CMD_HANDLER(__name) command_handler_map[#__name]=cmd_handler::__name;
        // set command handlers
        SET_CMD_HANDLER(exit);
        SET_CMD_HANDLER(help);
        SET_CMD_HANDLER(info);
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
        reset_command_receiver();
        for(;;){
            command_receiver();
        }
        // 只能通过输入命令 exit 退出
    }
#pragma endregion

#pragma region SpecialLogic
    // 特殊逻辑
#if FW_SERVER
    log_i("reset_reason: %d", static_cast<int>(reset_reason));
    log_i("wakeup_cause: %d", static_cast<int>(esp_sleep_get_wakeup_cause()));
    log_i("wakeup_cause bitmask: %d", static_cast<int>(esp_sleep_get_wakeup_causes()));
    if (reset_reason==ESP_RST_DEEPSLEEP){
        server_setup(true);
    }
    else {
        server_setup(false);
    }
#else
    for(auto& di:found_ble_devices){
        di.clear();
    }
    found_ble_client_device.clear();
    ble_lock = xSemaphoreCreateMutex();
    devlst_lock = xSemaphoreCreateMutex();
    devcli_lock = xSemaphoreCreateMutex();
    auto status = xTaskCreate(tfunc_alarm, "alarm", alarm_task_stack_size, NULL, 1, &task_alarm);
    if (status!=pdPASS){
        log_e("create task failed: %d", status);
        ESP_ERROR_CHECK(EXC_CREATE_ALARM_TASK_FAILED);
    }
    status = xTaskCreate(tfunc_scanner, "scanner", scanner_task_stack_size, NULL, 1, &task_scanner);
    if (status!=pdPASS){
        log_e("create task failed");
        ESP_ERROR_CHECK(EXC_CREATE_SCANNER_TASK_FAILED);
    }
    status = xTaskCreate(tfunc_ui, "ui", ui_task_stack_size, nullptr, 1, &task_ui);
    if (status!=pdPASS){
        log_e("create task failed");
        ESP_ERROR_CHECK(EXC_CREATE_UI_TASK_FAILED);
    }
#endif
#pragma endregion
}

#if FW_SERVER
void loop(){}
#else
void loop(){
    btn_funct.tick();
}
#endif