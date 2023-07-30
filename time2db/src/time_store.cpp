//src/time_store.cpp:

#include "time_store.h"
#include <iostream>

TimeStore::TimeStore() : stop_thread_(false) {
    thread_ = std::thread(&TimeStore::run, this);
}

TimeStore::~TimeStore() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_thread_ = true;
        cv_.notify_all();
    }
    thread_.join();
}

void TimeStore::storeSample(const std::string& name, unsigned long long time) {
    std::lock_guard<std::mutex> lock(mtx_);
    samples_.emplace_back(name, SampleData{ time, time, static_cast<double>(time), 1 });
    cv_.notify_all();
}

void TimeStore::run() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !samples_.empty() || stop_thread_; });

        if (stop_thread_) {
            break;
        }

        for (const auto& sample : samples_) {
            // In a real-world scenario, you would store the data in a database or file.
            // Here, we just print the data to simulate the storage process.
            const SampleData& data = sample.second;
            std::cout << "Storing: " << sample.first << " -> Max: " << data.max_time
                      << ", Min: " << data.min_time << ", Avg: " << data.avg_time << std::endl;
        }

        samples_.clear();
    }
}