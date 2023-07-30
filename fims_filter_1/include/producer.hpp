#ifndef PRODUCER_HPP
#define PRODUCER_HPP

#include <queue>
#include <mutex>
#include "data_object.hpp"

class Producer {
public:
    void run();

private:
    std::queue<DataObject>& dataQueue; // Reference to the data queue
    std::mutex& queueMutex; // Reference to the mutex for data queue access
    bool isRunning;

public:
    Producer(std::queue<DataObject>& dataQueue, std::mutex& queueMutex) 
        : dataQueue(dataQueue), queueMutex(queueMutex), isRunning(true) {}

    void stop() {
        isRunning = false;
    }
};

#endif // PRODUCER_HPP

