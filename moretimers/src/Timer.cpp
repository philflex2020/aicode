#include "Timer.h"

std::chrono::steady_clock::time_point Timer::baseTime = std::chrono::steady_clock::now();
std::unordered_map<std::string, std::shared_ptr<Timer>> timers;

Timer::Timer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr) 
    : name(name), interval(interval), offset(offset), callback(callback), varPtr(varPtr), synctime(0), run(false) {}

void Timer::start() {
    run = true;
    auto now = std::chrono::steady_clock::now();
    auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTime).count();
    int initialDelay = offset - timeSinceStart;
    if(initialDelay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(initialDelay));
    }
    while(run) {
        runtime = std::chrono::steady_clock::now();
        //runtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTime).count();
        //runtime  = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        if (synctime>0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(synctime));
            synctime = 0;
        }
        callback(this, varPtr);
    }


}

void Timer::stop() {
    run = false;
}

void Timer::sync () {  
    auto now = std::chrono::steady_clock::now();
    auto timeSinceRun = std::chrono::duration_cast<std::chrono::milliseconds>(now - runtime);
//    auto remainingTime = std::chrono::milliseconds(interval) - timeSinceRun;
    std::cout << " Time since run " << timeSinceRun.count() << " interval " << interval << std::endl;
    if (timeSinceRun.count() > interval / 2) {
        synctime = timeSinceRun.count();
    } else {
        synctime = 0;
    }
}

void startTimer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr) {
    std::shared_ptr<Timer> timer = std::make_shared<Timer>(name, interval, offset, callback, varPtr);
    std::thread timerThread(&Timer::start, timer);
    timerThread.detach();
    timers[name] = timer;
}

void stopTimer(std::string name) {
    timers[name]->stop();
    timers.erase(name);
}
