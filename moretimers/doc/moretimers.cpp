#include <iostream>
#include <thread>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>

// g++ -std=c++17 -o rc src/moretimers.cpp  -lpthread
class Timer;

// Define a type for the callback functions
typedef std::function<void(Timer*, void*)> TimerCallback;

// Define a global variable to store the base time
std::chrono::steady_clock::time_point baseTime = std::chrono::steady_clock::now();

// Define a class for the timer
class Timer {
public:
    Timer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr) 
        : name(name), interval(interval), offset(offset), callback(callback), varPtr(varPtr) {}
    
    void start() {
        run = true;
        auto now = std::chrono::steady_clock::now();
        auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTime).count();
        int initialDelay = offset - timeSinceStart;
        if(initialDelay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(initialDelay));
        }
        while(run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if (synctime>0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(synctime));
                synctime = 0;
            }
            runtime  = std::chrono::steady_clock::now();

            callback(this, varPtr);
        }
    }
    void stop(){
        run = false;
    }

    void sync () {  
        auto now = std::chrono::steady_clock::now();
        auto timeSinceRun = std::chrono::duration_cast<std::chrono::milliseconds>(now - runtime);
        auto remainingTime = std::chrono::milliseconds(interval) - timeSinceRun;
        if (remainingTime.count() > interval / 2) {
            synctime = remainingTime.count();
        } else {
            synctime = 0;
        }
    }

public:
    std::string name;
private:
    std::chrono::steady_clock::time_point runtime;
    int interval;
    int synctime;
    int offset;
    TimerCallback callback;
    void* varPtr;
    std::atomic<bool> run;
};

// Define a global map to keep track of the timers
std::unordered_map<std::string, std::shared_ptr<Timer>> timers;
// Define a function to start a timer
void startTimer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr) {
    std::shared_ptr<Timer> timer = std::make_shared<Timer>(name, interval, offset, callback, varPtr);
    std::thread timerThread(&Timer::start, timer);
    timerThread.detach();
    timers[name] = timer;
}

// Define a function to stop a timer
void stopTimer(std::string name) {
    timers[name]->stop();
    timers.erase(name);
}
// // Define a function to start a timer
// void startTimer(std::string name, int interval, int offset, TimerCallback callback, void* varPtr) {
//     Timer timer(name, interval, offset, callback, varPtr);
//     std::thread timerThread(&Timer::start, timer);
//     timers[name] = std::move(timerThread);
// }

// // Define a function to stop a timer
// void stopTimer(std::string name) {
//     timers[name].run = false;
//     timers[name].join();
//     timers.erase(name);
// }

// Example callback function
void onTimer(Timer* timer, void* varPtr) {
    // Do something with varPtr
    std::cout << "Timer " << timer->name << " callback called for variable at address: " << varPtr << std::endl;
}

// Example usage
int main() {
    // Record the base time
    baseTime = std::chrono::steady_clock::now();

    // Create a variable
    int var1 = 10;
    int var2 = 20;

    // Start a timer
    startTimer("timer1", 1000, 500, onTimer, &var1);

    // Do something else
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    startTimer("timer2", 1000, 400, onTimer, &var2);

    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    // Stop the timer
    stopTimer("timer1");
    stopTimer("timer2");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}
