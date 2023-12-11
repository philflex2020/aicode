#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>
#include <tuple>
#include <functional>
#include <algorithm>
#include <random>
#include <errno.h> //For errno - the error number
#include <cxxabi.h>

#include <modbus/modbus.h>
#include "logger/logger.h"
#include "gcom_config.h"
#include "gcom_iothread.h"
#include "gcom_modbus_decode.h"
#include "gcom_fims.h"
#include "gcom_timer.h"
#define BAD_DATA_ADDRESS 112345680

// #include "oldmodbus_iothread.h"

using namespace std::chrono_literals;

// TODO put function in threadControl

struct ThreadControl;
std::mutex io_output_mutex;

bool CloseModbusForThread(std::shared_ptr<IO_Thread> io_thread, bool debug);
void ioThreadFunc(ThreadControl &control, struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread);
void responseThreadFunc(ThreadControl &control, struct cfg &myCfg);
std::string regTypeToStr(cfg::Register_Types &register_type);

std::string workTypeToStr(WorkTypes &work_type)
{
    if (work_type == WorkTypes::Set)
        return "Set";
    else if (work_type == WorkTypes::Get)
        return "Get";
    return "Unknown";
}

// // Channel definition
// template <typename T>
// class ioChannel {
// private:
//     std::queue<T> queue;
//     std::mutex mtx;
//     std::condition_variable cv;

// public:
//     void send(T&& message) {
//         {
//             std::unique_lock<std::mutex> lock(mtx);
//             queue.emplace(std::move(message));
//         }
//         cv.notify_one();
//     }

//     bool receive(T& message) {
//         std::unique_lock<std::mutex> lock(mtx);
//         while (queue.empty()) {
//             cv.wait(lock);
//         }

//         message = std::move(queue.front());
//         queue.pop();
//         return true;
//     }

//     bool receive(T& message, double durationInSeconds) {
//         std::unique_lock<std::mutex> lock(mtx);
//         auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(durationInSeconds));
//         if (cv.wait_for(lock, duration, [this] { return !queue.empty(); })) {
//             message = std::move(queue.front());
//             queue.pop();
//             return true;
//         }
//         return false; // timed out without receiving a message
//     }

//     bool receive(T& message, const std::chrono::seconds& duration) {
//         std::unique_lock<std::mutex> lock(mtx);
//         if (cv.wait_for(lock, duration, [this] { return !queue.empty(); })) {
//             message = std::move(queue.front());
//             queue.pop();
//             return true;
//         }
//         return false; // timed out without receiving a message
//     }
// };

struct IO_Work;
// Global Channel Definitions
ioChannel<std::shared_ptr<IO_Work>> io_pollChan;     // Use Channel to send IO-Work to thread
ioChannel<std::shared_ptr<IO_Work>> io_setChan;      // Use Channel to send IO-Work to thread
ioChannel<std::shared_ptr<IO_Work>> io_responseChan; // Thread picks up IO_work and processes it
ioChannel<std::shared_ptr<IO_Work>> io_poolChan;     // Response channel returns io_work to the pool
ioChannel<int> io_threadChan;                        // Thread Control

struct ThreadControl
{
    ioChannel<std::shared_ptr<IO_Work>> *io_pollChan;     // Use Channel to send IO-Work to thread
    ioChannel<std::shared_ptr<IO_Work>> *io_setChan;      // Use Channel to send IO-Work to thread
    ioChannel<std::shared_ptr<IO_Work>> *io_responseChan; // Thread picks up IO_work and processes it
    ioChannel<std::shared_ptr<IO_Work>> *io_poolChan;     // Response channel returns io_work to the pool

    bool ioThreadRunning = true;
    bool responseThreadRunning = true;
    std::vector<std::shared_ptr<IO_Thread>> ioThreadPool;
    std::thread responseThread;

    int num_threads = 4;
    int num_responses = 0;
    double tResponse = 0.0;
    void startThreads()
    {
        // Initialize flags
        ioThreadRunning = true;
        responseThreadRunning = true;
    }

    void stopThreads()
    {
        ioThreadRunning = false;
        responseThreadRunning = false;
        for (auto &tp : ioThreadPool)
        {
            if (tp->thread.joinable())
            {
                tp->thread.join();
            }
        }
        // Send shutdown signal to threads
        // io_threadChan.send(0);
    }
};

ThreadControl threadControl;

void getConnectTimes(std::stringstream &ss, bool include_key)
{
    if (include_key)
    {
        ss << "\"thread_connection_info\": ";
    }
    ss << "[";
    bool first = true;
    for (auto &io_thread : threadControl.ioThreadPool)
    {
        if (io_thread->ctx != nullptr)
        {
            if (!first)
            {
                ss << ", ";
            }
            else
            {
                first = false;
            }
            ss << "{";
            ss << "\"id\":" << io_thread->id << ",";
            ss << "\"port\":" << io_thread->port << ",";
            ss << "\"time_to_connect\":\"" << io_thread->connect_time << " s\",";
            ss << "\"num_jobs\":" << io_thread->jobs << ",";
            ss << "\"num_fails\":" << io_thread->fails << "";
            ss << "}";
        }
    }
    ss << "]";
}

double get_time_double();
void randomDelay(int minMicroseconds, int maxMicroseconds);

IO_Work IO_workItem(int device_id, cfg::Register_Types register_type, int offset, int num_registers)
{
    IO_Work work;
    work.device_id = device_id;
    work.register_type = register_type;
    work.offset = offset;
    work.num_registers = num_registers;
    return work;
}

// code related to compacting IO_Work vectors
// Comparator function to sort IO_Work objects based on their offset
bool compare_by_offset(const IO_Work &a, const IO_Work &b)
{
    return a.offset < b.offset;
}

// std::vector<std::unique_ptr<IO_Work>>
bool compact_io_works(std::vector<std::unique_ptr<IO_Work>> &compacted, std::vector<std::unique_ptr<IO_Work>> &io_works)
{
    // std::vector<std::unique_ptr<IO_Work>> compacted;

    // 1. Group IO_Work by device_id and register_type, and sort by offset within each group
    using Key = std::pair<int, cfg::Register_Types>;
    std::map<Key, std::vector<std::unique_ptr<IO_Work>>> grouped_works;

    for (auto &work : io_works)
    {
        grouped_works[{work->device_id, work->register_type}].push_back(std::move(work));
    }

    for (auto &[key, group] : grouped_works)
    {
        std::sort(group.begin(), group.end(), [](const std::unique_ptr<IO_Work> &a, const std::unique_ptr<IO_Work> &b)
                  { return a->offset < b->offset; });

        // 2. Merge Overlapping and Contiguous Ranges
        for (size_t i = 0; i < group.size(); ++i)
        {
            std::unique_ptr<IO_Work> merged = std::move(group[i]);
            size_t j = i + 1;

            // Continue merging as long as j is within bounds
            while (j < group.size())
            {
                // Check for fully contained range
                if (group[j]->offset >= merged->offset &&
                    group[j]->offset + group[j]->num_registers <= merged->offset + merged->num_registers)
                {
                    j++;
                    continue;
                }

                // Check for contiguous and combinable range
                if (merged->offset + merged->num_registers == group[j]->offset &&
                    merged->num_registers + group[j]->num_registers <= 125)
                {
                    // Merge operation (for buf8 and buf16 as necessary)
                    // memcpy(merged->buf8 + merged->num_registers, group[j]->buf8, group[j]->num_registers);
                    // memcpy(merged->buf16 + merged->num_registers, group[j]->buf16, group[j]->num_registers * sizeof(u16));
                    merged->num_registers += group[j]->num_registers;
                    j++;
                }
                else
                {
                    // Break if neither contained nor contiguous and combinable
                    break;
                }
            }

            compacted.push_back(std::move(merged));
            i = j - 1;
        }
    }

    return true;
}

// std::vector<IO_Work> compact_io_works(const std::vector<IO_Work>& io_works) {
//     std::vector<IO_Work> compacted;

//     // 1. Group IO_Work by device_id and register_type, and sort by offset within each group
//     using Key = std::pair<int, RegisterTypes>;
//     std::map<Key, std::vector<IO_Work>> grouped_works;
//     for (const auto& work : io_works) {
//         grouped_works[{work.device_id, work.register_type}].push_back(work);
//     }
//     for (auto& [key, group] : grouped_works) {
//         std::sort(group.begin(), group.end(), [](const IO_Work& a, const IO_Work& b) {
//             return a.offset < b.offset;
//         });

//         // 2. Merge Overlapping and Contiguous Ranges
//         for (size_t i = 0; i < group.size(); ++i) {
//             IO_Work merged = group[i];
//             size_t j = i + 1;

//             // Continue merging as long as j is within bounds
//             while (j < group.size()) {
//                 // Check for fully contained range
//                 if (group[j].offset >= merged.offset &&
//                     group[j].offset + group[j].num_registers <= merged.offset + merged.num_registers) {
//                     j++;
//                     continue;
//                 }

//                 // Check for contiguous and combinable range
//                 if (merged.offset + merged.num_registers == group[j].offset &&
//                     merged.num_registers + group[j].num_registers <= 125) {
//                     // Merge operation (for buf8 and buf16 as necessary)
//                     //memcpy(merged.buf8 + merged.num_registers, group[j].buf8, group[j].num_registers);
//                     //memcpy(merged.buf16 + merged.num_registers, group[j].buf16, group[j].num_registers * sizeof(u16));
//                     merged.num_registers += group[j].num_registers;
//                     j++;
//                 } else {
//                     // Break if neither contained nor contiguous and combinable
//                     break;
//                 }
//             }

//             compacted.push_back(merged);
//             i = j - 1;
//         }
//     }

//     return compacted;
// }

// #include <iostream>
// #include <cstring>
// #include <vector>
// #include <map>
// #include <tuple>
// #include <algorithm>

// Assuming necessary type definitions and IO_Work structure are already defined above

// ... [compact_io_works function here] ...

// Test Function
void show_io_works(const std::vector<std::unique_ptr<IO_Work>> &io_works)
{
    // Output the compacted IO_Work entries
    for (const auto &work_ptr : io_works)
    {
        const IO_Work &work = *work_ptr; // Dereference the unique_ptr
        std::cout << "Device ID: " << work.device_id << ", Type: " << static_cast<int>(work.register_type)
                  << ", Offset: " << work.offset << ", Num Registers: " << work.num_registers << std::endl;
    }
}

void batch_io_works(const std::vector<std::unique_ptr<IO_Work>> &io_works)
{
    // Output the compacted IO_Work entries
    int vsize = io_works.size();
    double tNow = get_time_double();
    int vnum = 0;
    for (auto &work_ptr : io_works)
    {
        IO_Work &work = *work_ptr; // Dereference the unique_ptr
        work.tNow = tNow;
        work.work_group = vsize;
        work.work_id = vnum++;
    }
}

void batch_io_works_type(const std::vector<std::unique_ptr<IO_Work>> &io_works, int register_type)
{
    // Output the compacted IO_Work entries
    int vsize = 0;
    for (auto &work_ptr : io_works)
    {
        IO_Work &work = *work_ptr; // Dereference the unique_ptr
        if ((int)work.register_type == register_type)
            vsize++;
    }

    double tNow = get_time_double();
    int vnum = 0;
    for (auto &work_ptr : io_works)
    {
        IO_Work &work = *work_ptr; // Dereference the unique_ptr
        if ((int)work.register_type == register_type)
        {

            work.tNow = tNow;
            work.work_group = vsize;
            work.work_id = vnum++;
        }
    }
}

// these are the test bad registers
std::vector<int> bad_registers;

bool set_bad_reg(int reg)
{
    // Check if reg is not already in bad_registers
    auto it = std::find(bad_registers.begin(), bad_registers.end(), reg);
    if (it == bad_registers.end())
    {                                 // reg not found in bad_registers
        bad_registers.push_back(reg); // Add reg to bad_registers
        return true;                  // Successfully added
    }
    return false; // reg was already in bad_registers
}

void clear_bad_regs()
{
    bad_registers.clear(); // Remove all elements from bad_registers
}

void clear_bad_reg(int reg)
{
    // Find and remove reg from bad_registers if it exists
    auto it = std::remove(bad_registers.begin(), bad_registers.end(), reg);
    bad_registers.erase(it, bad_registers.end()); // Necessary to actually remove the elements
}

// this enables a search for bad ioregs
// simulates finding a bad reg
bool process_io_range(const IO_Work &work, int start_register, int num_registers)
{
    for (int reg = start_register; reg < start_register + num_registers; ++reg)
    {

        if (std::binary_search(bad_registers.begin(), bad_registers.end(), reg))
        {
            std::cout << "Found bad reg " << reg << std::endl;
            return false;
        }
    } // Replace this function with the actual logic for processing IO requests for a range of registers
    std::cout << "Processed IO request for registers from " << start_register << " to " << start_register + num_registers - 1 << std::endl;
    return true;
}

void process_io_registers(const IO_Work &work, int start, int end, std::vector<int> &bad_registers)
{
    std::cout << " trying start  " << start << " => end " << end << std::endl;
    if (start > end)
        return;

    // If the range consists of a single register, it must be the bad one
    // Single register in the range, and it's bad
    if (start == end && !process_io_range(work, start, 1))
    {
        bad_registers.push_back(start);
        return;
    }

    if (process_io_range(work, start, end - start + 1))
        return;
    // Divide the range into two sub-ranges and check each
    int mid = start + (end - start) / 2;
    process_io_registers(work, start, mid, bad_registers);   // Check the first half
    process_io_registers(work, mid + 1, end, bad_registers); // Check the second half
}

void process_io(const IO_Work &work, std::vector<int> &bad_registers)
{
    int current_start = work.start_register;
    int num_disabled = 0;
    for (int reg = work.start_register; reg < work.start_register + work.num_registers; ++reg)
    {
        if (std::binary_search(work.disabled_registers.begin(), work.disabled_registers.end(), reg))
        {
            num_disabled++;

            // Call process_io_for_range function for registers in the range of [current_start, reg)
            if (!process_io_range(work, current_start, reg - current_start))
            {
                // If the range is invalid, find the bad registers within this range
                process_io_registers(work, current_start, reg - 1, bad_registers);
            }

            // Set the new current_start to be the next register after the disabled one
            current_start = reg + 1;
        }
    }
    std::cout << " Found num_disabled " << num_disabled << std::endl;

    // Process the remaining registers if there are any
    if (current_start < work.start_register + work.num_registers)
    {
        if (!process_io_range(work, current_start, work.start_register + work.num_registers - current_start))
        {
            int current_end = work.start_register + work.num_registers - current_start;
            std::cout << " process_io_range failed current_start " << current_start << " current_end " << current_end << std::endl;

            // If the remaining range is invalid, find the bad registers within this range
            process_io_registers(work, current_start, current_end, bad_registers);
        }
    }
}

int test_find_bad_regs()
{

    std::vector<int> bad_registers;

    set_bad_reg(25);
    set_bad_reg(32);

    std::cout << "Setup Bad registers: " << std::endl;

    for (int reg : bad_registers)
    {
        std::cout << reg << " ";
    }
    std::cout << std::endl;

    IO_Work io_work;
    io_work.start_register = 10;
    io_work.num_registers = 50;

    process_io(io_work, bad_registers);

    std::cout << "Bad registers at end: " << std::endl;

    for (int reg : bad_registers)
    {
        std::cout << reg << " ";
    }
    std::cout << std::endl;

    io_work.disabled_registers.push_back(25);
    io_work.disabled_registers.push_back(32);

    bad_registers.clear();

    process_io(io_work, bad_registers);

    std::cout << "Bad registers after pass 2: " << std::endl;

    for (int reg : bad_registers)
    {
        std::cout << reg << " ";
    }
    std::cout << std::endl;

    std::cout << std::endl;

    return 0;
}

// #include <map>
// #include <vector>
// #include <functional>
// #include <iostream>

// ... [Your IO_Work struct and other necessary includes] ...

class IOWorkCollator
{
    std::map<double, std::vector<std::unique_ptr<IO_Work>>> workMap;

    // Define the type of the callback
    std::function<void(const std::vector<std::unique_ptr<IO_Work>> &)> callback;

public:
    // Allow setting the callback
    void setCallback(std::function<void(const std::vector<std::unique_ptr<IO_Work>> &)> cb)
    {
        callback = cb;
    }

    void addWork(std::unique_ptr<IO_Work> work)
    {
        double tNow = work->tNow;
        workMap[tNow].push_back(std::move(work));

        if (static_cast<int>(workMap[tNow].size()) == workMap[tNow].front()->work_group)
        {
            // If we have a valid callback, call it
            if (callback)
            {
                callback(workMap[tNow]);
            }
            workMap.erase(tNow);
        }
    }
};

// class IOWorkCollator {
//     std::map<double, std::vector<IO_Work>> workMap;

//     // Define the type of the callback
//     std::function<void(const std::vector<IO_Work>&)> callback;

// public:
//     // Allow setting the callback
//     void setCallback(std::function<void(const std::vector<IO_Work>&)> cb) {
//         callback = cb;
//     }

//     void addWork(const IO_Work& work) {
//         workMap[work.tNow].push_back(work);

//         // Check if the group is complete
//         std::cout << " got item for " << work.tNow << " expected size :"<< work.work_group << " current: "<< (int)workMap[work.tNow].size()<<std::endl;
//         if ((int)workMap[work.tNow].size() == work.work_group) {
//             // If we have a valid callback, call it
//             if(callback) {
//                 callback(workMap[work.tNow]);
//             }
//             workMap.erase(work.tNow);
//         }
//     }
// };

// this needs the cfg stuff and also needs to find data points in the cfg
// then it needs to create the output object associated with the decoded data.
// we need to pub the whole lot.
// unless its filtered.
// so just put the value in the cfg object after decode for now.

// we'll have a device_id , a type and an offset from the WORK object , look up the data point

// Standalone callback function
void processGroup(const std::vector<std::unique_ptr<IO_Work>> &group)
{
    std::cout << "Received a complete group with " << group.size() << " io_points.\n";

    // we'll proceed to pub the group

    // Further processing here if needed...
}

// // Usage:
// void onGroupComplete(const std::vector<IO_Work>& group) {
//     const IO_Work *item = &group[0];
//     double tid = item->tNow;

//     std::cout<<" IO work Group complete; size: "<< group.size() << "Group id " <<  tid << std::endl;
//     // Handle the complete group
//     // e.g., prepare this group for dispatch
// }

// this will be in the response thread
void processIOWork()
{
    IOWorkCollator collator;

    // Set the callback
    collator.setCallback(processGroup);

    // ... Populate your io_works vector ...

    // for (const auto& work : io_works) {
    //     collator.addWork(work);
    // }
}

bool send_io_vec(std::vector<std::unique_ptr<IO_Work>> &compacted_io_works, std::vector<std::unique_ptr<IO_Work>> &test_io_works)
{

    std::cout << " raw IO work requests " << std::endl;
    show_io_works(test_io_works);

    compact_io_works(compacted_io_works, test_io_works);

    batch_io_works_type(compacted_io_works, (int)RegisterTypes::TYPE_B);
    batch_io_works_type(compacted_io_works, (int)RegisterTypes::TYPE_A);

    std::cout << " compacted IO work requests " << std::endl;
    show_io_works(compacted_io_works);

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(compacted_io_works.begin(), compacted_io_works.end(), g);
    std::cout << " shufffled compacted IO work requests " << std::endl;
    show_io_works(compacted_io_works);

    return true;
}

void test_compact_io_works()
{
    std::vector<std::unique_ptr<IO_Work>> test_io_works;
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Holding, 10, 5)));
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Holding, 15, 5)));
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Holding, 50, 5)));
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Coil, 10, 100)));
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Coil, 110, 50)));
    test_io_works.push_back(std::make_unique<IO_Work>(IO_workItem(1, cfg::Register_Types::Coil, 112, 5)));

    std::vector<std::unique_ptr<IO_Work>> compacted_io_works;
    send_io_vec(compacted_io_works, test_io_works);

    // this is the return path
    IOWorkCollator collator;

    // Set the callback
    collator.setCallback(processGroup);

    for (auto &work : compacted_io_works)
    {
        collator.addWork(std::move(work));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// If `ioChannel` itself is not already thread-safe, you're correct that you will need to ensure safe access when the channel is shared across multiple threads, especially when you've encapsulated it inside the `ThreadControl` object.

// One way to provide safe access is by introducing thread-safe methods for the operations you require in the `ThreadControl` class. Here's an example for your specific case:

// This way, the `ThreadControl` class ensures that any access to the channels is thread-safe. Any thread that interacts with the channels will do so only through the methods provided by `ThreadControl`, ensuring consistent and safe access.
// IO Thread Function
// void ioThreadFunc(ThreadControl& control, std::shared_ptr<IO_Thread> io_thread) {
#include <iostream>
#include <stdexcept>
#include <cfenv> // For standard floating point functions

// int main() {
//      std::feclearexcept(FE_ALL_EXCEPT);  // Clear any existing exceptions

//     try {
//         std::fesetexceptflag(nullptr, FE_ALL_EXCEPT);  // Enable all floating point exceptions
//         std::feenableexcept(FE_DIVBYZERO);  // Enable the division by zero exception

//         double x = 1.0;
//         double y = 0.0;
//         double z = x / y;  // This will cause a division by zero

//         std::cout << z << std::endl;  // This line won't be executed
//     }
//     catch (const std::runtime_error& e) {
//         std::cerr << "Caught a floating point exception: " << e.what() << std::endl;
//     }

//     return 0;
// }

void runThreadError(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool &io_done, int &io_tries, int &max_io_tries, bool debug)
{
    io_thread->fails++;

    if (io_thread->ctx == nullptr && io_thread->ip != "")
    {
        for (int i = 0; i < 5 && io_thread->ctx == nullptr; ++i)
        {
            {

                std::lock_guard<std::mutex> lock2(io_output_mutex);
                if (debug)
                    std::cout << " getting modbus ctx #1 " << std::endl;
                auto ctime =
                    SetupModbusForThread(myCfg, io_thread, debug);
                io_work->cTime = ctime;
                
                if (debug)
                    std::cout << "   done ModbusSetup; connect_time :" << ctime << std::endl;
            }
            if (!io_thread->ctx)
            {
                std::lock_guard<std::mutex> lock2(io_output_mutex);
                FPS_ERROR_LOG("thread_id %d  try %d Connection attempt failed ",
                              io_thread->tid, io_tries);
                // int mberr = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    }
    else if (io_work->errno_code == 9)
    {
        int mberr = 0;
        if (io_thread->ctx)
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            // mberr = modbus_connect(io_thread->ctx);
            FPS_ERROR_LOG("thread_id %d  try %d  ctx %p  bad  connect %d",
                          io_thread->tid, io_tries,
                          (void *)io_thread->ctx,
                          mberr);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    else if (io_work->errno_code == 110)
    {
        int mberr = 0;
        if (io_thread->ctx)
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            // mberr = modbus_connect(io_thread->ctx);
            FPS_ERROR_LOG("thread_id %d  try %d  timeout on  connect %d",
                          io_thread->tid, io_tries,
                          //(void *)io_thread->ctx,
                          mberr);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    else if (io_work->errno_code == 115)
    {
        FPS_ERROR_LOG("thread_id %d  try %d  error on  connect",
                      io_thread->tid, io_tries
                      //(void *)io_thread->ctx,
        );

        modbus_close(io_thread->ctx);
        // modbus_free(io_thread->ctx);

        std::cout << " getting modbus ctx #2 " << std::endl;
        io_thread->ctx = modbus_new_tcp(io_thread->ip.c_str(), io_thread->port);
        std::cout << "   done modbus ctx #2 " << std::endl;

        if (io_thread->ctx)
            // mberr =
            modbus_connect(io_thread->ctx);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    else if (io_work->errno_code == 88)
    {
        int mberr = 0;
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);

            modbus_free(io_thread->ctx);

            std::cout << " getting modbus ctx #2 " << std::endl;
            io_thread->ctx = modbus_new_tcp(io_thread->ip.c_str(), io_thread->port);
            std::cout << "   done modbus ctx #2 " << std::endl;

            if (io_thread->ctx)
                mberr = modbus_connect(io_thread->ctx);
            else
                mberr = -2;
        }

        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            FPS_INFO_LOG("thread_id %d  tries %d new  ctx %p  mberr %d",
                         io_thread->tid, io_tries,
                         (void *)io_thread->ctx,
                         mberr);
        }
        // auto mberr =
        // modbus_close(io_thread->ctx);
        // std::cout<< std::dec << " modbus_close mberr:" << 0<<std::endl;
        // auto mberr = modbus_flush(io_thread->ctx);
        // std::cout<< std::dec << " modbus_connect  mberr:" << mberr<<std::endl;
        // FPS_INFO_LOG("thread_id %d  ctx flush", io_thread->tid) ;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // mberr = modbus_connect(io_thread->ctx);
        // std::cout<< std::dec << " Connect modbus mberr:" << mberr<<std::endl;

        // modbus_connect(io_thread->ctx);
    }
    else if (io_work->errno_code == 112345681)
    {
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            FPS_INFO_LOG("thread_id %d  tries %d ; Illegal Data Value in %d to %d , aborting for now",
                         io_thread->tid, io_tries,
                         io_work->offset,
                         (io_work->offset + io_work->num_registers)

                         //(void *)io_thread->ctx,
            );
            io_done = true;
            // io_tries = max_io_tries;
        }
    }
    else if (io_work->errno_code == BAD_DATA_ADDRESS)
    {
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            FPS_INFO_LOG("thread_id %d  tries %d ; Bad Data Address in %d to %d , aborting for now",
                         io_thread->tid, io_tries,
                         io_work->offset,
                         (io_work->offset + io_work->num_registers)

                         //(void *)io_thread->ctx,
            );
            io_done = true;
            // io_tries = max_io_tries;
        }
    }
    else if (io_work->errno_code == 112345691)
    {
        // modbus_free(io_thread->ctx);

        // io_thread->ctx = modbus_new_tcp(io_thread->ip.c_str(), io_thread->port);
        // auto mberr = modbus_connect(io_thread->ctx);

        // auto mberr =
        // modbus_close(io_thread->ctx);
        // std::cout<< std::dec << " modbus_close mberr:" << 0<<std::endl;
        auto mberr = modbus_flush(io_thread->ctx);
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            FPS_INFO_LOG("thread_id %d  tries %d flush mberr %d",
                         io_thread->tid, io_tries,
                         //(void *)io_thread->ctx,
                         mberr);
        }
        // std::cout<< std::dec << " modbus_flush  mberr:" << mberr<<std::endl;
        // FPS_INFO_LOG("thread_id %d  ctx flush", io_thread->tid) ;

        // std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // mberr = modbus_connect(io_thread->ctx);
        // std::cout<< std::dec << " Connect modbus mberr:" << mberr<<std::endl;

        // modbus_connect(io_thread->ctx);
    }
    // auto fstr = fmt::format("thread_{}_error", io_thread->tid);
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("thread_id %d  tries %d errno_code %d",
                     io_thread->tid, io_tries,
                     //(void *)io_thread->ctx,
                     io_work->errno_code);
        FPS_LOG_IT("thread_io_error");
    }
}

// in all of these we'll tackle the single / multi option, the off by one  and the auto disable
// the forced / unforced / debounce value option should have been taken care of already
// the local / remote io_work option will be handled here

void local_write_registers(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    int idx = 0;
    for (auto io_point : io_work->io_points)
    {
        memcpy(io_point->reg16, &io_work->buf16[idx], io_point->size * 2);
        idx += io_point->size;
    }
    io_work->errors = idx;
}

void local_read_registers(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    int idx = 0;
    for (auto io_point : io_work->io_points)
    {
        memcpy(&io_work->buf16[idx], io_point->reg16, io_point->size * 2);
        idx += io_point->size;
    }
    io_work->errors = idx;
}

void local_write_bits(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    int idx = 0;
    for (auto io_point : io_work->io_points)
    {
        memcpy(io_point->reg8, &io_work->buf8[idx], io_point->size);
        idx += io_point->size;
    }
    io_work->errors = idx;
}

void local_read_bits(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    int idx = 0;
    for (auto io_point : io_work->io_points)
    {
        memcpy(&io_work->buf8[idx], io_point->reg8, io_point->size);
        idx += io_point->size;
    }
    io_work->errors = idx;
}

void local_write_work(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if ((io_work->register_type == cfg::Register_Types::Holding) || (io_work->register_type == cfg::Register_Types::Input))
        local_write_registers(myCfg, io_work, debug);
    else if ((io_work->register_type == cfg::Register_Types::Coil) || (io_work->register_type == cfg::Register_Types::Discrete_Input))
        local_write_bits(myCfg, io_work, debug);
}

void local_read_work(struct cfg &myCfg, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if ((io_work->register_type == cfg::Register_Types::Holding) || (io_work->register_type == cfg::Register_Types::Input))
        local_read_registers(myCfg, io_work, debug);
    else if ((io_work->register_type == cfg::Register_Types::Coil) || (io_work->register_type == cfg::Register_Types::Discrete_Input))
        local_read_bits(myCfg, io_work, debug);
}

void handle_point_error(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<struct cfg::io_point_struct> io_point, const char *func_name, int err)
{
    io_point->errno_code = err;
    if (myCfg.auto_disable)
    {
        io_point->is_enabled = false;
        FPS_INFO_LOG("thread_id %d %s failed for [%s] offset %d point disabled ", io_thread->tid, func_name, io_point->id.c_str(), io_point->offset);
    }
    else
    {
        FPS_INFO_LOG("thread_id %d %s failed for [%s] offset %d", io_thread->tid, func_name, io_point->id.c_str(), io_point->offset);
    }
}

void io_write_registers(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers * 2;
        return;
    }
    int offset = 0;
    bool allowMulti = myCfg.allow_multi_sets;
    if (io_work->off_by_one)
        offset++;
    if (allowMulti)
    {
        io_work->errors = modbus_write_registers(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf16);
    }
    if (!allowMulti || io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_write_registers(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf16[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "write registers", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_write_register(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers * 2;
        return;
    }

    int offset = 0;
    int idx = 0;
    int errors = 0;
    io_work->errors = 0;
    if (io_work->off_by_one)
        offset++;
    // there should only be one register
    for (auto io_point : io_work->io_points)
    {
        if (io_point->multi_write_op_code || io_point->size > 1)
        {
            errors = modbus_write_registers(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf16[idx]);
            // std::cout << "write_registers errors " << errors << std::endl;
        }
        else
        {
            errors = modbus_write_register(io_thread->ctx, io_point->offset - offset, io_work->buf16[idx]);
            // std::cout << "io_point ["<<  io_point->id<< "] write_register error " << errors << std::endl;
        }
        if (errors < 0)
        {
            auto err = errno;
            handle_point_error(myCfg, io_thread, io_point, "write register", err);
        }
        else
        {
            io_work->errors += errors;
        }

        idx += io_point->size;
    }
}

void io_read_registers(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        printf("io_read_registers local get \n");
        io_work->errors = io_work->num_registers * 2;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;

    io_work->errors = modbus_read_registers(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf16);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_read_registers(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf16[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "read registers", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

// void io_read_register(struct cfg& myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work,  bool debug)
// {
//     io_work->errors = modbus_read_register(io_thread->ctx, io_work->offset, io_work->buf16);
// }

void io_read_input_registers(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers * 2;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;

    io_work->errors = modbus_read_input_registers(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf16);
    if (io_work->errors < 0)
    {
        io_work->errno_code = errno;
        // FPS_INFO_LOG("thread_id %d first read input_registers failed  offset %d  num %d off_by_one %d  err %s"
        //                 , io_thread->tid, io_work->offset, io_work->num_registers , offset, modbus_strerror(io_work->errno_code)) ;
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_read_input_registers(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf16[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "read input registers", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_read_bit(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;
    io_work->errors = modbus_read_bits(io_thread->ctx, io_work->offset - offset, io_work->num_registers, &io_work->buf8[0]);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_read_bits(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf8[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "read bits", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_write_bit(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;
    io_work->errors = modbus_write_bit(io_thread->ctx, io_work->offset - offset, io_work->buf8[0]);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_write_bit(io_thread->ctx, io_point->offset - offset, io_work->buf8[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "write bit", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_write_bits(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;
    io_work->errors = modbus_write_bits(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf8);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_write_bits(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf8[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "write bits", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_read_bits(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;
    io_work->errors = modbus_read_bits(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf8);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_read_bits(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf8[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "read bits", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

void io_read_input_bits(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    if (io_work->local)
    {
        io_work->errors = io_work->num_registers;
        return;
    }
    int offset = 0;
    if (io_work->off_by_one)
        offset++;
    io_work->errors = modbus_read_input_bits(io_thread->ctx, io_work->offset - offset, io_work->num_registers, io_work->buf8);
    if (io_work->errors < 0)
    {
        io_work->errors = 0;
        int idx = 0;
        int errors;
        for (auto io_point : io_work->io_points)
        {
            errors = modbus_read_input_bits(io_thread->ctx, io_point->offset - offset, io_point->size, &io_work->buf8[idx]);
            if (errors < 0)
            {
                auto err = errno;
                handle_point_error(myCfg, io_thread, io_point, "read input bits", err);
            }
            else
            {
                io_work->errors += errors;
            }
            idx += io_point->size;
        }
    }
}

// process_io_range
void runThreadWork(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug)
{
    io_work->tIo = get_time_double();
    if ((io_work->wtype == WorkTypes::Set) || (io_work->wtype == WorkTypes::SetMulti))
    {
        myCfg.performance_timers.modbus_write_timer.start();
    }
    else
    {
        myCfg.performance_timers.modbus_read_timer.start();
    }
    io_work->threadId = io_thread->tid;
    io_thread->jobs++;
    if (io_thread->ctx)
    {
        // std::cout << " Set device_id " << io_work->device_id << std::endl;
        modbus_set_slave(io_thread->ctx, io_work->device_id);
    }
    else
    {
        std::cout << " Note no context set up " << std::endl;
    }
    // modbus_strerror(set_work.errno_code)

    // Register_Types::Holding:          poll_errors = modbus_read_registers(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u16);
    // Register_Types::Input:            poll_errors = modbus_read_input_registers(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u16);
    // Register_Types::Coil:             poll_errors = modbus_read_bits(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u8);
    // Register_Types::Discrete_Input:   poll_errors = modbus_read_input_bits(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u8);
    // Register_Types::Holding            set_errors = modbus_write_register(my_workspace.ctx, curr_decode.offset, set_buffer[0]);
    //                                    set_errors = modbus_write_registers(my_workspace.ctx, curr_decode.offset, curr_decode.flags.get_size(), set_buffer);
    // Register_Types::Coil:              set_errors = modbus_write_bit(my_workspace.ctx, offset, val.set_val.u == 1UL);
    int io_tries = 0;
    // int max_io_tries = 10;
    int max_io_tries = myCfg.max_io_tries;
    bool io_done = false;

    while (!io_done && (io_tries < max_io_tries))
    {
        io_tries++;

        io_work->errors = -2; // flag a no go
        double tNow = get_time_double();
        bool setMulti = (io_work->wtype == WorkTypes::SetMulti) || myCfg.force_multi_sets;
        // bool getMulti = (io_work->wtype == WorkTypes::GetMulti) || myCfg.force_multi_sets;

        if ((io_work->wtype == WorkTypes::Set) || (io_work->wtype == WorkTypes::SetMulti))
        {
            if (0)
            {
                FPS_INFO_LOG("thread_id %d Set >> work type %d (%s)  offset %d num %d multi %d reg_type %d  (%s) ",
                             io_thread->tid, (int)io_work->wtype, workTypeToStr(io_work->wtype).c_str(), io_work->offset,
                             io_work->num_registers, (int)setMulti, (int)io_work->register_type, regTypeToStr(io_work->register_type).c_str());
                // printf("                   >>>>thread_id %d Get >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;
            }
            if (io_work->register_type == cfg::Register_Types::Holding)
            {
                if ((io_work->wtype == WorkTypes::Set) || (io_work->wtype == WorkTypes::SetMulti))
                {
                    if ((io_work->num_registers == 1) && !setMulti)
                    {
                        io_write_register(myCfg, io_thread, io_work, debug);
                        // printf("                   >>>>thread_id %d Set >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;

                        // io_work->errors = modbus_write_register(io_thread->ctx, io_work->offset, io_work->buf16[0]);
                    }
                    else
                    {
                        io_write_registers(myCfg, io_thread, io_work, debug);
                        // io_work->errors = modbus_write_registers(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf16);
                    }
                    if (0)
                    {
                        FPS_INFO_LOG("thread_id %d Set >> Holding : offset %d errors %d ", io_thread->tid, io_work->offset, io_work->errors);
                        // printf("                   >>>>thread_id %d Get >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;
                    }
                }

                else if ((io_work->wtype == WorkTypes::Get) || (io_work->wtype == WorkTypes::GetMulti))
                {
                    io_read_registers(myCfg, io_thread, io_work, debug);
                    // io_work->errors = modbus_read_registers(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf16);
                    if (0)
                    {
                        FPS_INFO_LOG("thread_id %d Get >> Holding : errors %d ", io_thread->tid, io_work->errors);
                        // printf("                   >>>>thread_id %d Get >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;
                    }
                }
                else
                {
                    FPS_INFO_LOG("thread_id %d Uknown  Type ", io_thread->tid);
                }
            }
            else if (io_work->register_type == cfg::Register_Types::Coil)
            {
                if ((io_work->wtype == WorkTypes::Set) || (io_work->wtype == WorkTypes::SetMulti))
                {
                    if ((io_work->num_registers == 1) && !setMulti)
                    {
                        io_write_bit(myCfg, io_thread, io_work, debug);
                        // io_work->errors = modbus_write_bit(io_thread->ctx, io_work->offset, io_work->buf8[0]);
                    }
                    else
                    {
                        io_write_bits(myCfg, io_thread, io_work, debug);
                        // io_work->errors = modbus_write_bits(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf8);
                    }
                }
                else if ((io_work->wtype == WorkTypes::Get) || (io_work->wtype == WorkTypes::GetMulti))
                {
                    io_read_bits(myCfg, io_thread, io_work, debug);
                    // io_work->errors = modbus_read_bits(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf8);
                }
            }
        }
        else if (io_work->wtype == WorkTypes::Get)
        {
            if (io_work->register_type == cfg::Register_Types::Holding)
            {
                io_read_registers(myCfg, io_thread, io_work, debug);
                if (myCfg.connection.debug)
                {
                    FPS_INFO_LOG("thread_id %d Get >> Holding : errors %d ", io_thread->tid, io_work->errors);
                    // printf("                   >>>>thread_id %d Get >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;
                }
            }
            else if (io_work->register_type == cfg::Register_Types::Input)
            {
                io_read_input_registers(myCfg, io_thread, io_work, debug);
                // io_work->errors = modbus_read_input_registers(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf16);
                if (myCfg.connection.debug)
                {
                    FPS_INFO_LOG("thread_id %d Get >> Input : errors %d ", io_thread->tid, io_work->errors);
                    // printf("                   >>>>thread_id %d Get >> Holding : errors %d \n",io_thread->tid, io_work->errors) ;
                }
            }
            else if (io_work->register_type == cfg::Register_Types::Coil)
            {
                io_read_bits(myCfg, io_thread, io_work, debug);
                // io_work->errors = modbus_read_bits(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf8);
            }
            else if (io_work->register_type == cfg::Register_Types::Discrete_Input)
            {
                io_read_input_bits(myCfg, io_thread, io_work, debug);
                // io_work->errors = modbus_read_input_bits(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf8);
                //  printf("                   >>>>thread_id %d Get >> Discrete Input : errors %d \n",io_thread->tid, io_work->errors) ;
                //   {
                //       FPS_ERROR_LOG("thread_id %d  dev_id %d Get >> Discrete_Inputs : \t%d errors %d ",
                //               io_thread->tid,
                //               io_work->device_id,
                //                (int)RegisterTypes::Holding, io_work->errors) ;
                //   }
            }
        }
        else if (io_work->register_type == cfg::Register_Types::Input)
        {
            io_read_input_registers(myCfg, io_thread, io_work, debug);
            // io_work->errors = modbus_read_input_registers(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf16);
            //  printf("                   >>>>thread_id %d Get >> Input : errors %d \n",io_thread->tid, io_work->errors) ;
        }
        else if (io_work->register_type == cfg::Register_Types::Discrete_Input)
        {
            io_read_input_bits(myCfg, io_thread, io_work, debug);
            // io_work->errors = modbus_read_input_bits(io_thread->ctx, io_work->offset, io_work->num_registers, io_work->buf8);
            //  printf("                   >>>>thread_id %d Get >> Discrete Input : errors %d \n",io_thread->tid, io_work->errors) ;
            //   {
            //       FPS_ERROR_LOG("thread_id %d  dev_id %d Get >> Discrete_Inputs : \t%d errors %d ",
            //               io_thread->tid,
            //               io_work->device_id,
            //                (int)RegisterTypes::Holding, io_work->errors) ;
            //   }
        }

        // random delay from 100 to 500 uSecs
        else if (io_work->wtype == WorkTypes::Noop)
        {
            io_done = true;
            ;

            randomDelay(100, 500);
            io_work->errors = 1;
        }

        double tRun = get_time_double() - tNow;
        if ((io_work->work_name == "test_io_point_single") && (io_work->test_it))
        {
            // make it worse if we are testing
            std::cout << "adding an extra 1200ms to try to make this late\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        }
        if (io_work->errors >= 0)
        {
            io_done = true;
        }

        io_work->errno_code = errno;
        io_work->tRun = tRun;

        if (debug || io_work->errors < 0)
        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            if (io_work->errors < 0)
            {
                FPS_ERROR_LOG("thread_id %d try %d type %d offset %d error %d code  %d -> %s ",
                              io_thread->tid, io_tries, (int)io_work->register_type, io_work->offset,
                              //(void *)io_thread->ctx,
                              io_work->errors, io_work->errno_code, modbus_strerror(io_work->errno_code));
            }
            else
            {
                if (myCfg.connection.debug)
                    FPS_INFO_LOG("thread_id %d Success, try %d type %d offset %d num %d tRun(mS) %f inreg [%d] inbit [%d] ", io_thread->tid, io_tries, (int)io_work->register_type, io_work->offset, io_work->errors, tRun * 1000.0, io_work->buf16[0], io_work->buf8[0]);
            }
            // if (debug)
            // {
            //     FPS_ERROR_LOG("thread_id %d Holding : \t%d ",io_thread->tid, (int)RegisterTypes::Holding) ;
            //     FPS_ERROR_LOG("thread_id %d Input : \t%d ",io_thread->tid, (int)RegisterTypes::Input) ;
            //     FPS_ERROR_LOG("thread_id %d Coil : \t%d ",io_thread->tid, (int)RegisterTypes::Coil) ;
            //     FPS_ERROR_LOG("thread_id %d Discrete_Input : \t%d ",io_thread->tid, (int)RegisterTypes::Discrete_Input) ;

            // }
        }
        if ((io_work->errors > 0) || (io_work->errors == -2))
        { // got successful communication
            // io_tries = max_io_tries;
            io_done = true;
        }
        else
        {
            // if (io_work->test_range) return false
            runThreadError(myCfg, io_thread, io_work, io_done, io_tries, max_io_tries, debug);
        }
    }
    io_work->tDone = get_time_double();
    if ((io_work->wtype == WorkTypes::Set) || (io_work->wtype == WorkTypes::SetMulti))
    {
        myCfg.performance_timers.modbus_write_timer.snap();
    }
    else
    {
        myCfg.performance_timers.modbus_read_timer.snap();
    }

    io_responseChan.send(std::move(io_work));
}

// thread function
void ioThreadFunc(ThreadControl &control, struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread)
{
    // int id = tid;
    std::shared_ptr<IO_Work> io_work;
    int signal;
    bool run = true;
    bool debug = true;
    double delay = 0.1;

    if (io_thread->ip != "")
    {
        for (int i = 0; i < 5 && io_thread->ctx == nullptr; ++i)
        {
            double ctime = SetupModbusForThread(myCfg, io_thread, debug);
            // std::cout << "   done ModbusSetup; connect_time :"<< ctime << std::endl;
            io_thread->cTime = ctime; // TODO use perf
        }
    }
    while (run && control.ioThreadRunning)
    {
        if (io_threadChan.receive(signal, delay))
        {
            if (signal == 0)
                run = false;
            if (signal == 1)
            {
                // try {
                //     std::fesetexceptflag(nullptr, FE_ALL_EXCEPT);  // Enable all floating point exceptions
                //     feenableexcept(FE_DIVBYZERO);  // Enable the division by zero exception

                if (io_setChan.peekpop(io_work))
                {
                    // if (io_pollChan.receive(io_work)) {
                    // std::cout << " runThreadwork for set" << std::endl;

                    runThreadWork(myCfg, io_thread, io_work, debug);
                }
                if (io_pollChan.peekpop(io_work))
                {
                    // if (io_pollChan.receive(io_work)) {
                    runThreadWork(myCfg, io_thread, io_work, debug);
                }
                // catch (const std::runtime_error& e) {
                //     std::cerr << "Caught a floating point exception: " << e.what() << std::endl;
                //   }
                //}
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("thread_id %d stopping after %d jobs  %d fails", io_thread->tid, io_thread->jobs, io_thread->fails);
        CloseModbusForThread(io_thread, debug);
    }
}

void clearpollChan(bool debug)
{
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    if (debug)
    {
        std::cout << " Clearing pollChan" << std::endl;
    }
    while (io_pollChan.receive(io_work, delay))
    {
        // runThreadWork(io_thread, io_work, debug);
        if (debug)
        {
            std::cout
                << "  start " << io_work.get()->offset
                << "  number " << io_work.get()->num_registers
                << std::endl;
        }
    }
    if (debug)
    {
        std::cout << " Cleared pollChan" << std::endl;
    }
}

void clearsetChan(bool debug)
{
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    if (debug)
    {
        std::cout << " Clearing setChan" << std::endl;
    }
    while (io_setChan.receive(io_work, delay))
    {
        // runThreadWork(io_thread, io_work, debug);
        if (debug)
        {
            std::cout
                << "  start " << io_work.get()->offset
                << "  number " << io_work.get()->num_registers
                << std::endl;
        }
    }
    if (debug)
    {
        std::cout << " Cleared setChan" << std::endl;
    }
}

void clearrespChan(bool debug)
{
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    if (debug)
    {
        std::cout << " Clearing respChan" << std::endl;
    }
    while (io_responseChan.receive(io_work, delay))
    {
        // runThreadWork(io_thread, io_work, debug);
        if (debug)
        {
            std::cout
                << "  start " << io_work.get()->offset
                << "  number " << io_work.get()->num_registers
                << std::endl;
        }
    }
    if (debug)
    {
        std::cout << " Cleared responseChan" << std::endl;
    }
}
// fixes tStart
// test function we'll have to set up some test data
//
void sendpollChantoResp(bool debug)
{
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    if (debug)
    {
        std::cout << " Pulling  pollChan" << std::endl;
    }
    while (io_pollChan.receive(io_work, delay))
    {
        io_work.get()->tStart = get_time_double();

        // io_responseChan.send(io_work);
        // runThreadWork(io_thread, io_work, debug);
        if (debug)
        {
            std::cout
                << "  start " << io_work.get()->offset
                << "  number " << io_work.get()->num_registers
                << std::endl;
        }
        io_responseChan.send(std::move(io_work));
    }
    if (debug)
    {
        std::cout << " Moved to resp channel" << std::endl;
    }
}

// here is the collector for the in flight pubgroups.
// struct PubGroup {
//     std::string key;
//     std::vector<std::shared_ptr<IO_Work>> works;
//     int work_group;
//     double tNow;
//     bool done;
//     bool erase;
// };

std::map<std::string, PubGroup> pubGroups;

// this will decode the io_work io_points
// and produce the fims output message

void processGroupCallback(struct PubGroup pub_group, struct cfg &myCfg)
{
    // auto compsh = pub_group.component.lock();
    double tNow = get_time_double();
    bool debug = myCfg.connection.debug;
    // debug = true;
    if (debug)
        std::cout << " processing workgroup " << pub_group.key
                  << " created at :" << pub_group.tNow
                  << " process time mS " << (tNow - pub_group.tNow) * 1000.0
                  << std::endl;

    // TODO decode and create body
    // std::map<std::string,std::map<std::string,std::any>> pubmap;

    // fmt::memory_buffer send_buf;
    // send_buf.push_back('{');
    //  flag this group as complete
    //  TODO I think this is OK since only the responseThread run this but check anyway.

    std::string pub_key = "pub_";
    std::string set_key = "set_";
    std::string get_key = "get_";

    if (pub_group.key.substr(0, pub_key.length()) == pub_key)
    {
        myCfg.performance_timers.pub_timer.start();
        pub_group.done = true;
        std::stringstream string_stream;
        std::string uri;
        bool first = true;
        string_stream << "{";
        for (auto io_work : pub_group.works)
        {
            if (first)
            {
                uri = "/" + io_work->components->component_id + "/" + io_work->components->id;
                first = false;
            }
            else
            {
                string_stream << ",";
            }
            gcom_modbus_decode(io_work, string_stream, myCfg);
            store_raw_data(io_work, myCfg.debug_decode);
        }
        string_stream << "}";

        send_pub(myCfg.fims_gateway, uri, string_stream.str());
        myCfg.performance_timers.pub_timer.snap();
        if (myCfg.connection.debug)
        {
            std::cout << "Published to: " << std::endl;
            std::cout << "\"" << uri << "\": " << string_stream.str() << std::endl;
        }
    }
    else if (pub_group.key.substr(0, set_key.length()) == set_key)
    {
        pub_group.done = true;
        std::stringstream string_stream;
        std::string uri;
        bool first = true;
        bool send = true;
        string_stream << "{";
        for (auto io_work : pub_group.works)
        {
            if (io_work->replyto == "")
            {
                send = false;
                break;
            }
            if (first)
            {
                // uri = "/"+ io_work->components->component_id + "/" + io_work->components->id;
                uri = io_work->replyto;
                first = false;
            }
            else
            {
                string_stream << ",";
            }
            gcom_modbus_decode(io_work, string_stream, myCfg);
            store_raw_data(io_work, false);
            if (debug)
                std::cout << __func__ << " offset :" << io_work->offset << " string so far [[" << string_stream.str() << "]]" << std::endl;
        }
        string_stream << "}";

        if (send)
            send_set(myCfg.fims_gateway, uri, string_stream.str());
    }
    else if (pub_group.key.substr(0, get_key.length()) == get_key)
    {
        pub_group.done = true;
        std::stringstream ss;
        std::string uri;
        bool first = true;
        bool send = true;
        ss << "{";
        for (auto io_work : pub_group.works)
        {
            if (debug)
                std::cout << __func__ << " offset :" << io_work->offset << " get string so far [[" << ss.str() << "]]" << std::endl;

            if (io_work->replyto == "")
            {
                send = false;
                if (debug)
                    std::cout << __func__ << "  no replyto " << std::endl;
                break;
            }
            else
            {
                if (debug)
                    std::cout << __func__ << "  found replyto [" << io_work->replyto << "]" << std::endl;
            }
            if (first)
            {
                uri = io_work->replyto;
                //"/"+ io_work->components->component_id + "/" + io_work->components->id;
                first = false;
            }
            else
            {
                ss << ",";
            }
            gcom_modbus_decode_debug(io_work, ss, myCfg, true, true);
            if (!io_work->local)
                local_write_work(myCfg, io_work, debug);
        }
        ss << "}";
        if (debug)
        {
            std::cout << __func__ << " get string at end [[" << ss.str() << "]]" << std::endl;
            std::cout << " uri [[" << uri << "]]" << std::endl;
        }

        if (send)
        {
            if (debug)
                std::cout << " uri [" << uri << "] sent" << std::endl;
            send_set(myCfg.fims_gateway, uri, ss.str());
        }
    }

    //     auto components = io_work.get()->components;
    //     // printf(" <%s> components %p \n", __func__, (void*)components.get()
    //     //             );
    //     if(!components) {
    //         if(io_work->errors <= 0)
    //             printf(" <%s> io_work errors %d error_code %d \n"
    //                 , __func__
    //                 , (int)io_work->errors
    //                 , (int)io_work->errno_code
    //             );
    //         io_poolChan.send(std::move(io_work));
    //         continue;
    //     }
    //     printf(" <%s> components %p uri /%s/%s ", __func__, (void*)components.get()
    //                 , io_work->components->component_id.c_str()
    //                 , io_work->components->id.c_str()
    //                 );
    //     std::shared_ptr<cfg::register_group_struct> register_groups = io_work.get()->register_groups;
    //     printf(" <%s> register_groups %p ", __func__, (void*)register_groups.get());

    //     int offset = io_work.get()->offset;
    //     printf(" offset %d", (int)offset);

    //     int offnum = io_work.get()->num_registers;
    //     printf(" offnum %d\n", (int)offnum);

    //     int onum = 0;
    //     for( std::shared_ptr<cfg::io_point_struct> individual_register : register_groups->io_point_map) {

    //                 std::any output;
    //                 gcom_decode_any(&io_work.get()->buf16[onum],
    //                                          &io_work.get()->buf8[onum],
    //                                          individual_register.get(),
    //                                          output,
    //                                          myCfg);

    //                 std::string uri = "/"+ io_work->components->component_id + "/" + io_work->components->id;
    //                 std::string id = individual_register->id ; //"/"+ io_work->components->component_id + "/" + io_work->components->id;
    //                 if (pubmap.find(uri)==pubmap.end()) {
    //                     pubmap[uri]= std::map<std::string,std::any>();
    //                 }
    //                 // this may be OK
    //                 // or we could put the raw data in the pubmap
    //                 // or a std::pair io_point_struct, std::any
    //                 pubmap[uri][id]=output;

    //                 // having got that drop it into the pubmap
    //                 //or we can decode it  into a string and have a pubmap of
    //                 // std::map<<std::string,std:map<std::string,std::string>> pubstrmap

    //             onum+=individual_register->size;
    //             //onum = offnum;
    //             //the value to decode starts at io_work.get()->u16_buff[onum]

    //         }
    //         // else
    //         // {
    //         //     std::cout << __func__
    //         //         << "******* ERROR map offset " << offset+onum
    //         //         << " not found decode stopped"
    //         //         << std::endl;
    //         //     break;
    //         // }
    //         //onum = offnum;
    //     //}

    //     //if (debug) {
    //         std::cout
    //         << __func__
    //         << "  regType "<< regTypeToStr( io_work.get()->register_type)
    //         << "  start "<< io_work.get()->offset
    //         << "  number "<< io_work.get()->num_registers
    //         << std::endl;
    //     //}
    // //}

    //     //std::cout << " processing pubgroup " << pub_group.key << "created at :" << pub_group.tNow << std::endl;
    //     io_poolChan.send(std::move(io_work));
    // }

    // for (const auto& uri_pair : pubmap) {
    //     std::cout << "\"" << uri_pair.first << "\":{";

    //     bool firstItem = true;
    //     for (const auto& id_pair : uri_pair.second) {
    //         if (!firstItem) {
    //             std::cout << ", ";
    //         }
    //         std::cout << "\"" << id_pair.first << "\": ";

    //         // Convert std::any back to the original type and print
    //         // This example assumes output is of type std::string
    //         // You'd need to replace std::string with the actual type stored in std::any
    //         const std::any& output = id_pair.second;
    //         try {
    //             u64 value = std::any_cast<u64>(output);
    //             std::cout << "\"" << value << "\""; // or use your own serialization for complex types
    //         } catch (const std::bad_any_cast& e) {
    //             try {
    //                     double  value = std::any_cast<double>(output);
    //                     std::cout << value; // or use your own serialization for complex types
    //                 } catch (const std::bad_any_cast& e) {
    //                     try {
    //                         s64 value = std::any_cast<s64>(output);
    //                         std::cout << value; // or use your own serialization for complex types
    //                     } catch (const std::bad_any_cast& e) {
    //                         try {
    //                             bool value = std::any_cast<bool>(output);
    //                             std::cout << value; // or use your own serialization for complex types
    //                         } catch (const std::bad_any_cast& e) {

    //                             std::cerr << "Bad any_cast: " << e.what() << "\n";
    //                             // Handle error: for example, print an error message, throw an exception, or skip this item
    //                             int status;
    //                             char *demangled_name = abi::__cxa_demangle(output.type().name(), 0, 0, &status);
    //                             if(status == 0) {
    //                                 std::cerr << "Type contained in std::any: " << demangled_name << "\n";
    //                                 free(demangled_name);
    //                             } else {
    //                                 std::cerr << "Could not demangle type name: " << output.type().name() << "\n";
    //                             }
    //                         }
    //                     }
    //                 }
    //         }

    //         firstItem = false;
    //     }
    //     std::cout << "}"<<std::endl;
    // }
}

void discardGroupCallback(struct PubGroup pub_group, struct cfg &myCfg)
{
    std::cout << " >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> dropping pubgroup " << pub_group.key << " created at :" << pub_group.tNow << std::endl;
    for (auto io_work : pub_group.works)
    {
        io_poolChan.send(std::move(io_work));
    }
}

void check_pubgroup_key(std::string key, std::shared_ptr<IO_Work> io_work)
{
    if (pubGroups.find(key) == pubGroups.end())
    {
        // create a new group
        // have to use key here
        pubGroups[key] = {key, io_work->pub_struct, {}, io_work->work_group, io_work->tNow, false, io_work->erase_group};
        printf("<%s> >>>>>>>> creating new pubGroup key  [%s] \n", __func__, key.c_str());
    }
}
// io_work->work_name
// io_work->tNow
// io_work->work_group

// If we dont have a key then its not part of a group

void processRespWork(std::shared_ptr<IO_Work> io_work, struct cfg &myCfg)
{

    std::string key = io_work->work_name; // + std::to_string(io_work->work_id);
    bool debug = myCfg.connection.debug;
    if (debug)
    {
        printf("<%s> >>>>>>>>>>>>>>>> checking  io_work [%s] size [%d] number [%d] \n", __func__, key.c_str(), io_work->work_group, io_work->work_id);
    }
    // crude check for not in a pub group
    // well we could hvaew pub, get remote , get local or set groups
    // if there is no key there is no group
    // this is a single request
    // run locals
    // Holding,
    //     Input,
    //     Coil,
    //     Discrete_Input,
    //     Undefined

    if (io_work->wtype == WorkTypes::Set) //  || (io_work->wtype == WorkTypes::SetMulti))
    {
        if (io_work->local)
        {
            if (debug)
                std::cout << "it was a local set" << std::endl;
            if ((io_work->register_type == cfg::Register_Types::Holding) || (io_work->register_type == cfg::Register_Types::Input))
            {
                local_write_registers(myCfg, io_work, debug);
            }
            else if ((io_work->register_type == cfg::Register_Types::Coil) || (io_work->register_type == cfg::Register_Types::Discrete_Input))
            {
                local_write_bits(myCfg, io_work, debug);
            }
        }
    }
    else if (io_work->wtype == WorkTypes::Get) //  || (io_work->wtype == WorkTypes::SetMulti))
    {
        if (io_work->local)
        {
            if (debug)
                std::cout << "it was a local get" << std::endl;
            if ((io_work->register_type == cfg::Register_Types::Holding) || (io_work->register_type == cfg::Register_Types::Input))
            {
                local_read_registers(myCfg, io_work, debug);
            }
            else if ((io_work->register_type == cfg::Register_Types::Coil) || (io_work->register_type == cfg::Register_Types::Discrete_Input))
            {
                local_read_bits(myCfg, io_work, debug);
            }
        }
    }

    if (key == "")
    {
        std::string reply;
        if (debug)
            printf("<%s> completing  io_work, errors %d  tNow %f reply [%s]\n", __func__, io_work->errors, io_work->tNow, io_work->replyto.c_str());
        // is it a set ?
        if (io_work->wtype == WorkTypes::Set) //  || (io_work->wtype == WorkTypes::SetMulti))
        {
            if (io_work->errors < 0)
                reply = "{\"gcom\":\"Modbus Set\",\"status\":\"Failed\"}";
            else
                reply = "{\"gcom\":\"Modbus Set\",\"status\":\"Success\"}";
            if (io_work->local)
            {
                //std::cout << __func__<<  "it was a local set" << std::endl;
                store_raw_data(io_work, false);
                // local_write_registers
            }
            else
            {
                //std::cout << __func__ <<"   it was a remote set" << std::endl;
                store_raw_data(io_work, false);

            }
        }

        if (io_work->wtype == WorkTypes::Get) // || (io_work->wtype == WorkTypes::GetMulti))
        {
            if (io_work->local)
            {
                std::stringstream string_stream;
                gcom_modbus_decode_debug(io_work, string_stream, myCfg, false, false);
                reply = string_stream.str();
                // std::cout << "it was a local get" << std::endl;
            }
            else
            {
                std::stringstream string_stream;
                gcom_modbus_decode_debug(io_work, string_stream, myCfg, false, false);
                store_raw_data(io_work, true);
                reply = string_stream.str();
                // std::cout << "it was a remote get" << std::endl;
            }
        }

        // we could have a replyto
        if (io_work->replyto != "")
        {
            myCfg.fims_gateway.Send(
                fims::str_view{"set", sizeof("set") - 1},
                fims::str_view{io_work->replyto.data(), io_work->replyto.size()},
                fims::str_view{nullptr, 0},
                fims::str_view{nullptr, 0},
                fims::str_view{reply.data(), reply.size()});
        }
        io_poolChan.send(std::move(io_work));
        return;
    }

    check_pubgroup_key(key, io_work);

    if (pubGroups.find(key) == pubGroups.end())
    {
        // create a new group
        // have to use key here
        //        pubGroups[key] = { key, {io_work},io_work->work_group, io_work->tNow, false, io_work->erase_group };
        if (myCfg.connection.debug)
            printf("<%s> >>>>>>>> pubgroup key not found   [%s] discard io_work \n", __func__, key.c_str());

        io_poolChan.send(std::move(io_work));
        return;
    }
    else
    {
        // if we get a new (later) tNow then discard and start again
        if (myCfg.connection.debug)
            printf("<%s> >>>>>>>> checking pubgroup %s size %d time  %f against incoming   %f \n", __func__, key.c_str(), (int)pubGroups[key].works.size(), pubGroups[key].tNow, io_work->tNow);
        if (io_work->tNow < pubGroups[key].tNow)
        {
            if (myCfg.connection.debug)
                printf("<%s> >>>>>>>> discarding stale incoming io_work; current pubgroup id  %f is later than incoming  id  %f \n", __func__, pubGroups[key].tNow, io_work->tNow);
            io_poolChan.send(std::move(io_work));
            return;
        }

        // did we get a match if so add to the group
        else if (io_work->tNow == pubGroups[key].tNow)
        {
            pubGroups[key].works.push_back(io_work);
            pubGroups[key].pub_struct = io_work->pub_struct;
        }
        else
        {
            // Callback: Discard group
            if (pubGroups[key].done == false)
                discardGroupCallback(pubGroups[key], myCfg);
            pubGroups[key] = {key, io_work->pub_struct, {io_work}, io_work->work_group, io_work->tNow, false, io_work->erase_group};
        }
    }
    if ((int)pubGroups[key].works.size() == (int)pubGroups[key].work_group)
    {
        // Callback: Process group
        pubGroups[key].done = true;
        if (myCfg.connection.debug)
            printf("<%s> >>>>>>>> completing group   %s  tnow   %f \n", __func__, key.c_str(), io_work->tNow);
        processGroupCallback(pubGroups[key], myCfg);
        syncTimeObjectByName(key, 0); // TODO: Need to configure the 0 to be controlled by config
        if (pubGroups[key].erase_group == true)
        {
            pubGroups.erase(key); // Optionally remove the group after processing
        }
    }
    else
    {
        if (myCfg.connection.debug)
            printf("<%s> >>>>>>>> still collecting  group  %s  size %d  out of %d  tnow   %f \n", __func__, key.c_str(), (int)pubGroups[key].works.size(), (int)pubGroups[key].work_group, io_work->tNow);
    }
}

// Response Thread Function
void responseThreadFunc(ThreadControl &control, struct cfg &myCfg)
{
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    while (control.responseThreadRunning)
    {
        if (io_responseChan.receive(io_work, delay))
        {
            io_work->tReceive = get_time_double();

            // Collate batches response_received_work
            processRespWork(io_work, myCfg);

            double tNow = get_time_double();
            {
                std::lock_guard<std::mutex> lock2(io_output_mutex);
                auto tEnd = io_work->tStart;
                auto duration = tNow - tEnd;
                control.num_responses++;
                control.tResponse += duration;

                // if (duration>1.0)
                //     std::cout << "   responseThreadFunc .. Processed by thread "<< io_work->threadId <<" time (Secs) since enqueue  " << (tNow - tEnd)*1.0 << std::endl;
                // else if (duration>0.0001)
                //     std::cout << "   responseThreadFunc .. Processed by thread "<< io_work->threadId <<" time (mSecs) since enqueue  " << (tNow - tEnd)*1000.0 << std::endl;
                // else
                //     std::cout << "   responseThreadFunc .. Processed by thread "<< io_work->threadId <<" time (uSecs) since enqueue  " << (tNow - tEnd)*1000000.0 << std::endl;
            }
            // io_poolChan.send(std::move(io_work));
        }
    }
}

void randomDelay(int minMicroseconds, int maxMicroseconds)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(minMicroseconds, maxMicroseconds);

    int sleepTime = distrib(gen);
    std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
}

void buffer_work(cfg::Register_Types register_type, int device_id, int offset, int num_regs)
{
    std::shared_ptr<IO_Work> io_work;

    io_work = std::make_shared<IO_Work>();

    // Modify io_work data if necessary here
    io_work->tStart = get_time_double();
    io_work->io_points.clear();
    io_work->local = false;

    io_work->device_id = device_id;
    io_work->register_type = register_type;
    io_work->offset = offset;
    // io_work->start_offset = offset;
    io_work->num_registers = num_regs;

    // io_work->set_bufs(num_regs, u16bufs, u8bufs);
    // io_work->wtype = wtype;
    io_work->test_mode = false;
    io_work->num_registers = num_regs;
    io_work->off_by_one = false;

    // TODO use dequw
    io_poolChan.send(std::move(io_work));

    return; // (io_work);
    // io_pollChan.send(std::move(io_work));
    // io_threadChan.send(1);

    // return true;  // You might want to return a status indicating success or failure.
}

std::shared_ptr<IO_Work> make_work(cfg::Register_Types register_type, int device_id,
                                   int offset, int num_regs, uint16_t *u16bufs, uint8_t *u8bufs, WorkTypes wtype)
{

    std::shared_ptr<IO_Work> io_work;
    std::string repto("");

    // if (!io_poolChan.receive(io_work,0)) {  // Assuming receive will return false if no item is available.
    //     std::cout << " create an io_work object "<< std::endl;
    //     io_work = std::make_shared<IO_Work>();
    // }
    if (!io_poolChan.peekpop(io_work))
    { // Assuming receive will return false if no item is available.
        // std::cout << " create an io_work object "<< std::endl;
        io_work = std::make_shared<IO_Work>();
    }
    io_work->io_points.clear();
    // Modify io_work data if necessary here
    io_work->tStart = get_time_double();
    io_work->device_id = device_id;
    io_work->register_type = register_type;
    io_work->offset = offset;
    // io_work->start_offset = offset;
    io_work->num_registers = num_regs;

    io_work->set_bufs(num_regs, u16bufs, u8bufs);
    io_work->wtype = wtype;
    io_work->test_mode = false;

    io_work->local = false;
    io_work->replyto = repto;
    io_work->work_name = repto;
    io_work->off_by_one = false;

    // io_work->work_name = rep;
    // io_work->local = false;
    // io_work->replyto = rep;

    return (io_work);
    // io_pollChan.send(std::move(io_work));
    // io_threadChan.send(1);

    // return true;  // You might want to return a status indicating success or failure.
}
// std::shared_ptr<IO_Work> make_work(cfg::Register_Types register_type, int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype ) {
// bool pollWork (std::shared_ptr<IO_Work> io_work) {
// bool setWork (std::shared_ptr<IO_Work> io_work) {

bool pollWork(std::shared_ptr<IO_Work> io_work)
{
    // std::cout << "Sending a poll item "<<std::endl;
    io_pollChan.send(std::move(io_work));
    io_threadChan.send(1);

    return true; // You might want to return a status indicating success or failure.
}

bool setWork(std::shared_ptr<IO_Work> io_work)
{
    io_setChan.send(std::move(io_work));
    io_threadChan.send(1);

    return true; // You might want to return a status indicating success or failure.
}

bool queue_work(cfg::Register_Types register_type, int device_id, int offset, int num_regs, uint16_t *u16bufs, uint8_t *u8bufs, WorkTypes wtype)
{

    std::shared_ptr<IO_Work> io_work;

    if (!io_poolChan.receive(io_work, 0))
    { // Assuming receive will return false if no item is available.
        io_work = std::make_shared<IO_Work>();
    }

    // Modify io_work data if necessary here
    io_work->io_points.clear();
    io_work->local = false;

    io_work->tStart = get_time_double();
    io_work->device_id = device_id;
    io_work->register_type = register_type;
    io_work->offset = offset;
    io_work->num_registers = num_regs;
    io_work->off_by_one = false;

    io_work->set_bufs(num_regs, u16bufs, u8bufs);
    io_work->wtype = wtype;
    io_work->test_mode = false;

    io_pollChan.send(std::move(io_work));
    io_threadChan.send(1);

    return true; // You might want to return a status indicating success or failure.
}

bool startRespThread(struct cfg &myCfg)
{
    // double tNow = get_time_double();

    // std::cout << " response thread starting  at "<< tNow << std::endl;
    threadControl.responseThread = std::thread(responseThreadFunc, std::ref(threadControl), std::ref(myCfg));
    // tNow = get_time_double();
    // std::cout << " response thread started  at "<< tNow << std::endl;
    //  {
    //      std::lock_guard<std::mutex> lock2(io_output_mutex);
    //      double tNow = get_time_double();
    //      FPS_INFO_LOG("response thread  stating at %f ", tNow);
    //      //std::cout << " response thread running  at "<< tNow << std::endl;
    //  }
    return true;
}

bool stopRespThread(struct cfg &myCfg)
{
    threadControl.responseThreadRunning = false;
    threadControl.responseThread.join();
    return true;
}

std::shared_ptr<IO_Thread> make_IO_Thread(int idx, const char *ip, int port, int connection_timeout, struct cfg &myCfg)
{
    // IO_Thread io_thread;
    std::shared_ptr<IO_Thread> io_thread = std::make_shared<IO_Thread>();
    // Actually start the threads here
    io_thread->tid = idx;
    io_thread->jobs = 0;
    io_thread->fails = 0;
    io_thread->ip = "";
    io_thread->myCfg = &myCfg;

    if (ip)
        io_thread->ip = ip;
    io_thread->port = port;
    io_thread->connection_timeout = connection_timeout;
    return (io_thread);
}

bool StartThreads(int num_threads, const char *ip, int port, int connection_timeout, struct cfg &myCfg)
{
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        Logging::Init("gcom_iothreads", (const int)0, (const char **)nullptr);
        FPS_INFO_LOG("IO Threads : starting.");
    }

    // threadControl.io_pollChan     = &io_pollChan;
    // threadControl.io_responseChan = &io_responseChan;
    // threadControl.io_poolChan     = &io_poolChan;

    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        double tNow = get_time_double();
        FPS_INFO_LOG("io_threads  stating  at %f ", tNow);
        // std::cout << " test_io_threads running  at "<< tNow << std::endl;
    }

    // Start the response thread
    startRespThread(myCfg);

    // int num_threads = 4;
    for (int i = 0; i < num_threads; ++i)
    {
        // IO_Thread io_thread;
        std::shared_ptr<IO_Thread> io_thread = make_IO_Thread(i, ip, port, connection_timeout, myCfg);
        // std::shared_ptr<IO_Thread> io_thread = std::make_shared<IO_Thread>();
        // // Actually start the threads here
        // io_thread->tid = i;
        // io_thread->jobs = 0;
        // io_thread->fails = 0;
        // io_thread->ip = "";

        // if(ip)io_thread->ip = ip;
        // io_thread->port = port;
        // io_thread->connection_timeout = connection_timeout;

        {
            std::lock_guard<std::mutex> lock2(io_output_mutex);
            double tNow = get_time_double();
            FPS_INFO_LOG("thread_id %d stating at %f ", i, tNow);
        }
        io_thread->thread = std::thread(ioThreadFunc, std::ref(threadControl), std::ref(myCfg), io_thread);

        //        io_thread->thread = std::thread (ioThreadFunc, std::ref(threadControl), io_thread.get());
        threadControl.ioThreadPool.push_back(io_thread); // std::thread (ioThreadFunc, std::ref(systemControl), io_thread));
    }
    // std::thread responseThread(responseThreadFunc, std::ref(*this));
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        double tNow = get_time_double();
        FPS_INFO_LOG("all io_threads running at %f ", tNow);
        FPS_LOG_IT("startup");
    }
    return true;
}

bool StartThreads(struct cfg &myCfg, bool debug)
{
    return StartThreads(myCfg.connection.max_num_connections, myCfg.connection.ip_address.c_str(), myCfg.connection.port, myCfg.connection.connection_timeout, myCfg);
}

bool StopThreads(struct cfg &myCfg, bool debug)
{
    threadControl.stopThreads();
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("all io_threads stopped \n");
    }

    threadControl.responseThread.join();
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("resp thread stopped \n");
    }
    return true;
}

int test_io_threads(struct cfg &myCfg)
{

    int num_threads = 4;
    StartThreads(num_threads, nullptr, 0, 2, myCfg);

    //     {
    //         std::lock_guard<std::mutex> lock2(io_output_mutex);
    //         Logging::Init("gcom_iothreads", (const int)0, (const char **)nullptr);
    //         FPS_INFO_LOG("IO Threads : starting.");
    //         FPS_LOG_IT("startup");
    //     }
    //     threadControl.io_pollChan = &io_pollChan;
    //     threadControl.io_pollChan = &io_responseChan;
    //     threadControl.io_pollChan = &io_poolChan;

    //     // Start the io_thread
    //     // std::thread ioThread(ioThreadFunc, std::ref(systemControl));

    //     {
    //         std::lock_guard<std::mutex> lock2(io_output_mutex);
    //         double tNow = get_time_double();
    //         std::cout << " test_io_threads running  at "<< tNow << std::endl;
    //     }

    //     // Start the response thread
    //     std::thread responseThread(responseThreadFunc, std::ref(threadControl));
    //     {
    //         std::lock_guard<std::mutex> lock2(io_output_mutex);
    //         double tNow = get_time_double();
    //         std::cout << " response hread running  at "<< tNow << std::endl;
    //     }

    //     int num_threads = 4;
    //     for (int i = 0 ; i < num_threads; ++i)
    //     {
    //         //IO_Thread io_thread;
    //         std::shared_ptr<IO_Thread> io_thread = std::make_shared<IO_Thread>();
    //         // Actually start the threads here
    //         io_thread->tid = i;
    //         io_thread->jobs = 0;
    //         {
    //             std::lock_guard<std::mutex> lock2(io_output_mutex);
    //             FPS_INFO_LOG("thread_id  %d stating ", i);
    //         }
    //         io_thread->thread = std::thread(ioThreadFunc, std::ref(threadControl), io_thread);

    // //        io_thread->thread = std::thread (ioThreadFunc, std::ref(threadControl), io_thread.get());
    //         threadControl.ioThreadPool.push_back(io_thread); //std::thread (ioThreadFunc, std::ref(systemControl), io_thread));
    //     }
    //         // std::thread responseThread(responseThreadFunc, std::ref(*this));
    //     {
    //         std::lock_guard<std::mutex> lock2(io_output_mutex);
    //         double tNow = get_time_double();
    //         std::cout << " io_threads running  at "<< tNow << std::endl;
    //     }

    // Simulate the sending of work
    for (int i = 0; i < 10; ++i)
    {
        queue_work(cfg::Register_Types::Holding, i, 0, 5, nullptr, nullptr, WorkTypes::Noop);
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Sleep for a bit before sending the next
    }
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        double tNow = get_time_double();
        std::cout << " jobs queued  at " << tNow << std::endl;
    }

    // Signal the io_thread to stop after some time
    std::this_thread::sleep_for(std::chrono::seconds(2));

    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        double tNow = get_time_double();
        std::cout << " done sleeping " << tNow << std::endl;
    }

    // io_threadChan.send(0);

    threadControl.stopThreads();
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("all threads stopping ");
    }

    threadControl.responseThread.join();
    // Signal the io_thread to stop after some time
    std::this_thread::sleep_for(std::chrono::seconds(2));
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        double tNow = get_time_double();
        std::cout << " final sleep " << tNow << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("IO Threads : stopping.");
        FPS_LOG_IT("shutdown");
    }
    return 0;
}

#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>

int wait_socket_ready(modbus_t *ctx, int timeout_sec)
{
    int socket_fd = modbus_get_socket(ctx);
    if (socket_fd == -1)
    {
        return -1; // Invalid socket
    }

    fd_set write_set;
    struct timeval timeout;

    FD_ZERO(&write_set);
    FD_SET(socket_fd, &write_set);
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    int result = select(socket_fd + 1, NULL, &write_set, NULL, &timeout);
    if (result > 0)
    {
        // // Check if connection was successful
        // int optval;
        // socklen_t optlen = sizeof(optval);
        // if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == 0) {
        //     if (optval == 0) {
        //         return 0; // Connection successful
        //     }
        // }
        return 0;
    }

    return -1; // Connection failed or timed out
}

double SetupModbusForThread(struct cfg &myCfg, std::shared_ptr<IO_Thread> io_thread, bool debug)
{
    uint32_t to_sec = io_thread->connection_timeout; // 2-10 seconds timeout
    uint32_t to_usec = 0;                            // 0 microsecond
    double tNow = get_time_double();
    io_thread->ctx = nullptr;
    modbus_t *ctx = nullptr; // modbus_new_tcp(io_thread->ip.c_str(), io_thread->port);
    if (myCfg.connection.is_RTU)
    {
        ctx = modbus_new_rtu(
            myCfg.connection.serial_device.c_str(),
            myCfg.connection.baud_rate,
            myCfg.connection.parity,
            myCfg.connection.data_bits,
            myCfg.connection.stop_bits);
    }
    else
    {
        ctx = modbus_new_tcp(io_thread->ip.c_str(), io_thread->port);
    }

    std::cout << "{ \"status\":\"note\" , \"message\":\" trying to create modbus ctx to " << io_thread->ip << "  port " << io_thread->port << "\"}" << std::endl;

    if (!ctx)
    {
        char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] failed to create modbus context to [%s] on port [%d].", myCfg.connection.name.c_str(), io_thread->ip.c_str(), io_thread->port);
        FPS_ERROR_LOG("%s", message);
        emit_event(&myCfg.fims_gateway, "Modbus Client", message, 1);
        return (get_time_double() - tNow) * -1;
    }

    auto mberr = modbus_connect(ctx);
    if (mberr != 0)
    {
        char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] failed to connect to [%s] on port [%d]. Modbus error [%d]", myCfg.connection.name.c_str(), io_thread->ip.c_str(), io_thread->port, mberr);
        FPS_ERROR_LOG("%s", message);
        emit_event(&myCfg.fims_gateway, "Modbus Client", message, 1);
        modbus_free(ctx);
        return (get_time_double() - tNow) * -1;
    }
    wait_socket_ready(ctx, 1);

    if (modbus_set_response_timeout(ctx, to_sec, to_usec) == -1)
    {
        char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] failed to create set response timeout to [%s] on port [%d].", myCfg.connection.name.c_str(), io_thread->ip.c_str(), io_thread->port);
        FPS_ERROR_LOG("%s", message);
        emit_event(&myCfg.fims_gateway, "Modbus Client", message, 1);        modbus_free(ctx);
        return (get_time_double() - tNow) * -1;
    }

    modbus_set_error_recovery(ctx, (modbus_error_recovery_mode)MODBUS_ERROR_RECOVERY_PROTOCOL);

    io_thread->ctx = ctx;
    io_thread->connect_time = get_time_double() - tNow;
    char message[1024];
    snprintf(message, 1024, "Modbus Client [%s] successfully connected to [%s] on port [%d].", myCfg.connection.name.c_str(), io_thread->ip.c_str(), io_thread->port);
    FPS_INFO_LOG("%s", message);
    emit_event(&myCfg.fims_gateway, "Modbus Client", message, 1);
    return (io_thread->connect_time);
}

bool SetUPModbusCtx(int num_threads, const char *ip, int port, int connection_timeout)
{
    int i;
    bool debug = true;
    uint32_t to_sec = connection_timeout; // 5 seconds timeout
    uint32_t to_usec = 0;                 // 0 microsecond

    // create thread
    for (i = 0; i < num_threads; i++)
    {

        //        IO_Thread* io_thread = new IO_Thread;
        threadControl.ioThreadPool[i]->ctx = nullptr;
        threadControl.ioThreadPool[i]->ip = std::string(ip);
        threadControl.ioThreadPool[i]->port = port;

        modbus_t *ctx = modbus_new_tcp(threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port);
        if (!ctx)
        {
            char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] failed to create modbus context to [%s] on port [%d].",  threadControl.ioThreadPool[i]->myCfg->connection.name.c_str(), threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port);
        FPS_ERROR_LOG("%s", message);
            return false;
        }
        void *vctx = (void *)ctx;
        if (debug)
            std::cout << " Thread_id" << i << " Create modbus ctx to " << ip << "  port " << port << " ctx " << std::hex << vctx << std::endl;

        auto mberr = modbus_connect(ctx);
        char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] successfully connected to [%s] on port [%d].",  threadControl.ioThreadPool[i]->myCfg->connection.name.c_str(), threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port);
        FPS_INFO_LOG("%s", message);
        wait_socket_ready(ctx, 1);

        if (mberr != 0)
        {
            char message[1024];
            snprintf(message, 1024, "Modbus Client [%s] failed to connect to [%s] on port [%d]. Modbus error [%d]", threadControl.ioThreadPool[i]->myCfg->connection.name.c_str(), threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port, mberr);
            FPS_ERROR_LOG("%s", message);
            modbus_free(ctx);
            return false;
        }

        if (modbus_set_response_timeout(ctx, to_sec, to_usec) == -1)
        {
            char message[1024];
            snprintf(message, 1024, "Modbus Client [%s] failed to create set response timeout to [%s] on port [%d].", threadControl.ioThreadPool[i]->myCfg->connection.name.c_str(), threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port);
            FPS_ERROR_LOG("%s", message);
            modbus_free(ctx);
            return false;
        }
        modbus_set_error_recovery(ctx, (modbus_error_recovery_mode)MODBUS_ERROR_RECOVERY_PROTOCOL);
        // modbus_set_error_recovery(ctx,  (modbus_error_recovery_mode)(MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));

        // if(debug)std::cout<< " set response timeout  "<<ip<< "  port "<< port <<std::endl;
        threadControl.ioThreadPool[i]->ctx = std::move(ctx);
    }
    return true;
}

bool CloseModbusForThread(std::shared_ptr<IO_Thread> io_thread, bool debug)
{
    auto ctx = io_thread->ctx;
    io_thread->ctx = nullptr;
    modbus_close(ctx);
    char message[1024];
    snprintf(message, 1024, "Modbus Client [%s] disconnected from [%s] on port [%d].", io_thread->myCfg->connection.name.c_str(), io_thread->ip.c_str(), io_thread->port);
    FPS_INFO_LOG("%s", message);
    emit_event(&io_thread->myCfg->fims_gateway, "Modbus Client", message, 1);
    modbus_free(ctx);
    return true;
}

bool CloseModbusCtx(int num_threads, const char *ip, int port)
{
    for (int i = 0; i < num_threads; i++)
    {
        auto ctx = threadControl.ioThreadPool[i]->ctx;
        threadControl.ioThreadPool[i]->ctx = nullptr;
        modbus_close(ctx);
        char message[1024];
        snprintf(message, 1024, "Modbus Client [%s] disconnected from [%s] on port [%d].", threadControl.ioThreadPool[i]->myCfg->connection.name.c_str(), threadControl.ioThreadPool[i]->ip.c_str(), threadControl.ioThreadPool[i]->port);
        FPS_INFO_LOG("%s", message);
        emit_event(&threadControl.ioThreadPool[i]->myCfg->fims_gateway, "Modbus Client", message, 1);
        modbus_free(ctx);
    }
    return true;
}
//

cfg::Register_Types strToRegType(std::string &register_type_str)
{
    // std::cout << " Register Type ["<< register_type <<"]"<<std::endl;
    auto register_type = cfg::Register_Types::Holding;
    if (register_type_str == "Coil")
        register_type = cfg::Register_Types::Coil;
    else if (register_type_str == "Input")
        register_type = cfg::Register_Types::Input;
    else if (register_type_str == "Input Registers")
        register_type = cfg::Register_Types::Input;
    else if (register_type_str == "Discrete_Input")
        register_type = cfg::Register_Types::Discrete_Input;
    else if (register_type_str == "Discrete Inputs")
        register_type = cfg::Register_Types::Discrete_Input;
    else if (register_type_str == "Discrete")
        register_type = cfg::Register_Types::Discrete_Input;
    return register_type;
}

std::string regTypeToStr(cfg::Register_Types &register_type)
{
    if (register_type == cfg::Register_Types::Holding)
        return "Holding";
    else if (register_type == cfg::Register_Types::Coil)
        return "Coil";
    else if (register_type == cfg::Register_Types::Input)
        return "Input";
    else if (register_type == cfg::Register_Types::Discrete_Input)
        return "Discrete Input";
    return "Unknown";
}

WorkTypes strToWorkType(std::string roper, bool debug = false)
{
    auto work_type = WorkTypes::Noop; // no op
    if (roper == "set")
    {
        work_type = WorkTypes::Set;
        if (debug)
            std::cout << " pushing set_work  " << std::endl;
    }
    else if (roper == "set_multi")
    {
        work_type = WorkTypes::SetMulti;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "get")
    {
        work_type = WorkTypes::Get;
        if (debug)
            std::cout << "  running with Get  " << roper << std::endl;
    }
    else if (roper == "get_multi")
    {
        work_type = WorkTypes::GetMulti;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "poll")
    {
        work_type = WorkTypes::Get;
        if (debug)
            std::cout << "  running with Get  " << roper << std::endl;
    }
    else if (roper == "poll_multi")
    {
        work_type = WorkTypes::GetMulti;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "bit_set")
    {
        work_type = WorkTypes::BitSet;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "bit_set_multi")
    {
        work_type = WorkTypes::BitSetMulti;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "bit_get")
    {
        work_type = WorkTypes::BitGet;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else if (roper == "bit_get_multi")
    {
        work_type = WorkTypes::BitGetMulti;
        if (debug)
            std::cout << " pushing  " << roper << std::endl;
    }
    else
    {
        std::cout << " operation " << roper << " not yet supported just use set or poll" << std::endl;
    }
    return work_type;
}

// this is the old code
void test_io_point_single(const char *ip, int port, int connection_timeout, const char *oper, int device_id, const char *regtype, int offset, int num_regs,
                          int value, int num_threads, struct cfg &myCfg, bool debug)
{
    // auto thread_debug = debug;
    // int num_threads = 4;
    int num_points = 1;

    // TODO fix these
    std::string register_type_str(regtype);
    std::string roper(oper);
    // int num_regs = 1;
    auto register_type = strToRegType(register_type_str);
    auto cfgreg_type = myCfg.typeFromStr(register_type_str);
    std::cout << " cfg Register Type" << myCfg.typeToStr(cfgreg_type) << std::endl;
    auto work_type = strToWorkType(oper);

    uint16_t u16bufs[130]; // MAX_MODBUS_NUM_REGS
    uint8_t u8bufs[130];
    for (int i = 0; i < num_regs; ++i)
    {
        u16bufs[i] = value;
        u8bufs[i] = value;
    }

    // Main_Thread  main_work;
    // std::vector<IO_Thread*> threadvec;

    // uint32_t to_sec = 2; // 5 seconds timeout
    // uint32_t to_usec = 0; // 0 microsecond
    // auto start = std::chrono::high_resolution_clock::now();

    StartThreads(num_threads, ip, port, connection_timeout, myCfg);
    // std::cout << " Threads Started"<< std::endl;
    // SetUPModbusCtx(num_threads, ip , port, connection_timeout);

    std::this_thread::sleep_for(100ms);

    for (int j = 0; j < 10; ++j)
    {
        for (int i = 0; i < num_points; ++i)
        {
            double tNow = get_time_double();
            u16bufs[0] = i;
            // std::shared_ptr<IO_Work> make_work(cfg::Register_Types register_type, int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype ) {

            auto io_work = make_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
            io_work->test_mode = true;
            io_work->work_group = num_points;
            io_work->work_name = std::string("test_io_point_single");
            io_work->tNow = tNow;

            if (j == 4)
                io_work->test_it = true;
            else
                io_work->test_it = false;

            pollWork(io_work);
            std::cout << " Test sleeping for 1 second" << std::endl;
            std::this_thread::sleep_for(1000ms);
            // std::cout << " Test io_work errno_code  "<< (int)io_work->errno_code<< std::endl;
            if (io_work->errno_code == BAD_DATA_ADDRESS)
            {
                std::cout << " Test io_work errno_code  " << (int)io_work->errno_code << " Skipping Test" << std::endl;
                j = 10;
            }
        }
        // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
        // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
        // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
        //  the response thread should have fired
    }
    std::cout << " Now sleeping for 2 seconds" << std::endl;
    std::this_thread::sleep_for(2000ms);

    // close context
    // TODO may need locks here
    // CloseModbusCtx(num_threads, ip , port);

    // io_threadChan.send(0);
    for (int i = 0; i < num_threads; ++i)
    {
        io_threadChan.send(0);
    }

    threadControl.stopThreads();
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("all threads stopping ");
    }

    threadControl.responseThread.join();
    // Signal the io_thread to stop after some time
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    // {
    //     std::lock_guard<std::mutex> lock2(io_output_mutex);
    //     double tNow = get_time_double();
    //     std::cout << " final sleep "<< tNow << std::endl;
    // }

    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("IO Threads : stopping.");
        FPS_LOG_IT("shutdown");
    }

    // for (auto threadp : threadvec)
    // {

    //     if(debug)std::cout << " waiting for thread " << threadp->id << " To close "<< std::endl;
    //     threadp->thread_future.wait();
    //     if(debug) std::cout << " thread " << threadp->id << " closing "<< std::endl;
    //     modbus_close(threadp->ctx);
    //     modbus_free(threadp->ctx);
    //     delete threadp;
    // }

    // //io_thread.thread_future.wait();

    // if(debug) std::cout << " all done " << std::endl;
    // delete set_work;

    // send some requests
    // stop the thread
    // done
}

void test_io_point_multi(const char *ip, int port, int connection_timeout, const char *oper, int device_id, const char *regtype, int offset, int value, int num_threads, struct cfg &myCfg, bool debug)
{
    // auto thread_debug = debug;
    // int num_threads = 4;
    int num_points = 10;

    // TODO fix these
    std::string register_type_str(regtype);
    std::string roper(oper);
    int num_regs = 1;
    auto register_type = strToRegType(register_type_str);

    auto work_type = strToWorkType(oper);

    uint16_t u16bufs[130]; // MAX_MODBUS_NUM_REGS
    uint8_t u8bufs[130];
    for (int i = 0; i < num_regs; ++i)
    {
        u16bufs[i] = value;
        u8bufs[i] = value;
    }

    // Main_Thread  main_work;
    // std::vector<IO_Thread*> threadvec;

    // uint32_t to_sec = 2; // 5 seconds timeout
    // uint32_t to_usec = 0; // 0 microsecond
    // auto start = std::chrono::high_resolution_clock::now();

    StartThreads(num_threads, ip, port, connection_timeout, myCfg);
    // SetUPModbusCtx(num_threads, ip , port, connection_timeout);

    std::this_thread::sleep_for(100ms);
    for (int i = 0; i < num_points; ++i)
    {
        u16bufs[0] = i;
        queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
    }
    // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
    // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
    // queue_work(register_type, device_id, offset, num_regs, u16bufs, u8bufs, work_type);
    //  the response thread should have fired

    std::this_thread::sleep_for(10000ms);

    // close context
    // TODO may need locks here
    // CloseModbusCtx(num_threads, ip , port);

    // io_threadChan.send(0);
    for (int i = 0; i < num_threads; ++i)
    {
        io_threadChan.send(0);
    }

    threadControl.stopThreads();
    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("all threads stopping ");
    }

    threadControl.responseThread.join();
    // Signal the io_thread to stop after some time
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    // {
    //     std::lock_guard<std::mutex> lock2(io_output_mutex);
    //     double tNow = get_time_double();
    //     std::cout << " final sleep "<< tNow << std::endl;
    // }

    {
        std::lock_guard<std::mutex> lock2(io_output_mutex);
        FPS_INFO_LOG("IO Threads : stopping.");
        FPS_LOG_IT("shutdown");
    }

    // for (auto threadp : threadvec)
    // {

    //     if(debug)std::cout << " waiting for thread " << threadp->id << " To close "<< std::endl;
    //     threadp->thread_future.wait();
    //     if(debug) std::cout << " thread " << threadp->id << " closing "<< std::endl;
    //     modbus_close(threadp->ctx);
    //     modbus_free(threadp->ctx);
    //     delete threadp;
    // }

    // //io_thread.thread_future.wait();

    // if(debug) std::cout << " all done " << std::endl;
    // delete set_work;

    // send some requests
    // stop the thread
    // done
}