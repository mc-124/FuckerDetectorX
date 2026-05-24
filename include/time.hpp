#pragma once

#include "error.hpp"
#include "config.hpp"
#include "common.hpp"
#include <esp_timer.h>

#include <Arduino.h>
#include <Wire.h>

#if FW_SERVER
#include <RTClib.h>
#endif

#ifdef USE_DS1307
#define RTC_MODULE RTC_DS1307
#else
#define RTC_MODULE RTC_DS3231
#endif

class DaySeconds {
    private:
    int __dayseconds;
    public:
    inline DaySeconds(void):__dayseconds(0){}
    inline DaySeconds(int sec):__dayseconds(sec){}  // 注意要先 %86400
    inline DaySeconds(const DaySeconds& ds):__dayseconds(ds.__dayseconds){}
    inline operator int(){return this->__dayseconds;}
    inline DaySeconds operator+(const DaySeconds& ds)const{
        int r=this->__dayseconds+ds.__dayseconds;
        return r%86400;
    }
    inline DaySeconds operator-(const DaySeconds& ds)const{
        int r=this->__dayseconds-ds.__dayseconds;
        return r%86400;
    }
    inline operator int()const{
        return this->__dayseconds;
    }
#define __DEF_CMP(__symbol) \
    inline bool operator __symbol (const DaySeconds& ds)const \
        {return this->__dayseconds __symbol ds.__dayseconds;}
    __DEF_CMP(>)__DEF_CMP(<)
    __DEF_CMP(>=)__DEF_CMP(<=)
    __DEF_CMP(==)__DEF_CMP(!=)
#undef __DEF_CMP
};
static_assert(sizeof(DaySeconds)==4,"dayseconds struct error");

inline uint32_t seconds(){return esp_timer_get_time()/1000000;}
DaySeconds get_dayseconds();
// 校准
void calibrate_dayseconds(const DaySeconds& now);

#if FW_SERVER

extern RTC_MODULE rtc;

struct SleepInterval {
    DaySeconds start_sec;
    DaySeconds end_sec;
    inline bool is_available()const{
        return this->start_sec!=this->end_sec;
    }
    // 进行中
    inline bool in_progress(const DaySeconds& now)const{
        return this->start_sec<=now&&now<this->end_sec;
    }
};
static_assert(sizeof(SleepInterval)==8,"struct error");

extern RTC_DATA_ATTR SleepInterval sleep_interval_array[sleep_interval_array_size];

void load_sleep_intervals();
void store_sleep_intervals();
SleepInterval* find_in_progress_interval(const DaySeconds& now);
SleepInterval* find_next_sleep_interval(const DaySeconds& now);
void init_rtc();

#else
#endif

