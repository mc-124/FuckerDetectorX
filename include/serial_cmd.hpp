#pragma once

#include "config.hpp"

#include <Arduino.h>
#include <array>
#include <map>
#include <string_view>

// 操作名缓冲区
extern char command_operator_buffer[cmd_operator_bufsize+1];
// 操作名长度
extern size_t command_operator_length;
// 参数缓冲区数组
extern char command_paramlist_buffer[cmd_paramlist_bufsize][cmd_param_bufsize+1];
// 参数数量
extern size_t command_param_count;
// 每个参数的长度
extern size_t command_paramlist_length[cmd_paramlist_bufsize];

using CommandHandlerFuncParams = std::array<std::string_view, cmd_paramlist_bufsize>;
using CommandHandlerFuncPtr = void(*)(size_t,const CommandHandlerFuncParams&);

// 命令处理函数表
extern std::map<std::string_view, CommandHandlerFuncPtr> command_handler_map;

// 重置状态机
void reset_command_receiver();
// 接收命令 需要在loop里循环调用
void command_receiver();