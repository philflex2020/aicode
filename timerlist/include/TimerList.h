#pragma once

// TimerList
// p. wilshire
// 07_16_2023

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

// Timer object structure


struct Timer {
    std::string id;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point expireTime;
    std::chrono::high_resolution_clock::duration reloadTime;
    std::function<void(void*,Timer*,void*)> callback;
    void* callbackParam;
};

bool compareTimers(const Timer& t1, const Timer& t2) {
    return t1.expireTime < t2.expireTime;
}



struct TimerList {
    // Variables for managing timers
    std::vector<Timer> timers;
    std::mutex timersMutex;
    std::condition_variable timersCV;
    bool terminateThread = false;
    std::thread tlThread;
    bool threadRunning = false;

    static void timerManagementThread(TimerList& tl) {
        while (true) {
            std::unique_lock<std::mutex> lock(tl.timersMutex);
            if (tl.timers.empty() || tl.terminateThread) {
                break;
            }

            // Check the next timer's expiration time
            const Timer& nextTimer = tl.timers.front();
            std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
            if (currentTime >= nextTimer.expireTime) {
                // Remove the expired timer
                Timer expiredTimer = nextTimer;
                tl.timers.erase(tl.timers.begin());
                lock.unlock();

                // Execute the callback function
                expiredTimer.callback((void *)&tl, &expiredTimer, expiredTimer.callbackParam);

                // Reload the timer if needed
                if (expiredTimer.reloadTime != std::chrono::high_resolution_clock::duration::zero()) {
                    expiredTimer.expireTime += expiredTimer.reloadTime;
                    std::lock_guard<std::mutex> lock(tl.timersMutex);
                    tl.timers.push_back(expiredTimer);
                    std::sort(tl.timers.begin(), tl.timers.end(), compareTimers);
                }
            } else {
                // Wait until the next timer's expiration time or start time
                auto sleepTime = std::min(nextTimer.expireTime, nextTimer.startTime) - currentTime;
                tl.timersCV.wait_for(lock, sleepTime);
            }
        }

    }; 
    
    // std::thread run(TimerList* tl) { 
    //     std::lock_guard<std::mutex> lock(timersMutex);
    //     if (!threadRunning)
    //     {
    //         //tlthread = std::thread(timerManagementThread, std::ref(tl));
    //         threadRunning = true;
    //     }
    //     return tlThread;
    // };

    void add(const std::string& id, int startTimeMs,
                        int expireTimeMs,
                        int reloadTimeMs,
                        std::function<void(void *, Timer*, void*)> callback,
                        void* callbackParam) {
        std::lock_guard<std::mutex> lock(timersMutex);

        // Check if the timer already exists
        auto timerIt = std::find_if(timers.begin(), timers.end(), [id](const Timer& timer) {
            return timer.id == id;
        });

        auto startTime  = std::chrono::milliseconds(startTimeMs);
        auto expireTime = std::chrono::milliseconds(expireTimeMs);
        auto reloadTime = std::chrono::milliseconds(reloadTimeMs);

        if (timerIt != timers.end()) {
            // Modify existing timer
            Timer& existingTimer = *timerIt;
            existingTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
            existingTimer.expireTime = existingTimer.startTime + expireTime;
            existingTimer.reloadTime = reloadTime;
            existingTimer.callback = callback;
            existingTimer.callbackParam = callbackParam;

            std::sort(timers.begin(), timers.end(), compareTimers);
        } else {
            // Add new timer
            Timer newTimer;
            newTimer.id = id;
            newTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
            newTimer.expireTime = newTimer.startTime + expireTime;
            newTimer.reloadTime = reloadTime;
            newTimer.callback = callback;
            newTimer.callbackParam = callbackParam;
            timers.push_back(newTimer);
            std::sort(timers.begin(), timers.end(), compareTimers);
        }

        timersCV.notify_one();
    };

    // Function to add or modify a timer in the list
    void addOrModifyTimer(const std::string& id, std::chrono::high_resolution_clock::duration startTime,
                        std::chrono::high_resolution_clock::duration expireTime,
                        std::chrono::high_resolution_clock::duration reloadTime,
                        std::function<void(void *, Timer*, void*)> callback,
                        void* callbackParam) {
        std::lock_guard<std::mutex> lock(timersMutex);

        // Check if the timer already exists
        auto timerIt = std::find_if(timers.begin(), timers.end(), [id](const Timer& timer) {
            return timer.id == id;
        });

        if (timerIt != timers.end()) {
            // Modify existing timer
            Timer& existingTimer = *timerIt;
            existingTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
            existingTimer.expireTime = existingTimer.startTime + expireTime;
            existingTimer.reloadTime = reloadTime;
            existingTimer.callback = callback;
            existingTimer.callbackParam = callbackParam;

            std::sort(timers.begin(), timers.end(), compareTimers);
        } else {
            // Add new timer
            Timer newTimer;
            newTimer.id = id;
            newTimer.startTime = std::chrono::high_resolution_clock::now() + startTime;
            newTimer.expireTime = newTimer.startTime + expireTime;
            newTimer.reloadTime = reloadTime;
            newTimer.callback = callback;
            newTimer.callbackParam = callbackParam;
            timers.push_back(std::move(newTimer));
            std::sort(timers.begin(), timers.end(), compareTimers);
        }

        timersCV.notify_one();
    };

    // Function to delete a timer from the list
    void deleteTimer(const std::string& id) {
        std::lock_guard<std::mutex> lock(timersMutex);
        timers.erase(std::remove_if(timers.begin(), timers.end(), [id](const Timer& timer) {
            return timer.id == id;
        }), timers.end());
    };

    void deleteTimerNoLock(const std::string& id) {
        timers.erase(std::remove_if(timers.begin(), timers.end(), [id](const Timer& timer) {
            return timer.id == id;
        }), timers.end());
    };

    // Function to wake up the management thread and terminate it
    void wakeUpAndTerminate() {
        std::lock_guard<std::mutex> lock(timersMutex);
        terminateThread = true;
        timersCV.notify_one();
    };

    // Function to show all items in the timer list
    void showTimers() {
        std::lock_guard<std::mutex> lock(timersMutex);
        std::cout << "Timer List:" << std::endl;
        for (const auto& timer : timers) {
            std::cout << "ID: " << timer.id << ", Expire Time: "
                    << std::chrono::high_resolution_clock::to_time_t(timer.expireTime) << std::endl;
        }
    }
    // Function to delete all items in the timer list
    void deleteAllTimers() {
        std::lock_guard<std::mutex> lock(timersMutex);
        int idx = 0;
        for (auto &timer : timers) {
            //timers.erase(idx, idx+1);
            idx++;
        }
        timers.clear();
    }
};

std::thread TimerListRun (TimerList &tl) {
    std::lock_guard<std::mutex> lock(tl.timersMutex);
    std::thread tlThread;
    if (!tl.threadRunning)
    {
        tlThread = std::thread(tl.timerManagementThread, std::ref(tl));
        tl.tlThread = std::move(tlThread);
        tl.threadRunning = true;
    }
    return tlThread;
}
