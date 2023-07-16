#pragma once
// TimeTracker
// p wilshire
// 07_15_2023

#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <vector>
#include <chrono>
#include <algorithm>

#ifndef LONG_LONG_MAX
#define LONG_LONG_MAX std::numeric_limits<long long int>::max()
#endif

using namespace std;
using namespace std::chrono;

class TimeTracker {
private:
    string name;
    long long minInterval;
    long long maxInterval;
    long long totalInterval;
    int sampleCount;

public:
    TimeTracker(const string &name) : name(name) {
        minInterval = LONG_LONG_MAX;
        maxInterval = 0;
        totalInterval = 0;
        sampleCount = 0;
    };
    TimeTracker() : name("name") {
        minInterval = LONG_LONG_MAX;
        maxInterval = 0;
        totalInterval = 0;
        sampleCount = 0;
    };

    void setName(const string&iname) {
        name=iname;
    };

    void addInterval(const chrono::nanoseconds &interval) {
        long long duration = interval.count();
        totalInterval += duration;
        sampleCount++;
        minInterval = min(minInterval, duration);
        maxInterval = max(maxInterval, duration);
    }

    void clearIntervals() {
        minInterval = LONG_LONG_MAX;
        maxInterval = 0;
        totalInterval = 0;
        sampleCount = 0;
    }

    double getAverageInterval() {
        if (sampleCount == 0) {
            return 0;
        }
        return static_cast<double>(totalInterval) / sampleCount;
    }

    long long getMinInterval() {
        return minInterval;
    }

    long long getMaxInterval() {
        return maxInterval;
    }

    int getSampleCount() {
        return sampleCount;
    }

    std::string toJsonStr(const char *iname) {
        std::stringstream ss;
        ss <<"\"" <<iname << "\":{";
        ss << "\"min\":"<< (float)(minInterval/1000000.0)<< ",";
        ss << "\"max\":"<< (float)(maxInterval/1000000.0)<< ",";
        ss << "\"avg\":"<< (float)(getAverageInterval()/1000000.0)<< ",";
        ss << "\"count\":"<< sampleCount<< "}";
        return ss.str();
    }
   std::string toJsonStr() {
        std::stringstream ss;
        ss <<"\"" <<name << "\":{";
        ss << "\"min\":"<< minInterval<< ",";
        ss << "\"max\":"<< maxInterval<< ",";
        ss << "\"avg\":"<< getAverageInterval()<< ",";
        ss << "\"count\":"<< sampleCount<< "}";
        return ss.str();
    }
};