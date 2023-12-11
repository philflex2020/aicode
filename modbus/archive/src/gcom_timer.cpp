// gcom_timer.cpp
// p. wilshire
// s .reynolds
// 11_08_2023
// self review 11_22_2023

#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <set>

#include "gcom_timer.h"


std::mutex timeObjectsMutex;

std::vector<std::shared_ptr<TimeObject>> timeObjects;


// code to use a list...
std::list<std::shared_ptr<TimeObject>> timeObjectList;


// Custom comparator used in a set
struct CompareTimerSet {
    bool operator()(const std::shared_ptr<TimeObject>& a, const std::shared_ptr<TimeObject>& b) const {
        return a->runTime < b->runTime;
    }
};

std::set<std::shared_ptr<TimeObject>, CompareTimerSet> timerSet;

std::mutex timerMutex;
std::condition_variable timerCv;
bool stopThread = false;


void _addTimeObjToSet(std::shared_ptr<TimeObject> timeObj){
    timerSet.insert(timeObj);
    timerCv.notify_one();
}

void addTimeObjToSet(std::shared_ptr<TimeObject> timeObj, double offset, bool isSync) {
    std::lock_guard<std::mutex> lock(timerMutex);
    _addTimeObjToSet(timeObj);
}

std::shared_ptr<TimeObject> findTimerSetByName(const std::set<std::shared_ptr<TimeObject>, CompareTimerSet>& timerSet, const std::string& name) {
    std::lock_guard<std::mutex> lock(timerMutex);
    auto it = std::find_if(timerSet.begin(), timerSet.end(),
                           [&name](const std::shared_ptr<TimeObject>& obj) {
                               return obj->name == name;
                           });

    if (it != timerSet.end()) {
        return *it;
    } else {
        return nullptr; // or throw an exception, or handle the not-found case as needed
    }
}
// timerThreadFunc
//checkandrunfirst
// if (first->runTime <= currentTime) {
//         // Check if it's not a sync timer or if it's a sync timer that hasn't run yet.
//         if (!first->sync || (first->sync && !first->run)) {
//             first->lastRun = currentTime;
//             first->runs++;
//             first->run = true;

        // if (first->sync) {
        // if (first->run) {
            
        //     if (first->lastRun + (first->repeatTime*2) <= currentTime) {
        //         first->run = false;                
        //     }

void timerSetThreadFunc() {
    std::cout <<__func__ << " running 1" << std::endl;
    std::unique_lock<std::mutex> lock(timerMutex);
    std::cout <<__func__ << " running 2" << std::endl;
    while (!stopThread) {
        if (!timerSet.empty()) {
            auto tNow = get_time_double();
            auto now = std::chrono::system_clock::now();
            //auto nowMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            //double nowSeconds = nowMicroseconds / 1e6;  // Convert microseconds to seconds
            //auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now).time_since_epoch().count();
            // std::chrono::duration_cast<std::chrono::seconds>(
            //                std::chrono::system_clock::now().time_since_epoch())
            //                .count();
            auto tObj = *timerSet.begin();
            double waitTime = tObj->runTime - tNow; 
            if(0)std::cout << " fOUND " << tObj->name 
                            << " runTime " << tObj->runTime 
                            << " waitTime " << waitTime 
                            << std::endl;

            if (waitTime <= 0) {
                if (1 ||!tObj->sync || (tObj->sync && !tObj->run)) {
                    tObj->lastRun = tNow;
                    tObj->runs++;
                    tObj->run = true;

                    // Run the callback
                    auto callback = tObj->callback;
                    auto name = tObj->name;
                    auto interval = tObj->repeatTime;
                    // reset sync if we need to 
                    if (tObj->sync) {
                        if (tObj->run) {
                            if (tObj->lastRun + (tObj->repeatTime*2) <= tNow) {
                                tObj->run = false;                
                            }
                        }
                    }
                    timerSet.erase(timerSet.begin());

                    tObj->runTime += interval;
                    if (tObj->sync)
                        tObj->runTime += interval;

                    lock.unlock(); // Unlock while running the callback
                    callback(tObj, tObj->param);
                    lock.lock(); // Re-lock to insert back
                    if((interval> 0.0) 
                       && (tObj->stopTime == 0 || tObj->runTime < tObj->stopTime)
                       && (tObj->count == 0 || tObj->runs < tObj->count)
                       )
                    {
                        if(0)std::cout << " Adding back " << tObj->name 
                            << " runTime " << tObj->runTime 
                            << std::endl;
                        timerSet.insert(tObj);
                    }
                    else
                    {
                        std::cout << " Dropping " << tObj->name 
                            << " tNow " << tNow 
                            << " interval " << interval 
                            << " count " << tObj->count 
                            << " runs " << tObj->runs 
                            << " runTime " << tObj->runTime 
                            << " stopTime " << tObj->stopTime 
                            << std::endl;

                    }
                }
            } else {
                timerCv.wait_until(lock, now + std::chrono::duration<double>(waitTime));
                //timerCv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::seconds(static_cast<long>(nextTimer.runtime - now)));
            }
        } else {
            timerCv.wait(lock);
        }
    }
    std::cout <<__func__ << " stopping " << std::endl;

}

// older timeObjectList functions

void _addTimeObject(std::shared_ptr<TimeObject> obj) {
    
    // Insert object in sorted position.
    auto it = std::upper_bound(timeObjectList.begin(), timeObjectList.end(), obj, 
        [](const std::shared_ptr<TimeObject>& a, const std::shared_ptr<TimeObject>& b) {
            return a->runTime < b->runTime;
        }
    );
    timeObjectList.insert(it, obj);
}

void addTimeObject(std::shared_ptr<TimeObject> obj) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    _addTimeObject(obj);

}

std::shared_ptr<TimeObject> getFirst() {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    if (timeObjectList.empty()) {
        return nullptr;
    }
    return timeObjectList.front();
}


void _modifyAndRelocate(std::shared_ptr<TimeObject> obj, double newRunTime) {
    obj->runTime = newRunTime;
    timeObjectList.remove(obj);
    _addTimeObject(obj);
}

void modifyAndRelocate(std::shared_ptr<TimeObject> obj, double newRunTime) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    return _modifyAndRelocate(obj, newRunTime);

}



std::shared_ptr<TimeObject> findByName(const std::string& nameToFind) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::find_if(timeObjectList.begin(), timeObjectList.end(), 
        [&nameToFind](const std::shared_ptr<TimeObject>& obj) {
            return obj->name == nameToFind;
        }
    );

    if (it != timeObjectList.end()) {
        return *it; // Found the object
    } else {
        return nullptr; // Object with the given name not found
    }
}


//std::vector<TimeObject> timeObjects;


Channel<int> timerChannel;
Channel<std::string> signalChannel;
std::condition_variable timerCV;
std::atomic<bool> shouldTerminate;

bool timeInit = false;
std::mutex timeDoubleMutex;
std::chrono::system_clock::time_point baseTime;

double get_time_double() {
    std::lock_guard<std::mutex> lock(timeDoubleMutex);
    if (!timeInit)
        baseTime = std::chrono::system_clock::now();
    timeInit =  true;
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = currentTime - baseTime;
    return duration.count();
}

double get_basetime_double() {
    std::lock_guard<std::mutex> lock(timeDoubleMutex);
    if (!timeInit)
        baseTime = std::chrono::system_clock::now();
    timeInit =  true;
    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = currentTime - baseTime;
    return duration.count();
}

//We can use get_time_double() for approximately 285 million years before you lose microsecond resolution.
//We  are safe using double to represent time with microsecond resolution for any foreseeable application!


void setTimeObject(const std::shared_ptr<TimeObject> newObj, double offset, bool isSync)
{
    newObj->offsetTime = offset;
    newObj->sync = isSync;

    auto origRunTime = newObj->runTime;
    auto tNow = get_time_double();
    if (newObj->repeatTime > 0 && tNow >= newObj->runTime) {
        // no matter when we request a timer, they are always sync'd to the same time 
        double nextRepeatTime = std::ceil(tNow / newObj->repeatTime ) * newObj->repeatTime;
        // Calculate the new run time based on repeated intervals and the offset
        double nextRunTime = nextRepeatTime + newObj->offsetTime;

        // update newObj.runTime
        newObj->runTime = nextRunTime;
    } else {
        // Only respect the initial runTime
        newObj->runTime = origRunTime + newObj->offsetTime;
    }

}
void addTimeObject(const std::shared_ptr<TimeObject> newObj, double offset, bool isSync, bool useSet) {
    //std::cout << __func__ << " check 1" << std::endl;
    // Use the helper function to insert the object in the correct position
    //std::cout << __func__ << " check 2" << std::endl;
    if (useSet)
    {
        std::lock_guard<std::mutex> lock(timerMutex);
        setTimeObject(newObj, offset, isSync);
        return _addTimeObjToSet(newObj);
    }

    else
    {
        std::lock_guard<std::mutex> lock(timeObjectsMutex);
        setTimeObject(newObj, offset, isSync);
        _addTimeObject(newObj);
    }
    //std::cout << __func__ << " check 3" << std::endl;

    timerChannel.send(timer::RECALIBRATE);
    //std::cout << __func__ << " check 4" << std::endl;
}

// void addTimeObject(const std::shared_ptr<TimeObject> newObj, double offset, bool isSync) {

// //void addTimeObject(const TimeObject& obj, double offset) {
//     std::lock_guard<std::mutex> lock(timeObjectsMutex);

//     newObj->offsetTime = offset;
//     newObj->sync = isSync;


//     auto origRunTime = newObj->runTime;
//     auto tNow = get_time_double();
//     if (newObj->repeatTime> 0 && tNow >= newObj->runTime) {
//         // no matter when we request a timer they are always sync'd to the same time 

//         double nextRepeatTime = std::ceil(tNow / newObj->repeatTime ) * newObj->repeatTime;
//         // Calculate the new run time based on repeated intervals and the offset
//         double nextRunTime = nextRepeatTime + newObj->offsetTime;

//         // update newObj.runTime
//         newObj->runTime = nextRunTime;
//     } else {
//         // Only respect the initial runTime
//         newObj->runTime = origRunTime;
//     }

//     // Add the newObj to the vector and sort
//     timeObjects.push_back(newObj);


//     std::sort(timeObjects.begin(), timeObjects.end(), 
//         [](const std::shared_ptr<TimeObject>& a, const std::shared_ptr<TimeObject>& b) {
//         return a->runTime < b->runTime;
//     });

//     timerChannel.send(timer::RECALIBRATE);
// }


bool shouldStopDuetoTime(const std::shared_ptr<TimeObject>& obj)
{ 
    if (!obj) {
        throw std::invalid_argument("The provided shared_ptr is nullptr.");
    }

    return obj->stopTime != 0 && obj->runTime >= obj->stopTime;
}

// do we have a stop count specified and have we run that many times
bool shouldStopDuetoCount(const std::shared_ptr<TimeObject>& obj)
{ 
    if (!obj) {
        std::cout << __func__ << " this is not a time obj"<<std::endl;
        throw std::invalid_argument("The provided shared_ptr is nullptr.");
    }

    return obj->count != 0 && obj->runs > obj->count;
}



// Helper function that updates runTime and relocates the time object WITHOUT locking the mutex.

void checkAndRunFirst() {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    
    if (timeObjectList.empty()) return;

    double currentTime = get_time_double();
    std::shared_ptr<TimeObject>& first = timeObjectList.front();

    if (first->runTime <= currentTime) {
        // Check if it's not a sync timer or if it's a sync timer that hasn't run yet.
        if (!first->sync || (first->sync && !first->run)) {
            first->lastRun = currentTime;
            first->runs++;
            first->run = true;

            //printf(" timer obj ... %p  ", (void*)first.get());
            //std::cout << " Timer running callback for name [" << first->name << "] " << std::endl;
            //std::cout << "               for param [" << first->param << "] " << std::endl;
            first->callback(first, first->param);  // still pass raw pointer to callback

            // Check if it's a repeating timer and shouldn't be stopped due to time or count.
            if (first->repeatTime > 0 && !shouldStopDuetoTime(first) && !shouldStopDuetoCount(first)) {
                _modifyAndRelocate(first, first->runTime + first->repeatTime); // update the runTime and reposition in the list without locking
            } else {
                timeObjectList.pop_front(); // Remove the timer object if it doesn't repeat or should be stopped.
            }
        }
        // allow the rest to run
        // we may resync if    first->lastRun + (first->repeatTime*2) < currentTime;

        if (first->sync) {
        if (first->run) {
            
            if (first->lastRun + (first->repeatTime*2) <= currentTime) {
                first->run = false;                
            }

            if (first->repeatTime > 0 && !shouldStopDuetoTime(first) && !shouldStopDuetoCount(first)) {
                _modifyAndRelocate(first, first->runTime + first->repeatTime); // update the runTime and reposition in the list without locking
            } else {
                timeObjectList.pop_front(); // Remove the timer object if it doesn't repeat or should be stopped.
            }
        }
        }
        timerChannel.send(timer::RECALIBRATE);
    }
}


void exampleCallback(std::shared_ptr<TimeObject> t, void* p) {
    double rtime = get_time_double();
    std::cout << "Callback for :" << t->name <<" executed at : " << rtime << " with param: " << *((int*)p) << std::endl;
}

std::shared_ptr<TimeObject> findTimeObjectByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    auto it = std::find_if(timeObjects.begin(), timeObjects.end(),
                           [&name](const std::shared_ptr<TimeObject>& obj) { return obj->name == name; });
    return (it != timeObjects.end()) ? *it : nullptr;
}

bool removeTimeObjectByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(timeObjectsMutex);
    
    // Use find_if to locate the named object
    auto it = std::find_if(timeObjectList.begin(), timeObjectList.end(), 
                           [&name](const std::shared_ptr<TimeObject>& obj) { return obj->name == name; });
    
    // If found, erase it
    if (it != timeObjectList.end()) {
        timeObjectList.erase(it);
        timerChannel.send(timer::RECALIBRATE);
        return true;
    }
    return false;
}

// TODO useSet
//std::shared_ptr<TimeObject> findTimerSetByName(const std::set<std::shared_ptr<TimeObject>, CompareTimerSet>& timerSet, const std::string& name) {
//    std::lock_guard<std::mutex> lock(timerMutex);

bool syncTimeObjectByName(const std::string& name, double error_margin_percent, bool useSet) {
    double tNow = get_time_double();

    if (useSet)
    {
        auto tObj = findTimerSetByName(timerSet, name);
        std::lock_guard<std::mutex> lock(timerMutex);
        if (tObj && tObj->sync)
        {
            timerSet.erase(tObj);
            if ((tObj->runTime - tNow) < (tObj->repeatTime * error_margin_percent)) {
                tObj->runTime = tNow + tObj->repeatTime;
            }
            tObj->run = false;
            timerSet.insert(tObj);
            return true;

        }
        return false;
    }
    else
    {
        std::lock_guard<std::mutex> lock(timeObjectsMutex);
        auto it = std::find_if(timeObjectList.begin(), timeObjectList.end(),
                            [&name](const std::shared_ptr<TimeObject>& obj) { return obj->name == name; });
        if (it != timeObjectList.end()) {
            
            // if we are before the runTime, (runtime-tNow) is > 0
            // if we are after the runTime, (runtime-tNow) is < 0
            if (((*it)->runTime - tNow) < ((*it)->repeatTime * error_margin_percent)) {
                (*it)->runTime = tNow + (*it)->repeatTime;
            }
            if((*it)->sync)
                (*it)->run = false;

            // Relocate the modified object.
            _modifyAndRelocate(*it, (*it)->runTime);

            timerChannel.send(timer::RECALIBRATE);
            return true;
        }
        return false;
    }
}

bool modifyTimeObjectByName(const std::string& name, double newRunTime, double newStopTime, double newRepeatTime, bool useSet) {
    if (useSet)
    {
       
        auto tObj = findTimerSetByName(timerSet, name);
        if (tObj)
        {
            std::lock_guard<std::mutex> lock(timerMutex);
            timerSet.erase(tObj);

            if(newRunTime > 0) tObj->runTime = newRunTime;
            if(newStopTime > 0 ) tObj->stopTime = newStopTime;
            if(newRepeatTime > 0) tObj->repeatTime = newRepeatTime;
            if((tObj->repeatTime > 0.0) && (tObj->stopTime != 0 && tObj->runTime < tObj->stopTime))
                    timerSet.insert(tObj);
            return true;

        }
        return false;
    }
    else
    {
        std::lock_guard<std::mutex> lock(timeObjectsMutex);
        auto it = std::find_if(timeObjectList.begin(), timeObjectList.end(), 
                            [&name](const std::shared_ptr<TimeObject>& obj) {
            return obj->name == name;
        });
        

        if (it != timeObjectList.end()) {
            if(newRunTime > 0) (*it)->runTime = newRunTime;
            if(newStopTime > 0 ) (*it)->stopTime = newStopTime;
            if(newRepeatTime > 0) (*it)->repeatTime = newRepeatTime;
            
            // Relocate the modified object.
            _modifyAndRelocate(*it, (*it)->runTime);

            timerChannel.send(timer::RECALIBRATE);
            return true;
        }
        return false;
    }
}

void showTimerObjects(bool useSet) {
    if (useSet)
    {
        std::lock_guard<std::mutex> lock(timerMutex);

        for (const auto& tPtr : timerSet) {
            std::cout << tPtr->name 
                  << " runTime :"    << tPtr->runTime 
                  << " repeatTime :" << tPtr->repeatTime 
                  << " runs :"       << tPtr->runs 
                  << " lastRun :"    << tPtr->lastRun 
                  << std::endl;

        }
    }
    else
    {
        std::lock_guard<std::mutex> lock(timeObjectsMutex);

        for (const auto& tPtr : timeObjectList) {
            std::cout << tPtr->name 
                  << " runTime :" << tPtr->runTime 
                  << " runs :" << tPtr->runs 
                  << " lastRun :" << tPtr->lastRun 
                  << std::endl;
        }
    }
}




void timerThreadFunc() {
    shouldTerminate = false;

    while (!shouldTerminate) {
        double delay = -1;
        bool shouldRunFirst = false; // flag to determine if we need to run the first timer

        {
            std::lock_guard<std::mutex> lock(timeObjectsMutex);
            if (!timeObjectList.empty()) {
                delay = timeObjectList.front()->runTime - get_time_double();
                if (delay < 0) {
                    shouldRunFirst = true;
                }
            }
        }

        if (shouldRunFirst) {
            checkAndRunFirst();
            continue;
        }

        int timerMessage = 0;
        std::string signalMessage;

        // Convert delay from seconds to std::chrono::microseconds
        std::chrono::microseconds waitDuration = (delay > 0) ? std::chrono::microseconds(static_cast<int>(delay * 1000000.0)) : std::chrono::microseconds::max();

        // First, we'll check for timer-related messages with the specified delay.
        bool gotTimerMsg = timerChannel.receive(timerMessage, waitDuration);

        // Immediately after, we'll check for signals without waiting.
        bool gotSignalMsg = signalChannel.receive(signalMessage, std::chrono::microseconds(0));

        // Process messages
        if (gotTimerMsg) {
            if (timerMessage != timer::RECALIBRATE)
                std::cout << " got timer message :" << timerMessage << " at time " << get_time_double() << std::endl;
            // Handle timer channel message
        }

        if (gotSignalMsg) {
            std::cout << " got signal message :" << signalMessage << " at time " << get_time_double() << std::endl;
            // Handle signal channel message
        }

        // If no messages were received and no timer was triggered, perform a default action or continue the loop.
        if (!gotTimerMsg && !gotSignalMsg && delay <= 0) {
            // Handle no message case (if needed)
            continue;
        }
    }
}



std::thread timerThread;
    
void runTimer()
{
    timerThread = std::thread(timerThreadFunc);
}

void stopTimer()
{
    shouldTerminate = true;
    timerChannel.send(3456);
    // Cleanup
    timerThread.join(); // In a real-world scenario, we'd have a mechanism to signal the timer thread to exit and then join.

}

void setBaseTime()
{
    baseTime = std::chrono::system_clock::now();
}

std::shared_ptr<TimeObject> createTimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, 
    double timer_repeatTime, int timer_count, void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams)
{
    double offset = 0.0;
    auto tobj = std::make_shared<TimeObject>(timer_name, timer_runTime, timer_stopTime,
         timer_repeatTime, timer_count, offset, timer_callback, timer_callbackParams);
    timeObjects.emplace_back(tobj);

    return tobj;
}

std::shared_ptr<TimeObject> createOffsetTimeObject(std::string timer_name, double timer_runTime, double timer_stopTime, 
         double timer_repeatTime, int timer_count, int timer_offset, void (*timer_callback)(std::shared_ptr<TimeObject>,void*), void* timer_callbackParams)
{
    auto tobj = std::make_shared<TimeObject>(timer_name, timer_runTime, timer_stopTime, 
            timer_repeatTime, timer_count, timer_offset, timer_callback, timer_callbackParams);
    timeObjects.emplace_back(tobj);
    return tobj;
}




int test_timer(bool useSet = false) 
{
    setBaseTime();
    std::cout << __func__ << " Running "<< (useSet?"Using SetList":"using origList")<< std::endl;

    double start = get_time_double();

    int exampleParam = 42;
    int exampleParam1 = 11;
    int exampleParam2 = 12;
    int exampleParam3 = 13;
    int exampleParam5 = 15;
    int exampleParam6 = 16;

    auto obj1 = std::make_shared<TimeObject>("obj1", 15.0,   0,    0.1,  0,   exampleCallback, &exampleParam1);
    auto obj2 = std::make_shared<TimeObject>("obj2", 1.0,   0,    1.0,    0,   exampleCallback, &exampleParam2);
    auto obj3 = std::make_shared<TimeObject>("obj3", 10.0,   0,    0.001,    10,   exampleCallback, &exampleParam3);
    auto obj5 = std::make_shared<TimeObject>("obj5", 1.0,   0,    0.2,    0,   exampleCallback, &exampleParam5);
    auto obj6 = std::make_shared<TimeObject>("obj6", 1.0,   0,    0.2,    0,   exampleCallback, &exampleParam6);

    addTimeObject(obj1, 0.0, false, useSet);
    addTimeObject(obj2, 0.0, true, useSet);
    addTimeObject(obj3, 0.01, false, useSet);

    showTimerObjects(useSet);

    auto obj4 = std::make_shared<TimeObject>("obj4", 19.0,   0,  0, 1, exampleCallback, &exampleParam);
    addTimeObject(obj4,0.0, false, useSet);
    modifyTimeObjectByName("obj4", 2.0, 0, 0, useSet);
    showTimerObjects(useSet);

    std::thread timerThread;
    if (useSet)
        timerThread = std::thread(timerSetThreadFunc);
    else
        timerThread = std::thread(timerThreadFunc);

    // Simulate some external activity
    std::this_thread::sleep_for(std::chrono::seconds(4));
    if (!useSet)
        signalChannel.send("External Signal 1");

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    if (!useSet)
        timerChannel.send(3456);
    std::cout << " syncing obj2 now after 4.6 seconds" << std::endl;
    syncTimeObjectByName("obj2",0.4,useSet);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (!useSet)
        signalChannel.send("External Signal 2");

    // Let's wait for a while to observe the behavior
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "adding obj5" << std::endl;
    addTimeObject(obj5, 0.0, false, useSet);

    std::cout << "sleeping for 5 secs" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "killing threads" << std::endl;
    shouldTerminate = true;
    stopThread = true;
    timerCv.notify_one();
    //std::cout << "killed threads" << std::endl;
    //addTimeObject(obj6, 0.0, false, useSet);
    // Cleanup

    timerThread.join(); // In a real-world scenario, we'd have a mechanism to signal the timer thread to exit and then join.

    showTimerObjects(useSet);
    double end = get_time_double();

    std::cout << " duration secs :" << (end - start)  << std::endl;

    return 0;
}


#ifdef MYTEST

int main() {
     return test_timer();
}
#endif

