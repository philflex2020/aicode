#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <thread>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>

class Timer;

typedef std::function<void(Timer*, void*)> TimerCallback;

class Timer {
public:
    Timer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr);
    void start();
    void stop();
    void sync();

public:
    std::string name;
    static std::chrono::steady_clock::time_point baseTime;

private:
    std::chrono::steady_clock::time_point runtime;
    int interval;
    int synctime;
    int offset;
    TimerCallback callback;
    void* varPtr;
    std::atomic<bool> run;
};

extern std::unordered_map<std::string, std::shared_ptr<Timer>> timers;
extern void startTimer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr);
extern void stopTimer(std::string name);

#endif // TIMER_H
