#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace timer {
    enum Constants {
        RECALIBRATE = 1234
    };
}
// Define the TimeObject class

// class TimeObject;
// std::vector<std::shared_ptr<TimeObject>> timeObjects;

class TimeObject {
public:
    std::string name;
    double runTime = 0.0;
    double stopTime = 0.0;
    double repeatTime = 0.0;
    double offsetTime = 0.0;
    double lastRun= 0.0;
    int count = 0;    // run this many times
    int runs = 0;     // inc this each time we run
    void (*callback)(std::shared_ptr<TimeObject>,void*)= nullptr;
    void* param = nullptr;
    bool enabled = false;
    bool sync = false;
    bool run = false;
    
    ~TimeObject()
    {
        std::cout << " Timer object ["<< name<<"] deleted " << std::endl; 
    }

    TimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, double timer_repeatTime, int timer_count,
                        void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams)
        : name(timer_name), 
            runTime(timer_runTime), 
            stopTime(timer_stopTime), 
            repeatTime(timer_repeatTime), 
            offsetTime(0.0), 
            lastRun(0.0), 
            count(timer_count),
            runs(0), 
            callback(timer_callback), 
            param(timer_callbackParams), 
            enabled(true) ,
            sync(false) ,
            run(false) 
        {
            printf(" Timer object created #11 name %p param %p sync %p\n", (void*)&name, (void*)&param, (void*)&sync);
            enabled = true;
            runs = 0;
            offsetTime = 0.0;
            sync = false;
            run = false;
            lastRun = 0.0;
            //timeObjects.emplace_back(this);
        }
    TimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, double timer_repeatTime, int timer_count, 
                                int timer_offset, void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams)
        :  name(timer_name), 
           runTime(timer_runTime), 
           stopTime(timer_stopTime), 
           repeatTime(timer_repeatTime), 
           count(timer_count), 
           callback(timer_callback), 
           param(timer_callbackParams) 
        {
            printf(" Timer object created #12 name %p param %p sync %p\n", (void*)&name, (void*)&param, (void*)&sync);
            enabled = true;
            runs = 0;
            offsetTime = timer_offset;
            sync = false;
            run = false;
            lastRun = 0.0;
            //timeObjects.emplace_back(this);
        }

    void setCallback(void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams){
        callback = timer_callback;
        param = timer_callbackParams;
    }

};


template <typename T>
class Channel {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void send(const T& message) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            queue.push(message);
        }
        cv.notify_one();
    }

    bool receive(T& message) {
        std::unique_lock<std::mutex> lock(mtx);
        while (queue.empty()) {
            cv.wait(lock);
        }

        message = queue.front();
        queue.pop();
        return true;
    }
    // Overloaded receive with a timeout
    bool receive(T& message, const std::chrono::microseconds& duration) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, duration, [this] { return !queue.empty(); })) {
            message = queue.front();
            queue.pop();
            return true;
        }
        return false; // timed out without receiving a message
    }
};



std::shared_ptr<TimeObject> createTimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, double timer_repeatTime,
     int timer_count, void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams);
std::shared_ptr<TimeObject> createOffsetTimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, double timer_repeatTime,
     int timer_count, int timer_offset, void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams);

void addTimeObject(std::shared_ptr<TimeObject> obj, double offset, bool sync, bool useSet=false);
//void addSyncTimeObject(std::shared_ptr<TimeObject> obj, double offset = 0.0);
void showTimerObjects(bool useSet=false);
void runTimer();
void stopTimer();

double get_time_double();
bool syncTimeObjectByName(const std::string& name, double error_margin_percent, bool useSet);
// void addTimeObject(const TimeObject& obj, double offset=0.0);
// void addSyncTimeObject(const TimeObject& obj, double offset=0.0);
// void showTimerObjects(void);
// void runTimer();
// void stopTimer();

int test_timer(bool useSet);

