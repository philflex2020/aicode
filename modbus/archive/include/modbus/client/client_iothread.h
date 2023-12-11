#pragma once

#include "shared_utils.hpp"


struct IO_Info {
    alignas(64) modbus_t* ctx = nullptr;
    Register_Types reg_type;
    // all to do with the polling operaton
    static std::chrono::steady_clock::time_point baseTime;
    std::string poll_timer; 
    std::string reply_to; 
    int num_work;
    int work_num;

    int offset;
    int size;
    int device_id;
    int errno_code;
    int num_registers;
    bool bit;
    int errors;
    u16 u16_buff[MODBUS_MAX_READ_REGISTERS];  // receive into this (holding and input)
    u8  u8_buff[MODBUS_MAX_READ_REGISTERS];   // receive into this (coil and discrete input)
    
};
struct IO_Work {

    //std::vector<Set_Info> set_vals;
    IO_Info * io_vals;

    int errno_code = 0; // this set to anything other than zero means there is an error
    IO_Work() = default;

    IO_Work(IO_Work&& other) noexcept 
        : io_vals(other.io_vals) {};

    IO_Work& operator=(IO_Work&& other) noexcept {
        io_vals = other.io_vals;
        return *this;
    }
};
