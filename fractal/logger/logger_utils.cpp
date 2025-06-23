#include "Logger.h"

double ref_time_dbl_wall() {
    using Clock = std::chrono::system_clock;
    auto now = Clock::now();
    std::chrono::duration<double> timestamp = now.time_since_epoch();
    return timestamp.count();  // seconds since epoch
}

double ref_time_dbl() {
    using Clock = std::chrono::steady_clock;
    static const auto start_time = Clock::now();
    auto now = Clock::now();
    std::chrono::duration<double> elapsed = now - start_time;
    return elapsed.count();  // seconds with sub-second resolution
}

uint64_t ref_time_us() {
    return static_cast<uint64_t>(ref_time_dbl() * 1'000'000);
}
