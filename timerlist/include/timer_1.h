#pragma once

// timer_1.h
// p. wilshire
// 07_14_2023

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
    std::function<void(void*,void*)> callback;
    void* callbackParam;
};
struct TimerCtl {
    // Variables for managing timers
    std::vector<Timer> timers;
    std::mutex timersMutex;
    std::condition_variable timersCV;
    bool terminateThread = false;
};

void timerManagementThread(TimerCtl &tcl);

// Function to add or modify a timer in the list
void addOrModifyTimer(TimerCtl &tcl, const std::string& id, std::chrono::high_resolution_clock::duration startTime,
                      std::chrono::high_resolution_clock::duration expireTime,
                      std::chrono::high_resolution_clock::duration reloadTime,
                      std::function<void(void*,void*)> callback,
                      void* callbackParam);

// Function to delete a timer from the list
void deleteTimer(TimerCtl &tcl, const std::string& id);

// Function to wake up the management thread and terminate it
void wakeUpAndTerminate(TimerCtl &tcl);

// Function to show all items in the timer list
void showTimerList(TimerCtl &tcl ); 

// Function to delete all items in the timer list
void deleteAllTimers(TimerCtl &tcl );

