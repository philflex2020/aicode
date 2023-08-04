# GCOM Process Monitoring - Specification

The GCOM Process Monitoring system aims to provide a way to monitor the performance of code blocks and measure their 
execution time. 
It consists of a timer object called `Perf`, which can be placed at the start of a code block to record the execution time. 
Additionally, there is a performance data aggregation thread responsible for collecting and aggregating the performance data 
sent by the `Perf` objects.

## Timer Object - Perf

### Suggested Class Definition:

```cpp
class Perf {
public:
    Perf(const std::string& userId, const std::string& timerId, const std::string& message);
    ~Perf();

private:
    std::string userId_;
    std::string timerId_;
    std::string message_;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime_;
};
```

### Possible Usage Example:

```cpp
void someFunction() {
    Perf perf("User1", "Timer1", "Start of someFunction");

    // Your code block here...

} // Perf destructor will be called here and send the performance data to the thread
```

## Performance Data Aggregation Thread

The performance data aggregation thread is responsible for collecting and aggregating the performance data sent by the `Perf` objects. The thread runs in the background and processes the data from the message queue.

### Example Thread Implementation:

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

struct PerfData {
    std::string userId;
    std::string timerId;
    std::string message;
    long long elapsedTime; // in microseconds
};

class PerfAggregator {
public:
    PerfAggregator();
    ~PerfAggregator();

    void addPerfData(const PerfData& data);

private:
    std::queue<PerfData> dataQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueNotEmpty_;
    std::thread aggregatorThread_;

    void aggregatorLoop();
};
```

### Usage Example:

```cpp
void PerfAggregator::addPerfData(const PerfData& data) {
    std::unique_lock<std::mutex> lock(queueMutex_);
    dataQueue_.push(data);
    queueNotEmpty_.notify_one();
}

void PerfAggregator::aggregatorLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueNotEmpty_.wait(lock, [this]() { return !dataQueue_.empty(); });

        // Process the data in the queue
        while (!dataQueue_.empty()) {
            PerfData data = dataQueue_.front();
            dataQueue_.pop();

            // Aggregate the data and save it to the performance database

            // Print the performance data for demonstration purposes
            std::cout << "User: " << data.userId << ", Timer: " << data.timerId
                      << ", Message: " << data.message << ", Elapsed Time: " << data.elapsedTime << " microseconds" << std::endl;
        }
    }
}
```

## Logging Data Storage

For the logging system, an external logging object can be used to store log messages. The logging object should provide a way to store log messages based on their origin id and log id. Additionally, it should retain up to 1024 repeated messages to help identify patterns.

### Logging Object Functions:

```cpp
class LoggingObject {
public:
    void storeLogMessage(const std::string& originId, const std::string& logId, const std::string& message);
    void clearLogMessages(const std::string& originId, const std::string& logId);
    std::string getLogMessage(const std::string& originId, const std::string& logId, int messageIndex);
    int getLogMessageCount(const std::string& originId, const std::string& logId);
};
```

## Conclusion

The GCOM Process Monitoring system, together with the external logging object, provides a comprehensive solution for monitoring the performance of code blocks and logging relevant information. By using the `Perf` class and the performance data aggregation thread, developers can gain insights into the performance of their code and make informed optimization decisions. The external logging object ensures that log messages are efficiently stored and retrievable, allowing for detailed analysis and troubleshooting. With the GCOM Process Monitoring system, developers can effectively optimize their applications and improve overall performance.

---

## Addendum - Sample Code

### Perf Class Implementation:

```cpp
#include <iostream>
#include <chrono>
#include <string>

class Perf {
public:
    Perf(const std::string& userId, const std::string& timerId, const std::string& message) 
        : userId_(userId), timerId_(timerId), message_(message) {
        startTime_ = std::chrono::high_resolution_clock::now();
    }

    ~Perf() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_).count();

        // Send performance data to the aggregation thread
        PerfData data;
        data.userId = userId_;
        data.timerId = timerId_;
        data.message = message_;
        data.elapsedTime = elapsedTime;

        aggregator_.addPerfData(data);
    }

private:
    std::string userId_;
    std::string timerId_;
    std::string message_;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime_;

    // Aggregator instance (singleton)
    static PerfAggregator aggregator_;
};

// Initialize static member variable
PerfAggregator Perf::aggregator_;

// Usage Example:
void someFunction() {
    Perf perf("User1", "Timer1", "Start of someFunction");

    // Your code block here...
}
```

### PerfAggregator Class Implementation:

```cpp
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

class PerfAggregator {
public:
    PerfAggregator() {
        aggregatorThread_ = std::thread(&PerfAggregator::aggregatorLoop, this);
    }

    ~PerfAggregator() {
        // Signal the aggregator thread to stop and join
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stopAggregator

_ = true;
            queueNotEmpty_.notify_one();
        }
        aggregatorThread_.join();
    }

    void addPerfData(const PerfData& data) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        dataQueue_.push(data);
        queueNotEmpty_.notify_one();
    }

private:
    std::queue<PerfData> dataQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueNotEmpty_;
    std::thread aggregatorThread_;
    bool stopAggregator_ = false;

    void aggregatorLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueNotEmpty_.wait(lock, [this]() { return !dataQueue_.empty() || stopAggregator_; });

            if (stopAggregator_) {
                break; // Exit the thread loop
            }

            // Process the data in the queue
            while (!dataQueue_.empty()) {
                PerfData data = dataQueue_.front();
                dataQueue_.pop();

                // Aggregate the data and save it to the performance database

                // Print the performance data for demonstration purposes
                std::cout << "User: " << data.userId << ", Timer: " << data.timerId
                          << ", Message: " << data.message << ", Elapsed Time: " << data.elapsedTime << " microseconds" << std::endl;
            }
        }
    }
};
```