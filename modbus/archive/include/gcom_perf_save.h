#pragma once

#include <iostream>
#include <pthread.h>
#include <chrono>
#include <string>
#include <map>
#include <limits>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <unistd.h>
#include <atomic>

std::atomic<bool> terminate_thread(false);

std::condition_variable termination_cv; // Condition variable for thread termination
class Stats;

std::map<std::string, Stats> labeled_stats;


double get_time_double();

const size_t BATCH_SIZE = 1;  // Change according to your needs

void* record_thread_func(void* arg);

pthread_t recordThread;
bool perf_running = false;

void run_perf()
{
    if (!perf_running)
    {
        std::cout << "starting record thread" <<std::endl;
        terminate_thread.store(false);

        pthread_create(&recordThread, nullptr, record_thread_func, nullptr);
        perf_running = true;
    }
}

class RecordQueue {
public:
    struct Record {
        std::string label;
        double duration;
    };

    std::vector<Record> batch;
    std::queue<std::vector<Record>> batches;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const std::string& label, double duration) {
        std::lock_guard<std::mutex> lock(mtx);

        batch.push_back({label, duration});

        if (batch.size() >= BATCH_SIZE) {
            batches.push(std::move(batch));
            batch.clear();
            cv.notify_one();
        }
    }

    bool pop(std::vector<Record>& records_batch) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !batches.empty(); });

        records_batch = std::move(batches.front());
        batches.pop();

        return true;
    }
    
    bool try_pop(std::vector<Record>& records_batch) {
        {
            std::lock_guard<std::mutex> lock(mtx);
        
            if (batches.empty()) {
                return false;
            }
        }
        return pop(records_batch);

    }

    // Force push remaining records, for graceful termination
    void flush() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!batch.empty()) {
            batches.push(std::move(batch));
            cv.notify_one();
        }
    }
};

RecordQueue recordQueue;

class Perf {
    std::string label;
    double start_time;
    int ping_count;    
public:
    Perf(const std::string& label_) : label(label_) {
        start_time = get_time_double();
        ping_count = 0;
        run_perf();
    };

    ~Perf() {
        auto end_time = get_time_double();
        //std::cout << label << " duration :"<<(end-start)<<std::endl;
        recordQueue.push(label, end_time-start_time);
    };
    void start() {
        start_time = get_time_double();
    }
    void snap () {
        auto end_time = get_time_double();
        //std::cout << label << " duration :"<<(end-start)<<std::endl;
        recordQueue.push(label, end_time-start_time);
    };

    void ping()
    {
        if (ping_count< 10 )
            ping_count++;
    }

};

struct Stats {
    double max_duration = 0;
    double min_duration = std::numeric_limits<double>::max();
    double total_duration = 0;
    int count = 0;

    void record_duration(double duration) {
        count++;
        total_duration += duration;
        if (duration > max_duration) max_duration = duration;
        if (duration < min_duration) min_duration = duration;
    }
};

