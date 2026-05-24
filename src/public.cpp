#include "public.hpp"

#pragma region Command

#define IMPL_CMD_HANDLER(__funcname) \
    void cmd_handler:: __funcname (size_t param_count, const CommandHandlerFuncParams& params)
;
#define CMD_CHK_LEN(__len) if(param_count!=__len){log_e("syntax error: param count error");return;}
#define CMD_PARSE_INT_PARAM(__pindex) \
    char* param##__pindex##_str = command_paramlist_buffer[__pindex];\
    char* param##__pindex##_end;\
    int param##__pindex = std::strtol(param##__pindex##_str,&param##__pindex##_end,10);\
    if (param##__pindex##_str==param##__pindex##_end){\
        log_e("syntax error: invalid param");\
    }
;

IMPL_CMD_HANDLER(exit){
    Serial.println("exit cli");
    esp_restart();
}

IMPL_CMD_HANDLER(help){
    CMD_CHK_LEN(0);
    Serial.println(
        "=== HELP ===\r\n"
        "/exit: exit CLI and restart system\r\n"
        "/help: print this message\r\n"
        "/info: print chip information\r\n"
#if FW_SERVER
        "/setdate <year> <month> <day>: set RTC date\r\n"
        "/settime <hour> <minute> <second>: set RTC time\r\n"
        "/getdatetime: print time\r\n"
        "/lssleep: print all sleep interval\r\n"
        "/addsleep <begin_sec> <end_sec>: append new sleep interval\r\n"
        "/delsleep <index>: remove a exists sleep interval\r\n"
        "/save: save modify\r\n"
#else
#endif
    );
}

IMPL_CMD_HANDLER(info){
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

IMPL_CMD_HANDLER(setdate){
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

IMPL_CMD_HANDLER(settime){
    CMD_CHK_LEN(3);
    if (command_paramlist_length[0]&&command_paramlist_length[1]&&command_paramlist_length[2]){
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

IMPL_CMD_HANDLER(getdatetime){
    CMD_CHK_LEN(0);
    DateTime now = rtc.now();
    memcpy(__datetime_buffer, datetime_format, sizeof(datetime_format));
    Serial.println(now.toString(__datetime_buffer));
}

IMPL_CMD_HANDLER(lssleep){
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

IMPL_CMD_HANDLER(addsleep){
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
    if (0<=param0&&param0<86400&&0<=param1&&param1<86400&&param0!=param1){
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

IMPL_CMD_HANDLER(delsleep){
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

IMPL_CMD_HANDLER(save){
    CMD_CHK_LEN(0);
    store_sleep_intervals();
}

#else

#endif
#undef CMD_CHK_LEN
#undef CMD_PARSE_INT_PARAM
#undef IMPL_CMD_HANDLER
#pragma endregion

#pragma region Init

void init_cli(){
    if (!digitalRead(FWPIN_BTN_BEGIN_CLI)){
#define SET_CMD_HANDLER(__name) command_handler_map[#__name]=cmd_handler::__name;
        log_i("begin cli");
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
        SET_CMD_HANDLER(save);
#else
#endif
#undef SET_CMD_HANDLER
        reset_command_receiver();
        for(;;){
            command_receiver();
        }
    }
}

void init_pub_io(){
    pinMode(FWPIN_EN_VBAT, OUTPUT);
    pinMode(FWPIN_LED, OUTPUT);
    pinMode(FWPIN_BOOT, INPUT);
    pinMode(FWPIN_BTN_BEGIN_CLI, INPUT);

    if (!Wire.begin(FWPIN_IIC_SDA, FWPIN_IIC_SCL)){
        log_e("init i2c failed");
        ESP_ERROR_CHECK(EXC_INIT_I2C_FAILED);
    }
}

void init_pub_devices(){
    init_ble();
}

#pragma endregion

#if !FW_SERVER
extern SemaphoreHandle_t lock_ble;
#endif

void transmit_advertising(AdvertisingType type){
    AdvertisingData data;
    init_advertising_data(data,type);

#if FW_SERVER
    data.now = get_dayseconds();
#else
    data.now = 0;

    p_blescan->stop();
    xSemaphoreTake(lock_ble, portMAX_DELAY);
#endif

    set_advertising_data(data);
    start_advertising();
    delay(advertising_timeout);
    stop_advertising();

#if !FW_SERVER
    xSemaphoreGive(lock_ble);
#endif
}

float read_battery_voltage_cached(){
    static uint32_t last_read_vbat_time = 0;
    static float last_vbat = 0.0f;
    uint32_t now = seconds();
    if (now-last_read_vbat_time>battery_voltage_cache_timeout){
        last_read_vbat_time = now;
        float vbat = read_battery_voltage();
        last_vbat = vbat;
        return vbat;
    }
    else {
        return last_vbat;
    }
}