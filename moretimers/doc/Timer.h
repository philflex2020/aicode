#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <atomic>
#include <thread>
#include <functional>

class Timer {
public:
    Timer(std::chrono::milliseconds interval, std::chrono::milliseconds offset, 
            std::function<void(Timer*, void*)> callback, void* varPtr);

    void start();
    void stop();
    void sync();
    
private:
    std::chrono::milliseconds interval;
    std::chrono::milliseconds offset;
    std::chrono::steady_clock::time_point baseTime;
    std::chrono::steady_clock::time_point runtime;
    int synctime;
    std::atomic<bool> run;
    std::function<void(Timer*, void*)> callback;
    void* varPtr;
};

#endif // TIMER_H