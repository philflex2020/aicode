//RecordQueue recordQueue;
#include <condition_variable>
#include <chrono>

#include "gcom_perf.h"

std::chrono::system_clock::time_point baseTimeMs;
// = std::chrono::system_clock::now();

double get_time_double();



void* record_thread_func(void* arg) {
    std::vector<RecordQueue::Record> records_batch;
    
    
    while (true) {

        if (!recordQueue.try_pop(records_batch)) {
            // Instead of busy-waiting, we'll sleep until signaled by the condition variable
            std::unique_lock<std::mutex> lock(recordQueue.mtx);
            termination_cv.wait(lock, [&]() { return terminate_thread.load() || !recordQueue.batches.empty(); });


            // After wake-up, check for termination condition
            if (terminate_thread.load() && recordQueue.batches.empty()) {
                break;  // If terminate is set and there's no data, then exit
            }

            continue;
        }

        for (const auto& record : records_batch) {
            std::cout<< " recording " << record.label << " duration " << record.duration << std::endl;
            labeled_stats[record.label].record_duration(record.duration);
        }
        records_batch.clear();
    }
    return nullptr;
}



//Added a global condition variable termination_cv for signaling thread termination.
//In the record_thread_func, if there's nothing to pop, the thread goes to sleep and waits on the condition variable until either there's data to process or it's time to terminate.
//In the test_perf function, after setting terminate_thread to true, we notify all waiting threads via termination_cv.notify_all().
//This approach reduces CPU usage of the recording thread when there's no work to do and ensures a quick exit when it's time to terminate.


void display_statistics() {
    std::cout << "Performance Statistics:\n";
    for (const auto& pair : labeled_stats) {
        const std::string& label = pair.first;
        const Stats& stats = pair.second;
        std::cout << "Label: " << label ;
        std::cout << "\tMax : " << stats.max_duration* 1000.0;   // convert to milliseconds
        std::cout << "\tMin : " << stats.min_duration *1000.0;   // convert to milliseconds
        std::cout << "\tAvg : " << (stats.total_duration / stats.count)* 1000.0;  // convert to milliseconds
        std::cout << "\tCount : " << stats.count << std::endl;
    }
}

struct Stats* getStats(std::string label)
{
    if (labeled_stats.find(label) != labeled_stats.end()) {
        return &labeled_stats[label];
    }
    return nullptr;
}

void runOneSecond()
{
    std::cout << " sleeping for a second" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
}


void stop_perf()
{
    terminate_thread.store(true);
    {
        Perf(" final call2");
        std::cout << " wait for thread to stop "<<std::endl;
    }
    termination_cv.notify_all(); // Notify the recording thread to terminate

    pthread_join(recordThread, nullptr);
    perf_running = false;
}


int test_perf() {
    baseTimeMs = std::chrono::system_clock::now();
    std::cout << " test_one_sec"<<std::endl;

    for (int i = 0 ; i < 10; i++)
    {
        std::cout << "Tick : " << i ;
        double dtime = get_time_double();
        std::cout << " double time =" << dtime << std::endl; 
        {
            Perf y("test_one_sec_two");
            auto x = Perf("test_one_sec_one");
            auto p = new Perf("test_one_sec");
            //x.ping();
            runOneSecond();
            //poll(nullptr, 0, 1000);
            delete p;
        }
    }
    std::cout << " test_one_msec"<<std::endl;

    for (int i = 0 ; i < 100; i++)
    {
        {
            auto p = new Perf("test_one_msec");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            delete p;
        }
    }

    std::cout << " flush"<<std::endl;
    recordQueue.flush();
    std::cout << " stop thread"<<std::endl;

    display_statistics();

    //terminate_thread.store(true);
    //termination_cv.notify_all(); // Notify the recording thread to terminate

    {
        Perf("final_call");
        std::cout << " wait for final_call "<<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // terminate_thread.store(true);
    // {
    //     Perf(" final call2");
    //     std::cout << " wait for thread to stop "<<std::endl;
    // }

    display_statistics();

    stop_perf();

    {
        Perf("another_final_call");
        std::cout << " wait for thread another final call "<<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    stop_perf();
    display_statistics();

    //pthread_cancel(recordThread);
    //pthread_join(recordThread, nullptr);


    return 0;
}


