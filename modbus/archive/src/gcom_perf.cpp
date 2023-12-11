#include <iostream>
#include <chrono>
#include <thread>
#include <pthread.h>

#include "gcom_perf.h"

//std::chrono::system_clock::time_point baseTimeMs;

double get_time_double();

pthread_t recordThread;
bool perf_running = false;
std::atomic<bool> terminate_thread(false);
std::condition_variable termination_cv; // Condition variable for thread termination



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

void Stats::set_label(std::string _label) {
    label = std::move(_label);
}
void Stats::start() {
    start_time = get_time_double();
    started = true;
}

void Stats::snap() {
    if (started) {
        double tNow = get_time_double();
        record_duration(tNow - start_time);
        started = false;
    }
}

void Stats::show() {
    if (count > 0)
    {
        std::cout << "Label: " << label
                  << "\tMax(mS): " << max_duration* 1000.0
                  << "\tMin(mS): " << min_duration *1000.0
                  << "\tAvg(mS): " << (total_duration /count) * 1000.0
                  << "\tCount: " << count 
                  << std::endl;
    }
}

void Stats::clear() {
    count = 0;
    total_duration = 0.0;
    max_duration = 0.0;
    min_duration = std::numeric_limits<double>::max();
}

void Stats::record_duration(double duration) {
    count++;
    total_duration += duration;
    if (duration > max_duration) max_duration = duration;
    if (duration < min_duration) min_duration = duration;
}

RecordQueue recordQueue;
class Stats;
std::map<std::string, Stats> labeled_stats;

Perf::Perf(const std::string& label_) : label(label_) {
    start_time = get_time_double();
    closed =  false;
    run_perf();
}

Perf::~Perf() {
    if (!closed)
    {
        auto end_time = get_time_double();
        //std::cout << label << " duration :"<<(end-start)<<std::endl;
        recordQueue.push(label, end_time-start_time);
        closed = true;
    }
}

void Perf::start() {
    start_time = get_time_double();
    closed = false;
}

void Perf::snap () {
    auto end_time = get_time_double();
    //std::cout << label << " duration :"<<(end-start)<<std::endl;
    recordQueue.push(label, end_time-start_time);
    closed = true;
}


void RecordQueue::push(const std::string& label, double duration) {
    std::lock_guard<std::mutex> lock(mtx);

    batch.push_back({label, duration});

    if (batch.size() >= BATCH_SIZE) {
        std::cout << " Pushing batch" << std::endl;
        batches.push(std::move(batch));
        batch.clear();
        cv.notify_one();
    }
}

bool RecordQueue::pop(std::vector<Record>& records_batch) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return !batches.empty(); });

    records_batch = std::move(batches.front());
    batches.pop();

    return true;
}
    
bool RecordQueue::try_pop(std::vector<Record>& records_batch) {
    {
        std::lock_guard<std::mutex> lock(mtx);
    
        if (batches.empty()) {
            return false;
        }
    }
    return pop(records_batch);
}

// Force push remaining records, for graceful termination
void RecordQueue::flush() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!batch.empty()) {
        batches.push(std::move(batch));
        cv.notify_one();
    }
}

void* record_thread_func(void* arg) {
    std::vector<RecordQueue::Record> records_batch;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(recordQueue.mtx);
            std::cout << "Waiting for a batch or termination signal..." << std::endl;
            recordQueue.cv.wait(lock, [&]() { 
                bool shouldWakeup = !recordQueue.batches.empty() || terminate_thread.load();
                if (shouldWakeup) {
                    std::cout << "Waking up due to batch or termination signal..." << std::endl;
                }
                return shouldWakeup;
            });
            
            if (!recordQueue.batches.empty()) {
                records_batch = std::move(recordQueue.batches.front());
                recordQueue.batches.pop();
            }
        }

        // Process the batch outside the lock
        for (const auto& record : records_batch) {
            std::cout << " recording " << record.label << " duration " << record.duration << std::endl;
            labeled_stats[record.label].record_duration(record.duration);
        }
        records_batch.clear();

        // Termination condition: if termination signal is received and no more batches to process.
        if (terminate_thread.load() && recordQueue.batches.empty()) {
            break;
        }
    }
    return nullptr;
}


void display_statistics() {
    std::cout << "Performance Statistics:\n";
    for (const auto& pair : labeled_stats) {
        const auto& label = pair.first;
        const auto& stats = pair.second;
        std::cout << "Label: " << label
                  << "\tMax: " << stats.max_duration* 1000.0
                  << "\tMin: " << stats.min_duration *1000.0
                  << "\tAvg: " << (stats.total_duration / stats.count) * 1000.0
                  << "\tCount: " << stats.count 
                  << std::endl;
    }
}

Stats* getStats(const std::string& label) {
    if (labeled_stats.find(label) != labeled_stats.end()) {
        return &labeled_stats[label];
    }
    return nullptr;
}

void runOneSecond() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void stop_perf() {
    terminate_thread.store(true);
    recordQueue.cv.notify_all();  // Notify the recording thread to terminate
    pthread_join(recordThread, nullptr);
    perf_running = false;
}


int test_perf() {
    //baseTimeMs = std::chrono::system_clock::now();

    Stats tStats;
    tStats.set_label("test_stats");
    for ( int i = 1; i< 100; ++i) {
        tStats.start();
        tStats.snap();
    }

    for (int i = 0; i < 10; i++) {
        {
            Perf y("test_one_sec_two");
            Perf x("test_one_sec_one");
            auto p = std::make_unique<Perf>("test_one_sec");
            runOneSecond();
        }
    }

    for (int i = 0; i < 100; i++) {
        Perf p("test_one_msec");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    recordQueue.flush();
    display_statistics();

    {
        Perf x("final_call");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stop_perf();

    {
        Perf x("another_final_call");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
   recordQueue.flush();
 
    stop_perf();
    display_statistics();
    tStats.show();

    return 0;
}
