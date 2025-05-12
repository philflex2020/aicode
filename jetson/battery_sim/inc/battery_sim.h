// battery_sim.h
#pragma once

#include <stdint.h>

#define NUM_RACKS 1
#define NUM_MODULES 10
#define NUM_CELLS 32

struct CellData {
    float voltage;
    float current;
    float temperature;
    float soc;       // State of Charge
    float resistance;
};

struct ModuleData {
    CellData cells[NUM_CELLS];
};

struct RackData {
    ModuleData modules[NUM_MODULES];
};

struct BatterySim {
    RackData racks[NUM_RACKS];
};

// global pointer (assume mapped externally to shared memory)
extern BatterySim* g_battery_sim;
