#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <queue>
#include <mutex>
#include <condition_variable>

// Define the TimeObject class
class TimeObject {
public:
    std::string name;
    double runTime;
    double stopTime;
    double repeatTime;
    int count;
    bool enabled;
    bool oneshot;
    void (*callback)(TimeObject*,void*);
    void* param;

    TimeObject(std::string n, double rt, double st, double rpt, int c, void (*cb)(TimeObject*,void*), void* p)
        : name(n), runTime(rt), stopTime(st), repeatTime(rpt), count(c), callback(cb), param(p) {
            enabled = true;
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