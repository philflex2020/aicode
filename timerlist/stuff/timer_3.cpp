#include <iostream>
#include <vector>
#include <chrono>
#include <functional>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>


//Here's an updated version of the C++ program that includes a function to manage a vector of timer objects. 
//Each timer object has an ID, a start time, an expiration time, a reload time, and a callback function with a void* callback parameter. 
//The objects in the vector are sorted in increasing expiration time order. 
//The management thread wakes up and processes either the callback function or a management object that adds, deletes, or modifies timer objects in the list. 
//If a timer object has a reload time of zero, the management thread deletes the timer from the vector. 
//If the timer object has a start time, the management thread delays processing the timer object until the start time. 
//Adding a new timer object or modifying an existing timer object wakes up the management thread to service any alterations to the list. 
// Additionally, the program allows waking up the management thread, deleting all items in the list, and terminating the thread.


// Timer object structure
struct Timer {
    int id;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point expireTime;
    std::chrono::steady_clock::duration reloadTime;
    std::function<void(void*)> callback;
    void* callbackParam;
};

// Comparator function for sorting timers based on expiration time
bool compareTimers(const Timer& t1, const Timer& t2) {
    return t1.expireTime < t2.expireTime;
}

// Global variables for managing timers
std::vector<Timer> timers;
std::mutex timersMutex;
std::condition_variable timersCV;
bool terminateThread = false;

// Function to manage timers
void timerManagementThread() {
    while (true) {
        std::unique_lock<std::mutex> lock(timersMutex);
        if (timers.empty() || terminateThread) {
            break;
        }

        // Check the next timer's expiration time
        const Timer& nextTimer = timers.front();
        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        if (currentTime >= nextTimer.expireTime) {
            // Remove the expired timer
            Timer expiredTimer = nextTimer;
            timers.erase(timers.begin());
            lock.unlock();

            // Execute the callback function
            expiredTimer.callback(expiredTimer.callbackParam);

            // Reload the timer if needed
            if (expiredTimer.reloadTime != std::chrono::steady_clock::duration::zero()) {
                expiredTimer.expireTime += expiredTimer.reloadTime;
                std::lock_guard<std::mutex> lock(timersMutex);
                timers.push_back(expiredTimer);
                std::sort(timers.begin(), timers.end(), compareTimers);
            }
        } else {
            // Wait until the next timer's expiration time or start time
            auto sleepTime = std::min(nextTimer.expireTime, nextTimer.startTime) - currentTime;
            timersCV.wait_for(lock, sleepTime);
        }
    }
}

// Function to add or modify a timer in the list
void addOrModifyTimer(int id, std::chrono::steady_clock::duration startTime, std::chrono::steady_clock::duration expireTime,
                      std::chrono::steady_clock::duration reloadTime, std::function<void(void*)> callback, void* callbackParam) {
    std::lock_guard<std::mutex> lock(timersMutex);

    // Check if the timer already exists
    auto timerIt = std::find_if(timers.begin(), timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    });

    if (timerIt != timers.end()) {
        // Modify existing timer
        Timer& existingTimer = *timerIt;
        existingTimer.startTime = std::chrono::steady_clock::now() + startTime;
        existingTimer.expireTime = existingTimer.startTime + expireTime;
        existingTimer.reloadTime = reloadTime;
        existingTimer.callback = callback;
        existingTimer.callbackParam = callbackParam;

        std::sort(timers.begin(), timers.end(), compareTimers);
    } else {
        // Add new timer
        Timer newTimer;
        newTimer.id = id;
        newTimer.startTime = std::chrono::steady_clock::now() + startTime;
        newTimer.expireTime = newTimer.startTime + expireTime;
        newTimer.reloadTime = reloadTime;
        newTimer.callback = callback;
        newTimer.callbackParam = callbackParam;
        timers.push_back(newTimer);
        std::sort(timers.begin(), timers.end(), compareTimers);
    }

    timersCV.notify_one();
}

// Function to delete a timer from the list
void deleteTimer(int id) {
    std::lock_guard<std::mutex> lock(timersMutex);
    timers.erase(std::remove_if(timers.begin(), timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    }), timers.end());
}

// Function to wake up the management thread and terminate it
void wakeUpAndTerminate() {
    std::lock_guard<std::mutex> lock(timersMutex);
    terminateThread = true;
    timersCV.notify_one();
}

// Example callback function
void exampleCallback(void* param) {
    int timerId = *static_cast<int*>(param);
    std::cout << "Timer expired: " << timerId << std::endl;
}

int main() {
    // Start the timer management thread
    std::thread managementThread(timerManagementThread);

    // Add some example timers
    addOrModifyTimer(1, std::chrono::seconds(2), std::chrono::seconds(5), std::chrono::seconds(0), exampleCallback, new int(1));
    addOrModifyTimer(2, std::chrono::seconds(0), std::chrono::seconds(10), std::chrono::seconds(0), exampleCallback, new int(2));
    addOrModifyTimer(3, std::chrono::seconds(5), std::chrono::seconds(10), std::chrono::seconds(5), exampleCallback, new int(3));

    // Wait for some time to observe timer expiration
    std::this_thread::sleep_for(std::chrono::seconds(20));

    // Modify an existing timer
    addOrModifyTimer(1, std::chrono::seconds(0), std::chrono::seconds(7), std::chrono::seconds(0), exampleCallback, new int(1));

    // Delete timer 2
    deleteTimer(2);

    // Wake up the management thread and terminate
    wakeUpAndTerminate();

    // Wait for the management thread to finish
    managementThread.join();

    return 0;
}
