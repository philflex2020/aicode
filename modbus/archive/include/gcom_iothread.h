#ifndef GCOM_IOTHREAD_H
#define GCOM_IOTHREAD_H


#include <condition_variable>
#include <mutex>
#include <mutex>
#include <atomic>
#include <future>

//#include "rigtorp/SPSCQueue.h"
//#include "rigtorp/MPMCQueue.h"

//#include "semaphore.hpp" // Linux semaphore wrapper with helper functions (use PCQ_Semaphore)

#include "shared_utils.hpp"
#include "gcom_config.h"

#define NEW_FPS_ERROR_PRINT(fmt_str, ...)            fmt::print(stderr, FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define NEW_FPS_ERROR_PRINT_NO_COMPILE(fmt_str, ...) fmt::print(stderr, fmt_str, ##__VA_ARGS__) 
#define NEW_FPS_ERROR_PRINT_NO_ARGS(fmt_str)         fmt::print(stderr, FMT_COMPILE(fmt_str))
// Channel definition
template <typename T>
class ioChannel {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void send(T&& message) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            queue.emplace(std::move(message));
        }
        cv.notify_one();
    }

    bool receive(T& message) {
        std::unique_lock<std::mutex> lock(mtx);
        while (queue.empty()) {
            cv.wait(lock);
        }

        message = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool receive(T& message, double durationInSeconds) {
        std::unique_lock<std::mutex> lock(mtx);
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(durationInSeconds));
        if (cv.wait_for(lock, duration, [this] { return !queue.empty(); })) {
            message = std::move(queue.front());
            queue.pop();
            return true;
        }
        return false; // timed out without receiving a message
    }

    bool receive(T& message, const std::chrono::seconds& duration) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, duration, [this] { return !queue.empty(); })) {
            message = std::move(queue.front());
            queue.pop();
            return true;
        }
        return false; // timed out without receiving a message
    }
    bool peekpop(T& message) {
        std::unique_lock<std::mutex> lock(mtx);
        if (!queue.empty()) {
            message = std::move(queue.front());
            queue.pop();
            return true;
        }
        return false;
    }
};


struct IO_Work {
    // this is to allow the poll item to be collected
    double tNow;
    std::string work_name;
    int work_id;       // 7 of 9
    int work_group;   // how many in the group
    int device_id;


    int threadId;
    cfg::Register_Types reg_type;
    WorkTypes wtype;  //pub set etc
    int errno_code;
    int offset;
    int errors;
    int start_register;
    int num_registers;
    std::vector<int> disabled_registers;

    std::vector<std::shared_ptr<struct cfg::map_struct>> items;
    std::shared_ptr<struct cfg::reg_struct> reg_map;
    std::shared_ptr<struct cfg::comp_struct> comp_map;


    double tStart;
    double tIo;
    double tDone;
    double tReceive;
    double tPool;
    double connect_time;
    uint8_t buf8[256];
    uint16_t buf16[256];

    int size;
    int bit;
    double response;
    bool test_mode = false;
    double tRun;
    double cTime;
    bool test_it = false;
    // IO_Work(IO_Work&& other) noexcept  {};

    // IO_Work& operator=(IO_Work&& other) noexcept {
    //     return *this;
    // };

    ioChannel<std::shared_ptr<IO_Work>>* io_repChan;  // Thread picks up IO_work and processes it

    void clear_bufs() {
        memset(buf8, 0, sizeof(buf8));    // Set buf8 to 0
        memset(buf16, 0, sizeof(buf16));  // Set buf16 to 0 
    }
    void set_bufs(int num_bufs,uint16_t* buf16_in, uint8_t* buf8_in ) {
        if (num_bufs > 0) {
            if (buf8_in) {
                memcpy(buf8, buf8_in, num_bufs* sizeof(uint8_t)); 
            }
            else {
                memset(buf8, 0, sizeof(buf8));    // Set buf8 to 0
            }
            if (buf16_in) {
                memcpy(buf16, buf16_in, num_bufs* sizeof(uint16_t)); 
            }
            else {
                memset(buf16, 0, sizeof(buf16));    // Set buf8 to 0
            }
        }
    }
};



struct PubGroup {
    std::string key;
    std::vector<std::shared_ptr<IO_Work>> works;
    int work_group;  // size
    double tNow;
    bool done=false;
};


struct ThreadControl;

struct IO_Thread {
    alignas(64) modbus_t* ctx = nullptr;
    //xMain_Thread* main_work;
    int id;
    std::atomic<bool> keep_running;
    std::future<bool> thread_future; // IO Thread future
   
    std::condition_variable* cv;
    std::mutex* mutex;
    bool* ready;

    int port;
    int tid;
    int jobs;
    int fails;

    std::thread thread;
    ThreadControl *tc;
    std::string ip;
    int connection_timeout;
    double connect_time;
    double cTime; // TODO deprecatd

};

std::shared_ptr<IO_Work> make_work(cfg::Register_Types reg_type,  int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype );
bool pollWork (std::shared_ptr<IO_Work> io_work);
bool setWork (std::shared_ptr<IO_Work> io_work);


#endif
