//include/time_tracker.h:

#ifndef TIME_TRACKER_H
#define TIME_TRACKER_H

#include <string>
#include <vector>
#include <mutex>
#include "time_store.h"

class TimeTracker {
public:
    TimeTracker();
    void recordSample(const std::string& name, unsigned long long time);
    std::string getJsonOutput(const std::string& name) const;

private:
    TimeStore time_store_;
    mutable std::mutex mtx_;
    std::vector<std::pair<std::string, SampleData>> samples_;
};

#endif // TIME_TRACKER_H