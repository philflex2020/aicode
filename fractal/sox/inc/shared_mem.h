#pragma once
// shared_mem.h
#include <cstdint>

constexpr int NUM_CELLS = 480;

// Pack data in millivolts, milli-°C, milli-%SOC for precision
struct SharedCell {
    uint16_t voltage_mV;    // e.g. 3123 for 3.123 V
    uint16_t est_soc_d1;    // 0–1000 (one decimal place, e.g. 753 = 75.3%)
    uint16_t true_soc_d1;   // same as above
    uint16_t temp_dC;       // e.g. 253 = 25.3 °C
};

struct SharedStats {
    uint16_t total_voltage_dV; // e.g. 158052 = 1580.52 V
    int16_t  pack_current_dA;  // signed: -123 = -12.3A
    uint16_t min_voltage_mV;
    uint16_t max_voltage_mV;
    uint16_t avg_voltage_mV;
    uint16_t min_temp_dC;
    uint16_t max_temp_dC;
    uint16_t avg_temp_dC;
    uint16_t min_soc_d1;
    uint16_t max_soc_d1;
    uint16_t avg_soc_d1;
};

struct SharedMemoryLayout {
    SharedStats stats;
    SharedCell cells[NUM_CELLS];
};
