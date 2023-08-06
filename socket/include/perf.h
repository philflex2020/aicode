#ifndef PERF_H
#define PERF_H

#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>

class PerfMeasurement {
    public:
        std::string name;
        double elapsedTime; // Time in milliseconds
    PerfMeasurement() : elapsedTime(0.0) {}
};

// std::queue<PerfMeasurement> perfQueue;
// std::mutex perfMutex;
// std::condition_variable perfCV;
class Perf {
    private:
    std::string name;
    bool running;
    double elapsedTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
public:
    static std::queue<PerfMeasurement> perfQueue;
    static std::mutex perfMutex;
    static std::condition_variable perfCV;
public:
    explicit Perf(const std::string& name) : name(name), running(false), elapsedTime(0.0) {}

    void start();

    void stop();

    double getElapsedTime() const;


};

// class Perf {
// public:
//     Perf(const std::string& name);

//     void start();
//     void stop();

//     void sendToAggregator(std::queue<PerfMeasurement>& queue,
//                           std::mutex& mutex,
//                           std::condition_variable& cv);

// private:
//     std::string name;
//     bool running;
//     std::chrono::high_resolution_clock::time_point startTime;
// };
// // namespace perf {

// // class Timing {
// // public:
// //     Timing() : startTime(), endTime(), running(false) {}

// //     void start() {
// //         startTime = std::chrono::high_resolution_clock::now();
// //         running = true;
// //     }

// //     void stop() {
// //         endTime = std::chrono::high_resolution_clock::now();
// //         running = false;
// //     }

// //     double getDuration() const {
// //         if (running) {
// //             auto current = std::chrono::high_resolution_clock::now();
// //             return std::chrono::duration<double, std::milli>(current - startTime).count();
// //         } else {
// //             return std::chrono::duration<double, std::milli>(endTime - startTime).count();
// //         }
// //     }

// // private:
// //     std::chrono::high_resolution_clock::time_point startTime;
// //     std::chrono::high_resolution_clock::time_point endTime;
// //     bool running;
// // };

// // class Collector {
// // public:
// //     void addTiming(double duration) {
// //         std::lock_guard<std::mutex> lock(mutex_);
// //         timings_.push(duration);
// //         cv_.notify_one();
// //     }

// //     double getTiming() {
// //         std::unique_lock<std::mutex> lock(mutex_);
// //         cv_.wait(lock, [this] { return !timings_.empty(); });
// //         double duration = timings_.front();
// //         timings_.pop();
//         return duration;
//     }

// private:
//     std::queue<double> timings_;
//     std::mutex mutex_;
//     std::condition_variable cv_;
// };

// } // namespace perf

#endif // PERF_H
