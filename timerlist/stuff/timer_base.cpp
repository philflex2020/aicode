#include <iostream>
#include <vector>
#include <chrono>
#include <functional>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>
#include <getopt.h>
#include <gtest/gtest.h>

#include "timer_1.h"

// Timer object structure

//TimerCtl tcl;

// Global variables for managing timers
// std::vector<Timer> timers;
// std::mutex timersMutex;
// std::condition_variable timersCV;
// bool terminateThread = false;
// Comparator function for sorting timers based on expiration time
bool compareTimers(const Timer& t1, const Timer& t2) {
    return t1.expireTime < t2.expireTime;
}

// Function to manage timers
void timerManagementThread(TimerCtl &tcl) {
    while (true) {
        std::unique_lock<std::mutex> lock(tcl.timersMutex);
        if (tcl.timers.empty() || tcl.terminateThread) {
            break;
        }

        // Check the next timer's expiration time
        const Timer& nextTimer = tcl.timers.front();
        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        if (currentTime >= nextTimer.expireTime) {
            // Remove the expired timer
            Timer expiredTimer = nextTimer;
            tcl.timers.erase(tcl.timers.begin());
            lock.unlock();

            // Execute the callback function
            expiredTimer.callback((void *)&tcl, expiredTimer.callbackParam);

            // Reload the timer if needed
            if (expiredTimer.reloadTime != std::chrono::high_resolution_clock::duration::zero()) {
                expiredTimer.expireTime += expiredTimer.reloadTime;
                std::lock_guard<std::mutex> lock(tcl.timersMutex);
                tcl.timers.push_back(expiredTimer);
                std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
            }
        } else {
            // Wait until the next timer's expiration time or start time
            auto sleepTime = std::min(nextTimer.expireTime, nextTimer.startTime) - currentTime;
            tcl.timersCV.wait_for(lock, sleepTime);
        }
    }
}

// Function to add or modify a timer in the list
void addOrModifyTimer(TimerCtl &tcl, const std::string& id, std::chrono::high_resolution_clock::duration startTime,
                      std::chrono::high_resolution_clock::duration expireTime,
                      std::chrono::high_resolution_clock::duration reloadTime,
                      std::function<void(void *, void*)> callback,
                      void* callbackParam) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);

    // Check if the timer already exists
    auto timerIt = std::find_if(tcl.timers.begin(), tcl.timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    });

    if (timerIt != tcl.timers.end()) {
        // Modify existing timer
        Timer& existingTimer = *timerIt;
        existingTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        existingTimer.expireTime = existingTimer.startTime + expireTime;
        existingTimer.reloadTime = reloadTime;
        existingTimer.callback = callback;
        existingTimer.callbackParam = callbackParam;

        std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
    } else {
        // Add new timer
        Timer newTimer;
        newTimer.id = id;
        newTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
        newTimer.expireTime = newTimer.startTime + expireTime;
        newTimer.reloadTime = reloadTime;
        newTimer.callback = callback;
        newTimer.callbackParam = callbackParam;
        tcl.timers.push_back(newTimer);
        std::sort(tcl.timers.begin(), tcl.timers.end(), compareTimers);
    }

    tcl.timersCV.notify_one();
}

// Function to delete a timer from the list
void deleteTimer(TimerCtl &tcl, const std::string& id) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.timers.erase(std::remove_if(tcl.timers.begin(), tcl.timers.end(), [id](const Timer& timer) {
        return timer.id == id;
    }), tcl.timers.end());
}

// Function to wake up the management thread and terminate it
void wakeUpAndTerminate(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.terminateThread = true;
    tcl.timersCV.notify_one();
}

// Function to show all items in the timer list
void showTimerList(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    std::cout << "Timer List:" << std::endl;
    for (const auto& timer : tcl.timers) {
        std::cout << "ID: " << timer.id << ", Expire Time: "
                  << std::chrono::high_resolution_clock::to_time_t(timer.expireTime) << std::endl;
    }
}

// Function to delete all items in the timer list
void deleteAllTimers(TimerCtl &tcl) {
    std::lock_guard<std::mutex> lock(tcl.timersMutex);
    tcl.timers.clear();
}
