//producer.cpp:

//cpp
//Copy code
#include "producer.hpp"
#include <chrono>
#include <thread>
#include <random>

void Producer::run() {
    while (isRunning) {
        // Generate some random data
        DataObject data;
        data.uri = "URI" + std::to_string(rand() % 5 + 1);
        data.id = "ID" + std::to_string(rand() % 10 + 1);
        data.value = "Value" + std::to_string(rand() % 100 + 1);
        data.isCommand = false;

        // Add data to the queue
        std::lock_guard<std::mutex> lock(queueMutex);
        dataQueue.push(data);

        // Sleep for a while before producing the next data
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
