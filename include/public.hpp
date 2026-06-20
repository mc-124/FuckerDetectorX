#pragma once
#include "config.hpp"
#include "common.hpp"
#include "time.hpp"
#include "serial_cmd.hpp"
#include "ble.hpp"

#pragma region Commands

#define DECL_CMD_HANDLER(__funcname) void __funcname (size_t param_count, const CommandHandlerFuncParams& params)

namespace cmd_handler {
    // params: param_count, params
    DECL_CMD_HANDLER(exit); // 重启芯片
    DECL_CMD_HANDLER(help); // 打印帮助信息
    DECL_CMD_HANDLER(version); // 打印固件版本和芯片信息
    DECL_CMD_HANDLER(vbat); // 打印电池电压
#if FW_SERVER
    DECL_CMD_HANDLER(setdate); // 设置日期
    DECL_CMD_HANDLER(settime); // 设置时间
    DECL_CMD_HANDLER(getdatetime); // 打印日期和时间
    DECL_CMD_HANDLER(lssleep); // 打印所有睡眠区间
    DECL_CMD_HANDLER(addsleep); // 添加睡眠区间
    DECL_CMD_HANDLER(delsleep); // 删除已有的睡眠区间
    DECL_CMD_HANDLER(savesleep); // 保存对睡眠区间的修改
#else

#endif
}

#undef DECL_CMD_HANDLER

#pragma endregion

void init_cli();
void init_pub_io();
void init_pub_devices();

void transmit_advertising(AdvertisingType type);
float read_battery_voltage_cached();