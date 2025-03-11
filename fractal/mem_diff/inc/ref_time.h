#ifndef REF_TIME_H
#define REF_TIME_H

#include <chrono>

inline double ref_time_dbl() {
    using namespace std::chrono;
    return duration<double>(high_resolution_clock::now().time_since_epoch()).count();
}

#endif // REF_TIME_H