#pragma once

#include <condition_variable>
#include <mutex>
#include <atomic>

#include "rigtorp/SPSCQueue.h"
#include "rigtorp/MPMCQueue.h"

#include "semaphore.hpp" // Linux semaphore wrapper with helper functions (use PCQ_Semaphore)

#include "shared_utils.hpp"

#define NEW_FPS_ERROR_PRINT(fmt_str, ...)            fmt::print(stderr, FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define NEW_FPS_ERROR_PRINT_NO_COMPILE(fmt_str, ...) fmt::print(stderr, fmt_str, ##__VA_ARGS__) 
#define NEW_FPS_ERROR_PRINT_NO_ARGS(fmt_str)         fmt::print(stderr, FMT_COMPILE(fmt_str))

class IO_Work {
    public:
    alignas(64) modbus_t* ctx = nullptr;
    Register_Types reg_type;
    int offset;
    int size;
    int device_id;
    int errno_code;
    int num_registers;
    bool bit;
    int errors;
    double response;
    u16 u16_buff[MODBUS_MAX_READ_REGISTERS];  // receive into this (holding and input)
    u8  u8_buff[MODBUS_MAX_READ_REGISTERS];   // receive into this (coil and discrete input)
    
    IO_Work() noexcept {
        memset(u16_buff, 0, sizeof (u16_buff));
        memset(u8_buff, 0, sizeof (u8_buff));
    };

    IO_Work(IO_Work&& other) noexcept  {};

    IO_Work& operator=(IO_Work&& other) noexcept {
        return *this;
    };
};



// For IO threads to consume off of (listener and main produce on these):
struct IO_Work_Q {
    PCQ_Semaphore signaller;                          // signals that work exists (shared between both q's)
    // rigtorp::MPMCQueue<Set_Work>  set_q{1024};        // highest priority (must be empty before going to poll_q)
    // rigtorp::MPMCQueue<Poll_Work> poll_q{256 * 256};  // lowest priority (set_q must be empty before popping off of this -> enqueued in batches one whole component at a time)
    rigtorp::MPMCQueue<IO_Work*>  set_q{1024};        // highest priority (must be empty before going to poll_q)
    rigtorp::MPMCQueue<IO_Work*>  get_q{1024};        // TODO get_q may be the same as a poll queue
    rigtorp::MPMCQueue<IO_Work*>  poll_q{1024};        // highest priority (must be empty before going to poll_q)
};
// For main to consume off of (listener and IO threads produce on these):
struct Main_Work_Q {
    PCQ_Semaphore signaller;                        // signals that work exists (shared between both q's)
    // rigtorp::SPSCQueue<Set_Work> set_q{1024};       // highest priority (must be empty before going to any other q -> NOTE(WALKER): main does "debounce" logic before requing work in another set_vals array)
    // rigtorp::MPMCQueue<Pub_Work> pub_q{256 * 256};  // IO threads produce on this q for main to publish when a batch of polls is done (on a per component basis)
    // rigtorp::SPSCQueue<Get_Work> get_q{1024};       // listener produces on this q for main to respond to get requests with json 
    rigtorp::MPMCQueue<IO_Work*>  pub_q{1024};        // highest priority (must be empty before going to poll_q)
    rigtorp::MPMCQueue<IO_Work*>  save_q{1024};       // highest priority (must be empty before going to poll_q)
};

struct Main_Thread {
    // main stuff:
    Main_Work_Q main_work_qs;
    bool client_keep_running;
    IO_Work_Q io_work_qs;
    PCQ_Semaphore signaller;                        // TODO check signals that work exists (shared between both q's)
    std::condition_variable cv;
    std::mutex mutex;
    bool ready;

};

struct IO_Thread {
    alignas(64) modbus_t* ctx = nullptr;
    Main_Thread* main_work;
    int id;
    std::atomic<bool> keep_running;
    std::future<bool> thread_future; // IO Thread future

    std::condition_variable* cv;
    std::mutex* mutex;
    bool* ready;
};


class IO_Thread_Pool
{
    public:
    IO_Thread_Pool(u8 max_num_conns) : max_num_conns(max_num_conns) {
        for (u8 i = 0; i < max_num_conns; ++i) {
            threads.push_back(new IO_Thread);
        }    
    }

    u8 max_num_conns = 1; // This is either "max_num_conns" in the config or "total_num_threads" after reading the config
    u8 num_conns = 0;       // actual num_threads variable underneath this new system now
    std::vector<IO_Thread*> threads;
    
    //IO_Thread_Workspace* io_thread_workspaces;
    //IO_Work_Q io_work_qs;

    ~IO_Thread_Pool() {
        for (u8 i = 0; i < max_num_conns; ++i) {
            delete (threads[i]);
        }    
    }

        // for (u8 i = 0; i < max_num_conns; ++i) {
        //     io_thread_workspaces[i].~IO_Thread_Workspace();
        // }

    void stop_threads() {
        // tell each thread to stop:
        for (u8 i = 0; i < max_num_conns; ++i) {
            threads[i]->keep_running = false;
        }
        // // signal each thread after setting the keep_running variable to false:
        // for (u8 i = 0; i < max_num_conns; ++i) {
        //     io_work_qs.signaller.signal();
        // }
        // wait for each thread to stop:
        for (u8 i = 0; i < num_conns; ++i) {
            threads[i]->thread_future.wait();
        }
    }

    // // helper functions:
    // static std::size_t get_bytes_needed(const config_loader::Client_Config& config) {
    //     std::size_t needed = 0;

    //     needed += sizeof(IO_Thread_Workspace) * config.connection.max_num_conns + alignof(IO_Thread_Workspace);

    //     return needed;
    // }
};