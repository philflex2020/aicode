#include <iostream>
#include "perf.h"

std::queue<PerfMeasurement> Perf::perfQueue;
std::mutex Perf::perfMutex;
std::condition_variable Perf::perfCV;

void Perf::start() {
    if (!running) {
        startTime = std::chrono::high_resolution_clock::now();
        running = true;
    }
}

void Perf::stop() {
    if (running) {
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = endTime - startTime;
        elapsedTime = duration.count();
        running = false;

        // Create a measurement and add it to the queue
        PerfMeasurement measurement;
        measurement.name = name;
        measurement.elapsedTime = elapsedTime;

        std::lock_guard<std::mutex> lock(perfMutex);
        std::cout << measurement.elapsedTime <<std::endl;
        perfQueue.push(measurement);
        perfCV.notify_one();
    }
}

double Perf::getElapsedTime() const {
    return elapsedTime;
}


// Perf::Perf(const std::string& name) : name(name), running(false) {}

// void Perf::start() {
//     startTime = std::chrono::high_resolution_clock::now();
//     running = true;
// }

// void Perf::stop() {
//     if (running) {
//         auto endTime = std::chrono::high_resolution_clock::now();
//         auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
//         double elapsedTime = duration / 1000.0; // Convert to milliseconds

//         PerfMeasurement measurement;
//         measurement.name = name;
//         measurement.elapsedTime = elapsedTime;

//         running = false;

//         // Send the measurement to the aggregator thread using the provided queue and locks
//         sendToAggregator(queue, mutex, cv);
//     }
// }

// void Perf::sendToAggregator(std::queue<PerfMeasurement>& queue,
//                             std::mutex& mutex,
//                             std::condition_variable& cv) {
//     // Lock the mutex to safely access the shared queue
//     std::unique_lock<std::mutex> lock(mutex);

//     queue.push(measurement);

//     // Notify the aggregator thread that new data is available
//     cv.notify_one();
// }
