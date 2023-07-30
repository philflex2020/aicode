//src/time_tracker.cpp:

#include "time_tracker.h"
#include <algorithm>

TimeTracker::TimeTracker() {}

void TimeTracker::recordSample(const std::string& name, unsigned long long time) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = std::find_if(samples_.begin(), samples_.end(),
        [&name](const auto& pair) { return pair.first == name; });

    if (it == samples_.end()) {
        samples_.emplace_back(name, SampleData{ time, time, static_cast<double>(time), 1 });
    } else {
        it->second.max_time = std::max(it->second.max_time, time);
        it->second.min_time = std::min(it->second.min_time, time);
        it->second.avg_time = ((it->second.avg_time * it->second.count) + time) / (it->second.count + 1);
        it->second.count++;
    }

    // Send the sample to the TimeStore for storage.
    time_store_.storeSample(name, time);
}

std::string TimeTracker::getJsonOutput(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = std::find_if(samples_.begin(), samples_.end(),
        [&name](const auto& pair) { return pair.first == name; });

    if (it == samples_.end()) {
        return "{}";
    } else {
        const SampleData& data = it->second;
        return "{\"name\": \"" + it->first + "\", \"max\": " + std::to_string(data.max_time) +
               ", \"min\": " + std::to_string(data.min_time) + ", \"avg\": " + std::to_string(data.avg_time) + "}";
    }
}