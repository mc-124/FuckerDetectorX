#include "time.hpp"

static DaySeconds _dayseconds_offset = 0;

DaySeconds get_dayseconds(){
    return DaySeconds(seconds()%86400) + _dayseconds_offset;
}

void calibrate_dayseconds(const DaySeconds& now){
    _dayseconds_offset = now-DaySeconds(seconds()%86400);
}

#if FW_SERVER

RTC_MODULE rtc;

RTC_DATA_ATTR static bool sleep_interval_array_is_loaded = false;
RTC_DATA_ATTR SleepInterval sleep_interval_array[sleep_interval_array_size];

void load_sleep_intervals(){
    if (sleep_interval_array_is_loaded) return;
    log_i("Loading sleep interval array");
    eeprom.readBuffer(sleep_interval_eeprom_address, 
        reinterpret_cast<uint8_t*>(sleep_interval_array),
        sleep_interval_array_size*8
    );
    for (uint8_t index=0;index<sleep_interval_array_size;index++){
        SleepInterval& si = sleep_interval_array[index];
        if (!(0<=int(si.start_sec)&&int(si.start_sec)<=86400)||!(0<=int(si.end_sec)&&int(si.end_sec)<=86400)){
            log_w("[%hhu] invalid sleep interval", index);
            si.start_sec = 0;
            si.end_sec = 0;
        }
    }
    sleep_interval_array_is_loaded = true;
}

void store_sleep_intervals(){
    log_i("Storing sleep interval array");
    eeprom.writeBuffer(sleep_interval_eeprom_address,
        reinterpret_cast<uint8_t*>(sleep_interval_array),
        sleep_interval_array_size*8
    );
}

SleepInterval* find_in_progress_interval(const DaySeconds& now){
    for (uint8_t i=0;i<sleep_interval_array_size;i++){
        SleepInterval* si = sleep_interval_array+i;
        if (si->is_available()&&si->in_progress(now)){
            return si;
        }
    }
    return nullptr;
}

SleepInterval* find_next_sleep_interval(const DaySeconds& now){
    SleepInterval* min_si = nullptr;
    DaySeconds min_diff = 86400;
    for (uint8_t i=0;i<sleep_interval_array_size;i++){
        SleepInterval* si = sleep_interval_array+i;
        // 解决无法跨天问题
        if (si->is_available()){
            DaySeconds diff = si->start_sec - now;
            if (diff<min_diff){
                min_si = si;
                min_diff = diff;
            }
        }
    }
    return min_si;
}

void init_rtc(){
    log_i("Init RTC");
    if (!rtc.begin(&Wire)){
        log_e("Init RTC failed");
        ESP_ERROR_CHECK(EXC_INIT_RTC_FAILED);
    }
#ifndef USE_DS1307
    if (rtc.lostPower()){
        log_w("RTC lost power");
    }
#endif
    // 校准 DaySeconds 偏移
    auto now = rtc.now();
    calibrate_dayseconds(now.secondstime()%86400);
}

#else
#endif