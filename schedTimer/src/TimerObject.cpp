#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "TimerObject.h"

//g++ -o tim -I include src/TimerObject.cpp  -lpthread
std::mutex timeObjectsMutex;
std::vector<TimeObject> timeObjects;
std::chrono::system_clock::time_point baseTime;
Channel<std::string> timerChannel;
Channel<std::string> signalChannel;
std::condition_variable timerCV;
std::atomic<bool> shouldTerminate;


double get_time_double() {
    //if (baseTime == 0)
    //    baseTime = std::chrono::system_clock::now();

    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = currentTime - baseTime;
    return duration.count();
}

// Add a TimeObject in sorted order
void addTimeObject(const TimeObject& obj) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto pos = std::lower_bound(timeObjects.begin(), timeObjects.end(), obj,
                                [](const TimeObject& a, const TimeObject& b) {
                                    return a.runTime < b.runTime;
                                });
    timeObjects.insert(pos, obj);
}

// Get the delay time for the next event, and if needed, run it
void checkAndRunFirst() {
    if (timeObjects.empty()) return;

    double currentTime = get_time_double();
    TimeObject& first = timeObjects.front();

    if (first.runTime <= currentTime) {
        first.count++;
        first.callback(&first, first.param);

        if (first.repeatTime > 0) {

            // move on to the next cycle time
            first.runTime += first.repeatTime;
            // but allow stop time to end this.
            if ((first.stopTime == 0 ) || (first.runTime < first.stopTime)) {
                addTimeObject(first);
                // Signal the main timer thread to adjust its delay.
                timerChannel.send("recalibrate");
            }
        }

        timeObjects.erase(timeObjects.begin());
    }
}

void exampleCallback(TimeObject* t, void* p) {
    double rtime = get_time_double();
    std::cout << "Callback for :" << t->name <<" executed at : " << rtime << " with param: " << *((int*)p) << std::endl;
}

TimeObject* findTimeObjectByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::find_if(timeObjects.begin(), timeObjects.end(),
                           [&name](const TimeObject& obj) { return obj.name == name; });
    return (it != timeObjects.end()) ? &(*it) : nullptr;
}

bool removeTimeObjectByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::remove_if(timeObjects.begin(), timeObjects.end(),
                             [&name](const TimeObject& obj) { return obj.name == name; });
    if (it != timeObjects.end()) {
        timeObjects.erase(it, timeObjects.end());
        timerChannel.send("recalibrate");
        return true;
    }
    return false;
}

// if less than 0.3 of repeat time is left then delay the next run time.
//syncTimeObjectByName(mytimername, 0.3) 

bool syncTimeObjectByName(const std::string& name, double sleft) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::remove_if(timeObjects.begin(), timeObjects.end(),
                             [&name](const TimeObject& obj) { return obj.name == name; });
    if (it != timeObjects.end()) {
        double tNow = get_time_double();

        if ((it->runTime - tNow) < (it->repeatTime * sleft)) {
            it->runTime = tNow + it->repeatTime;
        }
        // After modification, re-sort the vector to maintain order based on runTime.
        std::sort(timeObjects.begin(), timeObjects.end(), [](const TimeObject& a, const TimeObject& b) {
            return a.runTime < b.runTime;
        });
        timerChannel.send("recalibrate");

        return true;
    }
    return false;
}


bool modifyTimeObjectByName(const std::string& name, double newRunTime, double newStopTime, double newRepeatTime) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::find_if(timeObjects.begin(), timeObjects.end(), [&](const TimeObject& obj) {
        return obj.name == name;
    });

    if (it != timeObjects.end()) {
        if(newRunTime > 0) it->runTime = newRunTime;
        if(newStopTime > 0 ) it->stopTime = newStopTime;
        if(newRepeatTime > 0) it->repeatTime = newRepeatTime;
        
        // After modification, re-sort the vector to maintain order based on runTime.
        std::sort(timeObjects.begin(), timeObjects.end(), [](const TimeObject& a, const TimeObject& b) {
            return a.runTime < b.runTime;
        });
        timerChannel.send("recalibrate");
        return true;
    }
    return false;
}


void showTimerObjects()
{
    std::lock_guard<std::mutex> lock(timeObjectsMutex);

    for (auto t : timeObjects )
    std::cout << t.name << " runTime :" << t.runTime << std::endl;
}


void timerThreadFunc() {
    shouldTerminate =  false;

    while (!shouldTerminate) {
        double delay = -1;
        bool shouldRunFirst = false; // flag to determine if we need to run the first timer

        
        {
            std::lock_guard<std::mutex> lock(timeObjectsMutex);
            if (!timeObjects.empty()) {
                delay = timeObjects.front().runTime - get_time_double();
                if (delay < 0) {
                    shouldRunFirst = true;
                }
            }
        }
        if (shouldRunFirst) {
            checkAndRunFirst();
            continue;
        }

        std::string timerMessage, signalMessage;
        
        // Convert delay from seconds to std::chrono::microseconds
        std::chrono::microseconds waitDuration = (delay > 0) ? std::chrono::microseconds(static_cast<int>(delay * 1000000.0)) : std::chrono::microseconds::max();

        // First, we'll check for timer-related messages with the specified delay.
        bool gotTimerMsg = timerChannel.receive(timerMessage, waitDuration);

        // Immediately after, we'll check for signals without waiting.
        bool gotSignalMsg = signalChannel.receive(signalMessage, std::chrono::microseconds(0));

        // Process messages
        if (gotTimerMsg) {
            std::cout << " got timer message :" << timerMessage << " at time "<< get_time_double()<< std::endl;
            // Handle timer channel message
        }

        if (gotSignalMsg) {
            std::cout << " got signal message :" << signalMessage << " at time "<< get_time_double()<< std::endl;
            //std::cout << " got signal message" << std::endl;
            // Handle signal channel message
        }

        // If no messages were received and no timer was triggered, perform a default action or continue the loop.
        if (!gotTimerMsg && !gotSignalMsg && delay <= 0) {
            // Handle no message case (if needed)
            continue;
        }
    }
}



int test_timer() {

    std::cout << __func__ << " Running"<< std::endl;

    // std::thread timerThread(timerThreadFunc);
    // timerThread.detach();

    double start = get_time_double();

    int exampleParam = 42;
    int exampleParam1 = 11;
    int exampleParam2 = 12;
    int exampleParam3 = 13;

    TimeObject obj1("obj1",15.0, 0, 0.1, 1, exampleCallback, &exampleParam1);
    TimeObject obj2("obj2",13.0, 0, 0, 1, exampleCallback, &exampleParam2);
    TimeObject obj3("obj3",17.0, 0, 0, 1, exampleCallback, &exampleParam3);

    addTimeObject(obj1);
    addTimeObject(obj2);
    addTimeObject(obj3);

    showTimerObjects();


    TimeObject obj4("obj4", 19.0, 0, 0, 1, exampleCallback, &exampleParam);
    addTimeObject(obj4);
    modifyTimeObjectByName("obj4", 2.0, 0, 0);
    //removeTimeObjectByName("obj4");
    // Continually check and run events (this is a simple example - in a real
    // application you'd want to be more efficient than checking constantly in a loop)
    // while (!timeObjects.empty()) {
    //     checkAndRunFirst();
    // }

    std::thread timerThread(timerThreadFunc);
    
    // Simulate some external activity
    std::this_thread::sleep_for(std::chrono::seconds(2));
    signalChannel.send("External Signal 1");

    std::this_thread::sleep_for(std::chrono::seconds(6));
    timerChannel.send("Timer Message 1");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    signalChannel.send("External Signal 2");

    // Let's wait for a while to observe the behavior
    std::this_thread::sleep_for(std::chrono::seconds(20));

    shouldTerminate = true;
    // Cleanup
    timerThread.join(); // In a real-world scenario, we'd have a mechanism to signal the timer thread to exit and then join.

//    delete static_cast<std::string*>(obj1.param);
//    delete static_cast<std::string*>(obj2.param);
//    delete static_cast<std::string*>(obj3.param);
    showTimerObjects();
    double end = get_time_double();

    std::cout << " duration secs :" << (end - start)  << std::endl;
 
    return 0;
}



int main() {
    baseTime = std::chrono::system_clock::now();
    return test_timer();
}