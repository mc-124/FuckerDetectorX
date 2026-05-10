#include "serial_cmd.hpp"

#define KB_CTRL_C static_cast<char>(0x03)

char command_operator_buffer[cmd_operator_bufsize+1];
size_t command_operator_length;

char command_paramlist_buffer[cmd_paramlist_bufsize][cmd_param_bufsize+1];
size_t command_param_count;
size_t command_paramlist_length[cmd_paramlist_bufsize];

std::map<std::string_view, CommandHandlerFuncPtr> command_handler_map;

enum class CmdRcvFlags:uint8_t {
    NO_COMMAND,
    OPERATOR,
    PARAMS,
};

static CmdRcvFlags cmdrcvflags = CmdRcvFlags::NO_COMMAND;

void reset_command_receiver(){
    cmdrcvflags = CmdRcvFlags::NO_COMMAND;
    command_operator_length = 0;
    command_param_count = 0;
    for (size_t i=0;i<cmd_paramlist_bufsize;i++){
        command_paramlist_length[i] = 0;
    }
    memset(command_operator_buffer,0,sizeof(command_operator_buffer));
    memset(command_paramlist_buffer,0,sizeof(command_paramlist_buffer));
    memset(command_paramlist_length,0,sizeof(command_paramlist_length));
}

static bool is_valid_char(char chr){
    return ('0'<=chr&&chr<='9')||('A'<=chr&&chr<='Z')||('a'<=chr&&chr<='z')||(chr=='_'||chr=='.'||chr=='-');
}

static void command_handler(){
    std::string_view command_operator(command_operator_buffer,command_operator_length);
    if (command_handler_map.contains(command_operator)){
        CommandHandlerFuncPtr func = command_handler_map[command_operator];
        CommandHandlerFuncParams param_array;
        for (size_t i=0;i<cmd_paramlist_bufsize;i++){
            char* param_str = command_paramlist_buffer[i];
            std::string_view param(param_str, command_paramlist_length[i]);
            param_array[i] = param; // copy
        }
        func(command_param_count,param_array);
    }
    else {
        Serial.println("Error: invalid command");
    }
}

inline static void __backspace(){
    Serial.print("\b \b");
}

void command_receiver(){
    int byte = Serial.read();
    if (byte==-1){
        return;
    }
    else if (byte==KB_CTRL_C||byte==27){
        if (cmdrcvflags!=CmdRcvFlags::NO_COMMAND){
            reset_command_receiver();
            Serial.println("\r\nRESET");
            return;
        }
    }
    else if (byte=='\r'){
        return;
    }
    // /// @test serial
    // Serial.printf("Serial.read -> %d\n", byte);
    /*
        27: Esc
        127: Backspace
    */
    /// @note 有些串口工具似乎不会发送Enter，所以添加Tab等价于Enter
    switch (cmdrcvflags){
        case CmdRcvFlags::NO_COMMAND:
            if (byte=='/'){
                cmdrcvflags = CmdRcvFlags::OPERATOR;
                Serial.print('/');
            }
            break;
        case CmdRcvFlags::OPERATOR:
            if (is_valid_char(byte)){
                if (command_operator_length<cmd_operator_bufsize){
                    Serial.print(static_cast<char>(byte));
                    command_operator_buffer[
                        command_operator_length++
                    ] = byte;
                }
            }
            else if (byte=='\b'||byte==0x7f){ // backspace
                if (command_operator_length){
                    //Serial.print('\b');
                    __backspace();
                    command_operator_buffer[
                        --command_operator_length
                    ] = 0;
                }
                else {
                    //Serial.print('\b');
                    __backspace();
                    reset_command_receiver();
                }
            }
            else if (byte==' '){ // enter params
                if (command_operator_length){
                    cmdrcvflags = CmdRcvFlags::PARAMS;
                    command_param_count++;
                    Serial.print(' ');
                }
            }
            else if (byte=='\n'||byte=='\t'){ // end
                if (command_operator_length){
                    Serial.println();
                    command_handler();
                    reset_command_receiver();
                }
            }
            break;
        case CmdRcvFlags::PARAMS:
            if (is_valid_char(byte)){
                size_t& param_len = command_paramlist_length[command_param_count-1];
                if (param_len<cmd_param_bufsize){
                    command_paramlist_buffer[command_param_count-1][param_len++] = byte;
                    Serial.print(static_cast<int>(byte));
                }
            }
            else if (byte=='\b'||byte==0x7f){
                size_t& param_len = command_paramlist_length[command_param_count-1];
                if (param_len){
                    command_paramlist_buffer[command_param_count-1][--param_len] = 0;
                    //Serial.print('\b');
                    __backspace();
                }
                else {
                    command_param_count--;
                    //Serial.print('\b');
                    __backspace();
                    if (!command_param_count){
                        cmdrcvflags = CmdRcvFlags::OPERATOR;
                    }
                }
            }
            else if (byte==' '){
                if (command_param_count<cmd_paramlist_bufsize){
                    command_param_count++;
                    Serial.print(' ');
                }
            }
            else if (byte=='\n'||byte=='\t'){
                Serial.println();
                command_handler();
                reset_command_receiver();
            }
            break;
    }
}