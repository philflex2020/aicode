

Got it! Let’s **clean up and restructure** this code to make it:

✅ **Organized** into logical sections
✅ **Consistent** with naming and formatting
✅ Easier to maintain with shared memory, simulation, and HTTP server clearly separated

I’ll preserve **all existing functionality** but refactor it for readability.

---

## ✅ Cleaned & Organized Version

```cpp
// =====================================================
// sox_ws.cpp - Battery Cell Simulation + Shared Memory + Web Server
//
// Compile:
// g++ -std=c++17 -o sox_ws -I . sox_ws.cpp -pthread
//
// This simulates 480 LiFePO4 cells with random IR/capacity variations,
// runs a SOC Kalman filter, stores results in shared memory,
// and serves JSON data over HTTP.
//
// =====================================================

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>

#include <httplib.h>            // lightweight header-only HTTP server
#include <nlohmann/json.hpp>    // JSON library
using json = nlohmann::json;

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared_mem.h"

// =====================================================
// Global Constants
constexpr int NUM_CELLS = 480;
constexpr double NOMINAL_CAPACITY_AH = 280.0;
constexpr double NOMINAL_IR = 0.0005;     // 0.5 mΩ
constexpr double SIM_DT_S = 0.1;          // 100 ms simulation timestep

// =====================================================
// Shared Memory Handling
SharedMemoryLayout *shm_ptr = nullptr;

void init_shared_memory() {
    int fd = shm_open("/sox_cells", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open failed");
        exit(1);
    }
    if (ftruncate(fd, sizeof(SharedMemoryLayout)) < 0) {
        perror("ftruncate failed");
        exit(1);
    }
    shm_ptr = (SharedMemoryLayout *)mmap(
        nullptr,
        sizeof(SharedMemoryLayout),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );
    if (shm_ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    close(fd);
}

// =====================================================
// LiFePO4 OCV vs SOC Lookup Table
static std::vector<double> soc_table;
static std::vector<double> ocv_table;

void init_ocv_table() {
    soc_table.resize(101);
    ocv_table.resize(101);

    for (int i = 0; i <= 100; i++) {
        double soc = i / 100.0;
        soc_table[i] = soc;

        // Approx LiFePO4 curve: flat plateau around 3.29V with slopes near 0% & 100%
        double ocv = 3.29 + 0.05 * (soc - 0.5)
                   - 0.15 * exp(-20 * soc)
                   + 0.1  * exp(-20 * (1 - soc));

        ocv = std::clamp(ocv, 2.5, 3.4);
        ocv_table[i] = ocv;
    }
}

double lookup_ocv(double soc) {
    if (soc <= 0.0) return ocv_table.front();
    if (soc >= 1.0) return ocv_table.back();

    double idx_f = soc * 100.0;
    int idx = (int)floor(idx_f);
    double frac = idx_f - idx;
    return ocv_table[idx] + frac * (ocv_table[idx+1] - ocv_table[idx]);
}

// =====================================================
// Timing Helper
double ref_time_dbl() {
    using clock = std::chrono::steady_clock;
    static auto t0 = clock::now();
    auto now = clock::now();
    std::chrono::duration<double> elapsed = now - t0;
    return elapsed.count();
}

// =====================================================
// Kalman Filter for SOC + voltage bias
struct SOC_Kalman {
    double soc;
    double v_bias;
    double P[2][2];
    double Q_soc, Q_bias, R_meas;

    SOC_Kalman() {
        soc = 0.5;
        v_bias = 0.0;
        P[0][0] = 0.01;   // SOC variance
        P[0][1] = 0.0;
        P[1][0] = 0.0;
        P[1][1] = 0.001;  // bias variance

        Q_soc  = 1e-6;
        Q_bias = 1e-7;
        R_meas = 0.002;   // ~2mV noise
    }

    void predict(double dAh, double capacity_Ah) {
        double soc_delta = -dAh / capacity_Ah;
        soc = std::clamp(soc + soc_delta, 0.0, 1.0);

        // Covariance grows
        P[0][0] += Q_soc;
        P[1][1] += Q_bias;
    }

    void update(double measured_voltage, double current_A, double ir_drop) {
        double expected_voltage = lookup_ocv(soc) - ir_drop + v_bias;
        double residual = measured_voltage - expected_voltage;

        double dOCV_dSOC =
            (lookup_ocv(std::min(soc+0.01,1.0)) -
             lookup_ocv(std::max(soc-0.01,0.0))) / 0.02;

        double H[2] = { -dOCV_dSOC, 1.0 };

        // S = H * P * H^T + R
        double S = H[0]*(P[0][0]*H[0] + P[0][1]*H[1]) +
                   H[1]*(P[1][0]*H[0] + P[1][1]*H[1]) + R_meas;

        double K0 = (P[0][0]*H[0] + P[0][1]*H[1]) / S;
        double K1 = (P[1][0]*H[0] + P[1][1]*H[1]) / S;

        // Update state
        soc = std::clamp(soc + K0 * residual, 0.0, 1.0);
        v_bias += K1 * residual;

        // Update covariance
        double I_KH00 = 1.0 - K0*H[0];
        double I_KH01 =    - K0*H[1];
        double I_KH10 =    - K1*H[0];
        double I_KH11 = 1.0 - K1*H[1];

        double newP00 = I_KH00*P[0][0] + I_KH01*P[1][0];
        double newP01 = I_KH00*P[0][1] + I_KH01*P[1][1];
        double newP10 = I_KH10*P[0][0] + I_KH11*P[1][0];
        double newP11 = I_KH10*P[0][1] + I_KH11*P[1][1];

        P[0][0] = newP00;
        P[0][1] = newP01;
        P[1][0] = newP10;
        P[1][1] = newP11;
    }
};

// =====================================================
// Cell Model
struct Cell {
    double true_soc;
    double capacity_Ah;
    double ir_ohm;
    double temperature;
    double voltage;
    SOC_Kalman kalman;

    Cell(double nominal_capacity_Ah, double nominal_ir, std::mt19937 &gen) {
        std::normal_distribution<double> cap_var(1.0, 0.02); // ±2%
        std::normal_distribution<double> ir_var(1.0, 0.05);  // ±5%
        capacity_Ah = nominal_capacity_Ah * cap_var(gen);
        ir_ohm      = nominal_ir * ir_var(gen);

        true_soc   = 0.5;
        temperature = 25.0;
        voltage     = lookup_ocv(true_soc);
        kalman.soc  = 0.5;
    }

    void apply_current(double current_A, double dt_s) {
        double dAh = current_A * dt_s / 3600.0;
        true_soc = std::clamp(true_soc - dAh / capacity_Ah, 0.0, 1.0);

        voltage = lookup_ocv(true_soc) - current_A * ir_ohm;

        // simple thermal dynamics
        temperature += (std::abs(current_A) / 100.0) * 0.05 * dt_s;
        temperature += (25.0 - temperature) * 0.001;
    }

    double measure_voltage() {
        static thread_local std::default_random_engine gen(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.002); // ±2mV
        return voltage + noise(gen);
    }

    void kalman_update(double current_A, double dt_s) {
        double dAh = current_A * dt_s / 3600.0;
        kalman.predict(dAh, capacity_Ah);

        double ir_drop = current_A * ir_ohm;
        double meas    = measure_voltage();
        kalman.update(meas, current_A, ir_drop);
    }
};

// =====================================================
// Globals for Simulation + HTTP
std::vector<Cell> cells;
std::mutex sim_mutex;
double pack_current_setpoint = 0.0;

// =====================================================
// Shared Memory Update
void update_shared_memory() {
    double total_voltage = 0.0;
    double min_v = 9999, max_v = 0, sum_v = 0;
    double min_t = 9999, max_t = -9999, sum_t = 0;
    double min_soc = 1.0, max_soc = 0, sum_soc = 0;

    for (int i = 0; i < NUM_CELLS; i++) {
        const auto &c = cells[i];

        // Scale + write cell data
        shm_ptr->cells[i].voltage_mV    = (uint16_t)std::round(c.voltage * 1000.0);
        shm_ptr->cells[i].true_soc_d1   = (uint16_t)std::round(c.true_soc * 1000.0 / 10.0);
        shm_ptr->cells[i].est_soc_d1    = (uint16_t)std::round(c.kalman.soc * 1000.0 / 10.0);
        shm_ptr->cells[i].temp_dC       = (uint16_t)std::round(c.temperature * 10.0);

        total_voltage += c.voltage;
        sum_v += c.voltage;
        sum_t += c.temperature;
        sum_soc += c.true_soc;

        min_v = std::min(min_v, c.voltage);
        max_v = std::max(max_v, c.voltage);
        min_t = std::min(min_t, c.temperature);
        max_t = std::max(max_t, c.temperature);
        min_soc = std::min(min_soc, c.true_soc);
        max_soc = std::max(max_soc, c.true_soc);
    }

    shm_ptr->stats.total_voltage_dV = (uint16_t)std::round(total_voltage * 100.0);
    shm_ptr->stats.pack_current_dA  = (int16_t)std::round(pack_current_setpoint * 10.0);

    shm_ptr->stats.min_voltage_mV = (uint16_t)std::round(min_v * 1000.0);
    shm_ptr->stats.max_voltage_mV = (uint16_t)std::round(max_v * 1000.0);
    shm_ptr->stats.avg_voltage_mV = (uint16_t)std::round((sum_v / NUM_CELLS) * 1000.0);

    shm_ptr->stats.min_temp_dC = (uint16_t)std::round(min_t * 10.0);
    shm_ptr->stats.max_temp_dC = (uint16_t)std::round(max_t * 10.0);
    shm_ptr->stats.avg_temp_dC = (uint16_t)std::round((sum_t / NUM_CELLS) * 10.0);

    shm_ptr->stats.min_soc_d1 = (uint16_t)std::round(min_soc * 1000.0 / 10.0);
    shm_ptr->stats.max_soc_d1 = (uint16_t)std::round(max_soc * 1000.0 / 10.0);
    shm_ptr->stats.avg_soc_d1 = (uint16_t)std::round((sum_soc / NUM_CELLS) * 1000.0 / 10.0);
}

// =====================================================
// Simulation Loop (runs at 10Hz)
void sim_loop() {
    double next_tick = ref_time_dbl();

    while (true) {
        next_tick += SIM_DT_S;

        {
            std::lock_guard<std::mutex> lock(sim_mutex);

            for (auto &cell : cells) {
                cell.apply_current(pack_current_setpoint, SIM_DT_S);
                cell.kalman_update(pack_current_setpoint, SIM_DT_S);
            }
            update_shared_memory();
        }

        double delay = next_tick - ref_time_dbl();
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        }
    }
}

// =====================================================
// Serve index.html
void serve_index(httplib::Response &res) {
    std::ifstream f("../html/index.html");
    if (!f.is_open()) {
        res.status = 404;
        res.set_content("index.html not found", "text/plain");
        return;
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    res.set_content(buffer.str(), "text/html");
}

// =====================================================
// HTTP Server
void run_http_server() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        serve_index(res);
    });

    svr.Get("/sm_cells", [](const httplib::Request &, httplib::Response &res) {
        json j;
        j["total_voltage"]   = shm_ptr->stats.total_voltage_dV / 100.0;
        j["current_setpoint"] = shm_ptr->stats.pack_current_dA / 10.0;

        j["stats"] = {
            {"min_voltage", shm_ptr->stats.min_voltage_mV / 1000.0},
            {"max_voltage", shm_ptr->stats.max_voltage_mV / 1000.0},
            {"avg_voltage", shm_ptr->stats.avg_voltage_mV / 1000.0},
            {"min_temp",    shm_ptr->stats.min_temp_dC / 10.0},
            {"max_temp",    shm_ptr->stats.max_temp_dC / 10.0},
            {"avg_temp",    shm_ptr->stats.avg_temp_dC / 10.0},
            {"min_soc",     shm_ptr->stats.min_soc_d1 / 100.0},
            {"max_soc",     shm_ptr->stats.max_soc_d1 / 100.0},
            {"avg_soc",     shm_ptr->stats.avg_soc_d1 / 100.0}
        };

        std::vector<json> cell_array;
        for (int i = 0; i < 10; i++) { // only send first 10 cells
            const auto &c = shm_ptr->cells[i];
            cell_array.push_back({
                {"voltage",  c.voltage_mV / 1000.0},
                {"true_soc", c.true_soc_d1 / 100.0},
                {"est_soc",  c.est_soc_d1 / 100.0},
                {"temp",     c.temp_dC / 10.0}
            });
        }
        j["cells"] = cell_array;

        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/set_current", [&](const httplib::Request &req, httplib::Response &res) {
        auto j = json::parse(req.body, nullptr, false);
        if (!j.is_discarded() && j.contains("current")) {
            pack_current_setpoint = j["current"].get<double>();
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\":\"invalid JSON\"}", "application/json");
        }
    });

    std::cout << "HTTP server running on port 8090...\n";
    svr.listen("0.0.0.0", 8090);
}

// =====================================================
// Main Entry
int main() {
    init_shared_memory();
    init_ocv_table();

    std::cout << "Initializing " << NUM_CELLS << " LiFePO4 cells...\n";
    std::mt19937 gen(std::random_device{}());
    cells.reserve(NUM_CELLS);
    for (int i = 0; i < NUM_CELLS; i++) {
        cells.emplace_back(NOMINAL_CAPACITY_AH, NOMINAL_IR, gen);
    }

    std::thread sim_thread(sim_loop);
    std::thread http_thread(run_http_server);

    sim_thread.join();
    http_thread.join();

    return 0;
}
```

---

## ✅ What changed?

* **Organized sections** with clear headings.
* Consistent **naming & spacing**.
* Removed duplicate endpoints & redundant JSON code.
* Clear **SIM loop** → `update_shared_memory()` called each step.
* Shared memory **initialized once**, used by both sim & HTTP.
* Web server now **only serves `/sm_cells`** from shared memory (single source of truth).

---

### ✅ Next Steps

Do you want me to also:

1. Add a **pack alarms tab** (voltage/temp out of range)?
2. Add **`shm_unlink()` cleanup** on exit?
3. Combine **sim + web server** into a **service mode** with a config flag?

Should I also simplify the **index.html** tabs for cells/stats/alarms?

Got it! Let’s extend your **480-cell LiFePO₄ Kalman simulation** so that:

✅ It **runs every 100 ms** using `ref_time_dbl()` for precise timing
✅ It launches a **tiny HTTP/JSON server on port 8090**
✅ The server responds with JSON (e.g., pack average SOC, first few cells, etc.)

We’ll use a **minimal embedded server** (`uWebSockets` or `cpp-httplib`), and allow basic JSON commands like:

* `GET /status` → returns **true SOC vs estimated SOC for a few cells**
* `POST /command` with JSON (e.g., `{ "current": 30.0 }`) to force a pack current

Here’s a **working outline**:

---

## ✅ Extended Code

```cpp
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>

#include <httplib.h>            // lightweight header-only HTTP server
#include <nlohmann/json.hpp>    // JSON library

using json = nlohmann::json;

// =====================================================
// LiFePO4 OCV vs SOC table
static std::vector<double> soc_table;
static std::vector<double> ocv_table;

void init_ocv_table() {
    soc_table.resize(101);
    ocv_table.resize(101);

    for (int i = 0; i <= 100; i++) {
        double soc = i / 100.0;
        soc_table[i] = soc;

        double ocv = 3.29 + 0.05 * (soc - 0.5) 
                   - 0.15 * exp(-20*soc) 
                   + 0.1  * exp(-20*(1-soc));
        if (ocv < 2.5) ocv = 2.5;
        if (ocv > 3.4) ocv = 3.4;
        ocv_table[i] = ocv;
    }
}

double lookup_ocv(double soc) {
    if (soc <= 0.0) return ocv_table.front();
    if (soc >= 1.0) return ocv_table.back();

    double idx_f = soc * 100.0;
    int idx = (int)floor(idx_f);
    double frac = idx_f - idx;

    return ocv_table[idx] + frac * (ocv_table[idx+1] - ocv_table[idx]);
}

// =====================================================
// Timing helper (double seconds)
double ref_time_dbl() {
    using clock = std::chrono::steady_clock;
    static auto t0 = clock::now();
    auto now = clock::now();
    std::chrono::duration<double> elapsed = now - t0;
    return elapsed.count();
}

// =====================================================
// Kalman filter for SOC + voltage bias
struct SOC_Kalman {
    double soc;
    double v_bias;
    double P[2][2];
    double Q_soc, Q_bias, R_meas;

    SOC_Kalman() {
        soc = 0.5;
        v_bias = 0.0;
        P[0][0] = 0.01;
        P[0][1] = 0.0;
        P[1][0] = 0.0;
        P[1][1] = 0.001;

        Q_soc = 1e-6;
        Q_bias = 1e-7;
        R_meas = 0.002;
    }

    void predict(double dAh, double capacity_Ah) {
        double soc_delta = -dAh / capacity_Ah;
        soc += soc_delta;
        if (soc < 0.0) soc = 0.0;
        if (soc > 1.0) soc = 1.0;
        P[0][0] += Q_soc;
        P[1][1] += Q_bias;
    }

    void update(double measured_voltage, double current_A, double ir_drop) {
        double expected_voltage = lookup_ocv(soc) - ir_drop + v_bias;
        double residual = measured_voltage - expected_voltage;

        double dOCV_dSOC = (lookup_ocv(std::min(soc+0.01,1.0)) - lookup_ocv(std::max(soc-0.01,0.0))) / 0.02;
        double H[2] = { -dOCV_dSOC, 1.0 };

        double S = H[0]*(P[0][0]*H[0] + P[0][1]*H[1]) +
                   H[1]*(P[1][0]*H[0] + P[1][1]*H[1]) + R_meas;

        double K0 = (P[0][0]*H[0] + P[0][1]*H[1]) / S;
        double K1 = (P[1][0]*H[0] + P[1][1]*H[1]) / S;

        soc += K0 * residual;
        v_bias += K1 * residual;

        if (soc < 0.0) soc = 0.0;
        if (soc > 1.0) soc = 1.0;

        double I_KH00 = 1.0 - K0*H[0];
        double I_KH01 =    - K0*H[1];
        double I_KH10 =    - K1*H[0];
        double I_KH11 = 1.0 - K1*H[1];

        double newP00 = I_KH00*P[0][0] + I_KH01*P[1][0];
        double newP01 = I_KH00*P[0][1] + I_KH01*P[1][1];
        double newP10 = I_KH10*P[0][0] + I_KH11*P[1][0];
        double newP11 = I_KH10*P[0][1] + I_KH11*P[1][1];

        P[0][0] = newP00;
        P[0][1] = newP01;
        P[1][0] = newP10;
        P[1][1] = newP11;
    }
};

// =====================================================
// Cell model
struct Cell {
    double true_soc;
    double capacity_Ah;
    double ir_ohm;
    double temperature;
    double voltage;
    SOC_Kalman kalman;

    Cell(double nominal_capacity_Ah, double nominal_ir, std::mt19937 &gen) {
        std::normal_distribution<double> cap_var(1.0, 0.02);
        std::normal_distribution<double> ir_var(1.0, 0.05);
        capacity_Ah = nominal_capacity_Ah * cap_var(gen);
        ir_ohm = nominal_ir * ir_var(gen);

        true_soc = 0.5;
        temperature = 25.0;
        voltage = lookup_ocv(true_soc);
        kalman.soc = 0.5;
    }

    void apply_current(double current_A, double dt_s) {
        double dAh = current_A * dt_s / 3600.0;
        true_soc -= dAh / capacity_Ah;
        if (true_soc < 0.0) true_soc = 0.0;
        if (true_soc > 1.0) true_soc = 1.0;

        voltage = lookup_ocv(true_soc) - current_A * ir_ohm;

        temperature += (std::abs(current_A) / 100.0) * 0.05 * dt_s;
        temperature += (25.0 - temperature) * 0.001;
    }

    double measure_voltage() {
        static thread_local std::default_random_engine gen(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.002);
        return voltage + noise(gen);
    }

    void kalman_update(double current_A, double dt_s) {
        double dAh = current_A * dt_s / 3600.0;
        kalman.predict(dAh, capacity_Ah);

        double ir_drop = current_A * ir_ohm;
        double meas = measure_voltage();
        kalman.update(meas, current_A, ir_drop);
    }
};

// =====================================================
// Globals
constexpr int NUM_CELLS = 480;
constexpr double NOMINAL_CAPACITY_AH = 280.0;
constexpr double NOMINAL_IR = 0.0005;
constexpr double SIM_DT_S = 0.1; // 100ms

std::vector<Cell> cells;
double pack_current = 0.0;
std::mutex sim_mutex;

// =====================================================
// Simulation loop every 100ms
void sim_loop() {
    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<double> current_dist(0.0, 50.0);

    double next_tick = ref_time_dbl();

    while (true) {
        next_tick += 0.1; // 100ms steps

        {
            std::lock_guard<std::mutex> lock(sim_mutex);

            // if no override current, randomize
            if (pack_current == 0.0)
                pack_current = current_dist(gen);

            for (auto &cell : cells) {
                cell.apply_current(pack_current, SIM_DT_S);
                cell.kalman_update(pack_current, SIM_DT_S);
            }
        }

        // Sleep until next tick
        double now = ref_time_dbl();
        double delay = next_tick - now;
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        }
    }
}

// =====================================================
// Simple HTTP/JSON server
void run_http_server() {
    httplib::Server svr;

    svr.Get("/status", [](const httplib::Request&, httplib::Response &res) {
        std::lock_guard<std::mutex> lock(sim_mutex);

        json j;
        double avg_true_soc = 0, avg_est_soc = 0;
        for (auto &c : cells) {
            avg_true_soc += c.true_soc;
            avg_est_soc += c.kalman.soc;
        }
        avg_true_soc /= NUM_CELLS;
        avg_est_soc /= NUM_CELLS;

        j["avg_true_soc"] = avg_true_soc * 100.0;
        j["avg_est_soc"] = avg_est_soc * 100.0;
        j["pack_current"] = pack_current;

        // Include first 5 cells as sample
        json sample = json::array();
        for (int i = 0; i < 5; i++) {
            json c;
            c["true_soc"] = cells[i].true_soc;
            c["est_soc"]  = cells[i].kalman.soc;
            c["voltage"]  = cells[i].voltage;
            sample.push_back(c);
        }
        j["sample_cells"] = sample;

        res.set_content(j.dump(2), "application/json");
    });

    svr.Post("/command", [](const httplib::Request &req, httplib::Response &res) {
        auto body = json::parse(req.body, nullptr, false);
        if (!body.is_discarded() && body.contains("current")) {
            double new_current = body["current"];
            {
                std::lock_guard<std::mutex> lock(sim_mutex);
                pack_current = new_current;
            }
            res.set_content("{\"status\":\"current updated\"}", "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\":\"invalid JSON\"}", "application/json");
        }
    });

    std::cout << "HTTP server running on port 8090...\n";
    svr.listen("0.0.0.0", 8090);
}

// =====================================================
// Main
int main() {
    init_ocv_table();

    std::cout << "Initializing 480 LiFePO4 cells...\n";
    std::mt19937 gen(std::random_device{}());
    cells.reserve(NUM_CELLS);
    for (int i = 0; i < NUM_CELLS; i++) {
        cells.emplace_back(NOMINAL_CAPACITY_AH, NOMINAL_IR, gen);
    }

    std::thread sim_thread(sim_loop);
    std::thread http_thread(run_http_server);

    sim_thread.join();
    http_thread.join();
    return 0;
}
```

---

## ✅ How it works

1. **Simulation thread (`sim_loop`)**

   * Runs every 100 ms
   * Applies the current to all 480 cells
   * Updates true SOC + voltage + Kalman estimate

2. **HTTP Server thread**

   * `/status` returns JSON like:

     ```json
     {
       "avg_true_soc": 49.83,
       "avg_est_soc": 49.90,
       "pack_current": -12.3,
       "sample_cells": [
         {"true_soc":0.498,"est_soc":0.497,"voltage":3.29},
         ...
       ]
     }
     ```
   * `POST /command` with JSON like `{ "current": 30.0 }` overrides the pack current.

3. **Thread-safe**: uses a mutex for `cells` + `pack_current`.

4. **Benchmark-friendly**: 480 cells × 10 Hz is trivial load for i.MX93.

---

## ✅ Usage

* Compile with `-pthread` and include `httplib.h` (single header).
* Run on your i.MX93.
* Open browser → `http://<ip>:8090/status`
* Send a new current:

  ```bash
  curl -X POST -H "Content-Type: application/json" \
       -d '{"current":50.0}' http://localhost:8090/command
  ```

---

Would you like me to:

✅ Add **temperature effect on IR & OCV**?
✅ Or **SIMD optimize** the main update loop for i.MX93?

Also, do you want the `/status` endpoint to return **all 480 cells**, or keep it compact?


=================================
Got it—let’s set up a **simple 1D Kalman filter** for smoothing a single battery cell voltage reading at **10 Hz (every 0.1s)**.

We’ll assume:

* **State** is the true cell voltage.
* **Measurement** is a noisy reading from the ADC.
* We’ll model slow drift (process noise) + measurement noise.

---

## ✅ Step 1: Basic Kalman Filter Model

We use a simple discrete-time system:

```
x[k] = x[k-1] + w   (process model)
z[k] = x[k]   + v   (measurement)
```

* **x\[k]** → estimated true voltage at time step k
* **z\[k]** → measured voltage from ADC
* **w** → process noise \~ N(0, Q)
* **v** → measurement noise \~ N(0, R)

Where:

* **Q** = how much we trust the voltage to drift (small for a slow cell)
* **R** = measurement noise variance (ADC & sensor noise)

---

## ✅ Step 2: Kalman Update Equations

For each step:

1. **Predict step**

   ```
   x_predict = x_prev
   P_predict = P_prev + Q
   ```

2. **Update step**

   ```
   K = P_predict / (P_predict + R)
   x_new = x_predict + K * (z_measured - x_predict)
   P_new = (1 - K) * P_predict
   ```

---

## ✅ Step 3: Typical Values for a Battery Cell

* Suppose your ADC has **±5 mV RMS noise** → R = (0.005²)
* Cell voltage drifts slowly (\~millivolts/second) → Q = 1e-6

At **10 Hz**, this filter will:

* **Smooth out ADC noise**
* Still track slow changes (like under load)

---

## ✅ Step 4: Minimal C++ Implementation

```cpp
struct KalmanFilter {
    double x;  // state (voltage)
    double P;  // error covariance
    double Q;  // process noise
    double R;  // measurement noise

    KalmanFilter(double init_voltage, double q, double r) 
        : x(init_voltage), P(1), Q(q), R(r) {}

    double update(double z) {
        // Prediction step
        double x_pred = x;
        double P_pred = P + Q;

        // Kalman gain
        double K = P_pred / (P_pred + R);

        // Update step
        x = x_pred + K * (z - x_pred);
        P = (1 - K) * P_pred;

        return x; // smoothed voltage
    }
};

// Example usage
KalmanFilter cell_filter(3.7, 1e-6, 0.000025); // Q small, R ~ 5mV noise^2

// Loop at 10Hz:
double measured_voltage = read_adc();  // e.g. 3.701 V
double smoothed_voltage = cell_filter.update(measured_voltage);
```

---

## ✅ Step 5: Next Steps

1. **Tune Q & R**

   * Measure raw ADC variance → set R = σ²
   * For a very slow-changing cell voltage, keep Q small

2. **Add Temperature/Load Model**

   * Later we can include a model of internal resistance to better estimate open-circuit voltage.

3. **SOx Estimation**

   * Combine filtered voltage + current + temperature into a **SoC estimator**.

---

Would you like me to:
✅ **Generate a ready-to-run simulation** (Python) that shows how this filter smooths noisy voltage?
✅ Or **extend it toward SoC (State-of-Charge) estimation using OCV + Coulomb counting**?
Got it! We’ll build a **basic LiFePO₄ 280 Ah cell model** with:

✅ **Voltage & temperature simulation** under charge/discharge
✅ **Apparent internal resistance** → voltage drop from load current
✅ **Thermal model** → temperature rise from I²R heating
✅ A **measurement function** that adds ADC noise
✅ Works with your **Kalman filter**

This will give you a realistic simulation of 480 cells, updating at **10 Hz**.

---

## ✅ 1. Basic Cell Model

### Key Parameters (typical LiFePO₄ 280Ah)

| Parameter                     |      Value |
| ----------------------------- | ---------: |
| Nominal Voltage               |      3.2 V |
| Max Voltage                   |     3.65 V |
| Min Voltage                   |      2.5 V |
| Capacity                      |     280 Ah |
| Internal Resistance (cold)    |  \~0.25 mΩ |
| Internal Resistance (warm)    |  \~0.15 mΩ |
| Heat Capacity (thermal mass)  | \~1000 J/K |
| Thermal resistance to ambient | \~0.05 K/W |

So at **100A discharge**, I²R loss = 100² × 0.0002 = **2W**, small but noticeable.
At **280A discharge**, it’s **\~15 W** per cell.

---

## ✅ 2. Struct for Each Cell State

```cpp
struct SimCell {
    double soc;       // State of charge [0..1]
    double voltage;   // Current terminal voltage (V)
    double temperature; // Current temp (°C)
    double ir;        // apparent internal resistance (Ohms)

    // for Kalman measurement noise
    double true_voltage;
};
```

---

## ✅ 3. Simulation Functions

### 3.1 Open-circuit voltage curve (simplified)

LiFePO₄ voltage vs SOC is **flat**. We approximate with piecewise linear:

```cpp
inline double ocv_from_soc(double soc) {
    if (soc <= 0.0) return 2.5;   // fully empty
    if (soc >= 1.0) return 3.65;  // fully charged

    // Approx flat plateau at ~3.3V, small slope
    return 3.0 + 0.6 * soc;  // 3.0V @ 0% → 3.6V @ 100%
}
```

---

### 3.2 Update cell state with a current load

```cpp
void simulate_cell(SimCell &cell, double current_A, double dt_s) {
    // Update SOC based on current
    double cell_capacity_As = 280.0 * 3600.0;  // 280 Ah → As
    cell.soc -= (current_A * dt_s) / cell_capacity_As;  // current >0 = discharge

    // Clamp SOC
    if (cell.soc < 0.0) cell.soc = 0.0;
    if (cell.soc > 1.0) cell.soc = 1.0;

    // Base OCV
    double ocv = ocv_from_soc(cell.soc);

    // Temperature-dependent resistance (colder → higher IR)
    double r_temp = cell.ir * (25.0 / (cell.temperature + 1.0));

    // Terminal voltage under load
    cell.true_voltage = ocv - current_A * r_temp;  

    // Clamp realistic voltage
    if (cell.true_voltage < 2.0) cell.true_voltage = 2.0;
    if (cell.true_voltage > 3.7) cell.true_voltage = 3.7;

    // Thermal model: I²R heating (W)
    double heat_power = current_A * current_A * r_temp;

    // Cool to ambient (passive dissipation)
    double ambient_temp = 25.0;
    double thermal_mass_J_per_K = 1000.0; // big heavy cell
    double thermal_resistance_K_per_W = 0.05;

    double heat_loss = (cell.temperature - ambient_temp) / thermal_resistance_K_per_W;
    double net_heat_W = heat_power - heat_loss;

    // Temperature rise ΔT = P * dt / C
    cell.temperature += (net_heat_W * dt_s) / thermal_mass_J_per_K;

    // Finally assign voltage for reading
    cell.voltage = cell.true_voltage;
}
```

---

### 3.3 Measurement function (with ADC noise)

```cpp
inline double noisy_voltage(double true_v) {
    double noise_stddev = 0.005; // 5mV noise
    double n = ((rand() % 1000) / 1000.0 - 0.5) * 2 * noise_stddev;
    return true_v + n;
}
```

---

## ✅ 4. Simulate 480 Cells Together

```cpp
class BatterySim {
public:
    static constexpr size_t NUM_CELLS = 480;
    SimCell cells[NUM_CELLS];

    BatterySim() {
        for (auto &c : cells) {
            c.soc = 0.8; // start 80% charged
            c.temperature = 25.0;
            c.ir = 0.0002; // 0.2 mΩ typical
            c.voltage = ocv_from_soc(c.soc);
        }
    }

    // Apply same current to all cells (simple)
    void step(double current_A, double dt_s) {
        for (auto &c : cells) {
            simulate_cell(c, current_A, dt_s);
        }
    }

    // Get measurement array with ADC noise
    std::array<double, NUM_CELLS> measure_all() {
        std::array<double, NUM_CELLS> m{};
        for (size_t i = 0; i < NUM_CELLS; i++) {
            m[i] = noisy_voltage(cells[i].true_voltage);
        }
        return m;
    }
};
```

---

## ✅ 5. Combine Simulation + Kalman Filtering

```cpp
int main() {
    BatterySim sim;
    KalmanBatteryArray kalman(3.3, 1e-6, 0.005*0.005);

    double dt = 0.1; // 10Hz

    for (int step = 0; step < 300; step++) {
        double load_current = (step < 100) ? 50.0 : -20.0; // discharge then charge

        // Simulate 1 step
        sim.step(load_current, dt);

        // Take noisy measurement
        auto measurements = sim.measure_all();

        // Kalman filter all cells
        kalman.update_all(measurements);

        // Print first 3 cells
        std::cout << "Step " << step
                  << " Load=" << load_current << "A | "
                  << "C0 true=" << sim.cells[0].true_voltage 
                  << " meas=" << measurements[0] 
                  << " filt=" << kalman.cells[0].x
                  << " Temp=" << sim.cells[0].temperature << "°C"
                  << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

---

## ✅ What This Gives You

* **Realistic cell voltage behavior** under load
* **Thermal drift**
* **Noisy ADC readings**
* **Filtered Kalman estimate**

You can now **stress-test your Kalman filter** with realistic charge/discharge scenarios.

---

## ✅ Next Possible Enhancements

* More **realistic LiFePO₄ OCV curve** from datasheet
* **SOC drift** (capacity fade, hysteresis)
* Different **IR per cell** (random variation)
* Add **parallel/series pack current distribution**

---

Would you like me to:

1. **Add random variation in IR & capacity for each cell (manufacturing differences)?**
2. **Include a more realistic OCV lookup table from LiFePO₄ data?**
3. Or **extend with Coulomb counting for SoC**?

What’s the next step?
Got it! Let’s enhance the model with:

✅ **Random variation of IR & capacity** per cell (±5–10%)
✅ **More realistic LiFePO₄ OCV vs SOC curve** from actual data

This makes the simulation much closer to a real 280 Ah LiFePO₄ pack.

---

## ✅ 1. Realistic LiFePO₄ OCV Curve

LiFePO₄ has a **flat plateau \~3.3 V** with sharp knee near 0–10% and 90–100% SOC. Approximation:

| SOC % | OCV (V) |
| ----- | ------- |
| 0%    | 2.50    |
| 5%    | 3.00    |
| 10%   | 3.20    |
| 20%   | 3.27    |
| 30%   | 3.30    |
| 40%   | 3.31    |
| 50%   | 3.32    |
| 60%   | 3.33    |
| 70%   | 3.34    |
| 80%   | 3.35    |
| 90%   | 3.40    |
| 95%   | 3.50    |
| 100%  | 3.65    |

We’ll **interpolate** between these points.

---

### OCV Lookup + Linear Interpolation

```cpp
static const std::vector<std::pair<double, double>> lifepo4_ocv_table = {
    {0.00, 2.50},
    {0.05, 3.00},
    {0.10, 3.20},
    {0.20, 3.27},
    {0.30, 3.30},
    {0.40, 3.31},
    {0.50, 3.32},
    {0.60, 3.33},
    {0.70, 3.34},
    {0.80, 3.35},
    {0.90, 3.40},
    {0.95, 3.50},
    {1.00, 3.65}
};

double ocv_from_soc(double soc) {
    if (soc <= 0.0) return lifepo4_ocv_table.front().second;
    if (soc >= 1.0) return lifepo4_ocv_table.back().second;

    // Find two adjacent points
    for (size_t i = 1; i < lifepo4_ocv_table.size(); i++) {
        if (soc <= lifepo4_ocv_table[i].first) {
            double soc0 = lifepo4_ocv_table[i - 1].first;
            double v0   = lifepo4_ocv_table[i - 1].second;
            double soc1 = lifepo4_ocv_table[i].first;
            double v1   = lifepo4_ocv_table[i].second;

            double t = (soc - soc0) / (soc1 - soc0);
            return v0 + t * (v1 - v0);
        }
    }
    return lifepo4_ocv_table.back().second;
}
```

So now SOC → OCV is **nonlinear** and matches actual LiFePO₄ behavior.

---

## ✅ 2. Random Variation for IR & Capacity

Each cell is slightly different. Let’s give each cell a random **capacity** & **internal resistance**.

```cpp
inline double rand_variation(double base, double pct_variation) {
    double factor = 1.0 + ((rand() / double(RAND_MAX)) - 0.5) * 2.0 * pct_variation;
    return base * factor;
}

struct SimCell {
    double soc;          // State of charge [0..1]
    double voltage;      // Measured voltage
    double temperature;  // °C
    double ir;           // Ohms (varies per cell)
    double capacity_As;  // Actual capacity (varies per cell)
    double true_voltage; // internal
};
```

When initializing the pack:

```cpp
BatterySim() {
    srand(time(nullptr));
    for (auto &c : cells) {
        c.soc = 0.8;
        c.temperature = 25.0;

        // Random IR ±10%
        c.ir = rand_variation(0.00020, 0.10); // 0.2 mΩ ±10%

        // Random capacity ±5%
        c.capacity_As = rand_variation(280.0 * 3600.0, 0.05);

        c.voltage = ocv_from_soc(c.soc);
    }
}
```

---

## ✅ 3. Updated Simulation with Per-cell Capacity

```cpp
void simulate_cell(SimCell &cell, double current_A, double dt_s) {
    // SOC update (each cell has different usable capacity)
    cell.soc -= (current_A * dt_s) / cell.capacity_As;
    if (cell.soc < 0.0) cell.soc = 0.0;
    if (cell.soc > 1.0) cell.soc = 1.0;

    // Base OCV from realistic curve
    double ocv = ocv_from_soc(cell.soc);

    // Temp-dependent IR scaling
    double r_temp = cell.ir * (25.0 / (cell.temperature + 1.0));

    // Terminal voltage under load
    cell.true_voltage = ocv - current_A * r_temp;

    if (cell.true_voltage < 2.0) cell.true_voltage = 2.0;
    if (cell.true_voltage > 3.7) cell.true_voltage = 3.7;

    // Thermal model: simple I²R heating
    double heat_power = current_A * current_A * r_temp;
    double ambient = 25.0;
    double thermal_mass_J_per_K = 1000.0;
    double thermal_resistance_K_per_W = 0.05;

    double heat_loss = (cell.temperature - ambient) / thermal_resistance_K_per_W;
    double net_heat_W = heat_power - heat_loss;
    cell.temperature += (net_heat_W * dt_s) / thermal_mass_J_per_K;

    cell.voltage = cell.true_voltage;
}
```

Now, each cell:

* Discharges slightly differently (different capacity)
* Sags differently under load (different IR)
* Warms differently (IR affects heating)

---

## ✅ 4. Full Pack Simulation Example

```cpp
int main() {
    BatterySim sim;

    double dt = 0.1; // 10Hz
    for (int step = 0; step < 200; step++) {
        double load_current = (step < 100) ? 100.0 : -50.0; // 100A discharge then 50A charge

        sim.step(load_current, dt);

        // Check first 3 cells
        for (int i = 0; i < 3; i++) {
            auto &c = sim.cells[i];
            std::cout << "Step " << step
                      << " Cell" << i
                      << " SOC=" << c.soc*100 << "%"
                      << " V=" << c.true_voltage
                      << " IR=" << c.ir*1000 << "mΩ"
                      << " Temp=" << c.temperature << "°C\n";
        }
        std::cout << "------\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

---

## ✅ What’s New in This Version?

* **Real OCV curve** → realistic flat plateau, steep knees
* **Manufacturing variation** → capacity ±5%, IR ±10%
* **More realistic heating** → IR variation changes heat rate
* Cells will **diverge over time**, like real packs

---

## ✅ Next Step Options

1. **Add random *initial SOC* variation** (e.g. ±2%)
2. **Series pack voltage monitoring** (sum of all 480 cells)
3. **Balancing simulation** (passive bleed resistors)
4. Integrate a **Kalman filter per cell** to estimate SOC

---Here's a more realistic LiFePO₄ open circuit voltage (OCV) curve versus state of charge (SOC). This curve is based on empirical fits and reflects the characteristic flat region around 3.3–3.4V typical of LiFePO₄ chemistry.

### Next steps:

* We can assign each of the 480 cells:

  * A random internal resistance (IR) within a small range (e.g., 0.3–0.5 mΩ)
  * A random capacity (e.g., 275–285 Ah)
* We’ll use this OCV curve as the baseline for both measurement and simulation.

Would you like me to generate the C++ data structures and code that initialize and simulate these cell behaviors next?


Would you like me to:

✅ **Add initial SOC variation** and **simulate passive balancing**?
✅ Or focus on **SOC estimation (Kalman filter + coulomb counting)** next?Let’s break this down.

We have **480 cells**, each updated **10 times per second** (every 100 ms). For each update we would:

1. **Simulate or measure cell voltage**:

   * Lookup OCV from the SOC table (≈20 entries → simple interpolation).
   * Add IR drop (current × IR).
   * Update SOC based on charge/discharge current (Coulomb counting).
   * Update temperature with a simple thermal model.

2. **Run a Kalman filter** on each cell voltage (small 2–4 state Kalman filter).

---

### **Computation per update**

* **OCV lookup + interpolation** ≈ \~20 floating-point ops
* **Coulomb counting update** ≈ \~5 ops
* **IR voltage calculation** ≈ \~5 ops
* **Temperature update** ≈ \~5 ops
* **Kalman filter update** ≈ \~50–100 ops

So per cell ≈ **\~150 floating-point ops per update**

---

### **Per cycle (100 ms)**

* **480 cells × 150 ops = \~72,000 ops per 100 ms**
* **Per second** = \~720,000 floating-point ops/sec.

---

### **i.MX93 performance**

The i.MX93 (Cortex‑A55 dual core @ \~1.7 GHz) can easily do **hundreds of millions of FLOPs/sec**.

Even a *single core* could handle **hundreds of thousands of cells** at 10 Hz in real time.
So **480 cells at 10 Hz would be trivial**, likely **<1% CPU usage** on one core.

---

### **So practically:**

* The simulation + Kalman filter for 480 cells at 10 Hz will run **near-instantaneously** on an i.MX93.
* You could even run **multiple parallel packs (e.g., 10×480 cells)** without issue.

---

Would you like me to:
✅ **Write the C++ code for the full 480-cell simulation with random IR & capacity variation, OCV lookup, and a simple Kalman filter**
✅ **AND** include a benchmark timer so you can see exactly how long it takes on the i.MX93?
Got it! Here’s **C++ code** that:

✅ Creates **480 cells** with random IR & capacity variations
✅ Simulates **LiFePO₄ OCV vs SOC** behavior
✅ Applies **charge/discharge current** + thermal model
✅ Runs a **simple Kalman filter** on each cell voltage
✅ Benchmarks how long **one 10 Hz update cycle** takes

You can run this on your **i.MX93** and see actual timings.

---

### **C++ Code: 480 Cell Kalman Simulation**

```cpp
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>

// ---------------------------------------------
// Simple LiFePO4 OCV vs SOC lookup table
// (SOC 0.0 → 1.0 in steps of 0.1)
static const std::vector<double> soc_table = {
    0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0
};
static const std::vector<double> ocv_table = {
    2.50, 3.00, 3.20, 3.25, 3.27, 3.28, 3.29, 3.30, 3.32, 3.35, 3.40
};

// ---------------------------------------------
// Linear interpolation for OCV
double lookup_ocv(double soc) {
    if (soc <= 0.0) return ocv_table.front();
    if (soc >= 1.0) return ocv_table.back();

    for (size_t i = 0; i < soc_table.size()-1; i++) {
        if (soc >= soc_table[i] && soc <= soc_table[i+1]) {
            double ratio = (soc - soc_table[i]) / (soc_table[i+1] - soc_table[i]);
            return ocv_table[i] + ratio * (ocv_table[i+1] - ocv_table[i]);
        }
    }
    return ocv_table.back();
}

// ---------------------------------------------
// Simple 1D Kalman filter for voltage
struct KalmanFilter {
    double x;      // state (voltage estimate)
    double P;      // error covariance
    double Q;      // process noise
    double R;      // measurement noise

    KalmanFilter(double init_voltage = 3.3) {
        x = init_voltage;
        P = 0.01;
        Q = 0.0001;  // small process noise
        R = 0.001;   // measurement noise
    }

    double update(double measurement) {
        // Prediction step
        P += Q;

        // Measurement update
        double K = P / (P + R); // Kalman gain
        x = x + K * (measurement - x);
        P = (1 - K) * P;
        return x;
    }
};

// ---------------------------------------------
// Cell Model
struct Cell {
    double soc;         // State of charge [0.0 - 1.0]
    double capacity_Ah; // Actual capacity variation
    double ir_ohm;      // Internal resistance
    double temperature; // °C
    double voltage;     // Terminal voltage
    KalmanFilter kf;    // Kalman filter for voltage

    // Constructor with random variations
    Cell(double nominal_capacity_Ah, double nominal_ir, std::mt19937 &gen) {
        std::normal_distribution<double> cap_var(1.0, 0.02); // ±2% capacity variation
        std::normal_distribution<double> ir_var(1.0, 0.05);  // ±5% IR variation
        capacity_Ah = nominal_capacity_Ah * cap_var(gen);
        ir_ohm = nominal_ir * ir_var(gen);

        soc = 0.5;            // start at 50% SOC
        temperature = 25.0;   // start at ambient
        voltage = lookup_ocv(soc);
        kf = KalmanFilter(voltage);
    }

    // Apply current for dt seconds (positive = discharge)
    void apply_current(double current_A, double dt_s) {
        double dAh = current_A * dt_s / 3600.0;
        soc -= dAh / capacity_Ah;
        if (soc < 0.0) soc = 0.0;
        if (soc > 1.0) soc = 1.0;

        // Base voltage
        double ocv = lookup_ocv(soc);

        // IR drop
        double v_drop = current_A * ir_ohm;
        voltage = ocv - v_drop;

        // Simple thermal model: rise 0.05 °C per second per 100A
        temperature += (std::abs(current_A) / 100.0) * 0.05 * dt_s;

        // Cool towards 25°C slowly
        temperature += (25.0 - temperature) * 0.001;
    }

    double measure_voltage() {
        // Add measurement noise ±2mV
        static thread_local std::default_random_engine gen(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.002);
        return voltage + noise(gen);
    }

    void kalman_update() {
        double meas = measure_voltage();
        voltage = kf.update(meas);
    }
};

// ---------------------------------------------
// Simulation parameters
constexpr int NUM_CELLS = 480;
constexpr double NOMINAL_CAPACITY_AH = 280.0;
constexpr double NOMINAL_IR = 0.0005;    // 0.5 mΩ typical
constexpr double SIM_DT_S = 0.1;         // 10 Hz update

int main() {
    std::cout << "Initializing 480 LiFePO4 cells..." << std::endl;

    // Random generator for manufacturing variation
    std::mt19937 gen(std::random_device{}());

    // Create all cells
    std::vector<Cell> cells;
    cells.reserve(NUM_CELLS);
    for (int i = 0; i < NUM_CELLS; i++) {
        cells.emplace_back(NOMINAL_CAPACITY_AH, NOMINAL_IR, gen);
    }

    // Random current simulation (charge/discharge ±50A)
    std::normal_distribution<double> current_dist(0.0, 50.0);

    // Benchmark start
    auto start_time = std::chrono::high_resolution_clock::now();

    // Run 100 simulation steps (10 seconds total)
    for (int step = 0; step < 100; step++) {
        double pack_current = current_dist(gen); // one pack current applied to all cells
        for (auto &cell : cells) {
            cell.apply_current(pack_current, SIM_DT_S);
            cell.kalman_update();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "Simulated 480 cells for 10s (100 steps @10Hz) in "
              << elapsed.count() << " ms" << std::endl;

    // Print one example cell state
    const auto &c0 = cells[0];
    std::cout << "Example cell after 10s: SOC=" << c0.soc*100 << "%, "
              << "Voltage=" << c0.voltage << "V, Temp=" << c0.temperature << "°C" << std::endl;

    return 0;
}
```

---

### **What this does**

✅ Simulates **480 cells** with random IR + capacity
✅ Uses a **LiFePO₄ OCV table** for realistic voltage behavior
✅ Adds IR drop + random noise
✅ Updates a **1D Kalman filter** per cell
✅ Measures total runtime for **100 simulation steps (10 seconds @10Hz)**

---

### **Expected performance**

* On an i.MX93, this should be **<10 ms** for all 480 cells at 10 Hz → **<<1% CPU usage**.
* You’ll see something like:

```
Simulated 480 cells for 10s (100 steps @10Hz) in 3.4 ms
Example cell after 10s: SOC=49.8%, Voltage=3.276V, Temp=25.3°C
```

---

### **Next steps**

Would you like me to:

✅ Add a **more detailed LiFePO₄ OCV table (like 0.01 SOC resolution)**?
✅ Add **per-cell current variation** (some cells slightly mismatched)?
✅ Add a **Coulomb-counting Kalman filter** (SOC estimation + voltage)?

Should I extend it to a **full SOC estimator with current sensor noise**?

Got it! Let’s **upgrade the simulation** with:

✅ **High-resolution OCV table** (0.01 SOC steps for LiFePO₄ cells)
✅ A **2-state Kalman filter** that estimates BOTH:
   • **SOC (Coulomb-counting + voltage correction)**
   • **Voltage measurement noise**

This makes it much closer to what a **real BMS would do**.

---

### ✅ **Key Upgrades**

1. **Detailed OCV Table**

   * 101 points from SOC=0.00 → 1.00 with realistic LiFePO₄ shape.
2. **SOC+Voltage Kalman Filter**

   * State vector **x = \[SOC, Voltage\_offset]**
   * Predicts SOC via Coulomb counting
   * Corrects SOC with OCV vs measured terminal voltage

---

### **C++ Full Code**

```cpp
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>

// =====================================================
// LiFePO4 detailed OCV vs SOC table (0.0 → 1.0 step 0.01)
static std::vector<double> soc_table;
static std::vector<double> ocv_table;

void init_ocv_table() {
    soc_table.resize(101);
    ocv_table.resize(101);

    for (int i = 0; i <= 100; i++) {
        double soc = i / 100.0;
        soc_table[i] = soc;

        // Simplified LiFePO4 OCV model:
        // flat plateau ~3.29V, with slight slopes near 0% & 100%
        double ocv = 3.29 + 0.05 * (soc - 0.5) - 0.15 * exp(-20*soc) + 0.1*exp(-20*(1-soc));
        if (ocv < 2.5) ocv = 2.5;
        if (ocv > 3.4) ocv = 3.4;
        ocv_table[i] = ocv;
    }
}

// =====================================================
// Linear interpolation for OCV lookup
double lookup_ocv(double soc) {
    if (soc <= 0.0) return ocv_table.front();
    if (soc >= 1.0) return ocv_table.back();

    double idx_f = soc * 100.0;
    int idx = (int)floor(idx_f);
    double frac = idx_f - idx;

    return ocv_table[idx] + frac * (ocv_table[idx+1] - ocv_table[idx]);
}

// =====================================================
// 2-state Kalman Filter for SOC estimation
struct SOC_Kalman {
    // State: [SOC, Voltage_bias]
    double soc;          // estimated SOC
    double v_bias;       // estimated voltage bias (drift)
    double P[2][2];      // 2x2 covariance
    double Q_soc;        // process noise (SOC drift)
    double Q_bias;       // process noise (voltage drift)
    double R_meas;       // measurement noise (voltage sensor)

    SOC_Kalman() {
        soc = 0.5;
        v_bias = 0.0;
        P[0][0] = 0.01;  // initial SOC uncertainty
        P[0][1] = 0.0;
        P[1][0] = 0.0;
        P[1][1] = 0.001; // small bias uncertainty

        Q_soc  = 1e-6;   // small process noise for SOC
        Q_bias = 1e-7;   // tiny drift
        R_meas = 0.002;  // ~2mV measurement noise
    }

    // Prediction step using Coulomb counting
    void predict(double dAh, double capacity_Ah) {
        double soc_delta = -dAh / capacity_Ah;
        soc += soc_delta;
        if (soc < 0.0) soc = 0.0;
        if (soc > 1.0) soc = 1.0;

        // Covariance update
        P[0][0] += Q_soc;
        P[1][1] += Q_bias;
    }

    // Update step using measured terminal voltage
    void update(double measured_voltage, double current_A, double ir_drop) {
        // Expected voltage = OCV(soc) - IR_drop + bias
        double expected_voltage = lookup_ocv(soc) - ir_drop + v_bias;
        double residual = measured_voltage - expected_voltage;

        // Measurement Jacobian H = [dOCV/dSOC, 1]
        // approximate slope: OCV change per SOC ~ 0.1V max over SOC
        double dOCV_dSOC = (lookup_ocv(std::min(soc+0.01,1.0)) - lookup_ocv(std::max(soc-0.01,0.0))) / 0.02;
        double H[2] = { -dOCV_dSOC, 1.0 };

        // S = H*P*H^T + R
        double S = H[0]*(P[0][0]*H[0] + P[0][1]*H[1]) +
                   H[1]*(P[1][0]*H[0] + P[1][1]*H[1]) + R_meas;

        // Kalman gain K = P*H^T * S^-1
        double K0 = (P[0][0]*H[0] + P[0][1]*H[1]) / S;
        double K1 = (P[1][0]*H[0] + P[1][1]*H[1]) / S;

        // Update state
        soc += K0 * residual;
        v_bias += K1 * residual;

        if (soc < 0.0) soc = 0.0;
        if (soc > 1.0) soc = 1.0;

        // Update covariance: P = (I - K*H)*P
        double I_KH00 = 1.0 - K0*H[0];
        double I_KH01 =    - K0*H[1];
        double I_KH10 =    - K1*H[0];
        double I_KH11 = 1.0 - K1*H[1];

        double newP00 = I_KH00*P[0][0] + I_KH01*P[1][0];
        double newP01 = I_KH00*P[0][1] + I_KH01*P[1][1];
        double newP10 = I_KH10*P[0][0] + I_KH11*P[1][0];
        double newP11 = I_KH10*P[0][1] + I_KH11*P[1][1];

        P[0][0] = newP00;
        P[0][1] = newP01;
        P[1][0] = newP10;
        P[1][1] = newP11;
    }
};

// =====================================================
// Cell model with internal resistance & capacity
struct Cell {
    double true_soc;
    double capacity_Ah;
    double ir_ohm;
    double temperature;
    double voltage;
    SOC_Kalman kalman;

    Cell(double nominal_capacity_Ah, double nominal_ir, std::mt19937 &gen) {
        std::normal_distribution<double> cap_var(1.0, 0.02); // ±2%
        std::normal_distribution<double> ir_var(1.0, 0.05);  // ±5%
        capacity_Ah = nominal_capacity_Ah * cap_var(gen);
        ir_ohm = nominal_ir * ir_var(gen);

        true_soc = 0.5;
        temperature = 25.0;
        voltage = lookup_ocv(true_soc);
        kalman.soc = 0.5; // initial SOC guess
    }

    void apply_current(double current_A, double dt_s) {
        // True SOC changes
        double dAh = current_A * dt_s / 3600.0;
        true_soc -= dAh / capacity_Ah;
        if (true_soc < 0.0) true_soc = 0.0;
        if (true_soc > 1.0) true_soc = 1.0;

        // Terminal voltage = OCV(true_soc) - IR_drop
        voltage = lookup_ocv(true_soc) - current_A * ir_ohm;

        // Simple thermal rise
        temperature += (std::abs(current_A) / 100.0) * 0.05 * dt_s;
        temperature += (25.0 - temperature) * 0.001;
    }

    double measure_voltage() {
        static thread_local std::default_random_engine gen(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.002); // ±2mV
        return voltage + noise(gen);
    }

    void kalman_update(double current_A, double dt_s) {
        // Coulomb counting prediction
        double dAh = current_A * dt_s / 3600.0;
        kalman.predict(dAh, capacity_Ah);

        // IR drop used in measurement update
        double ir_drop = current_A * ir_ohm;
        double meas = measure_voltage();
        kalman.update(meas, current_A, ir_drop);
    }
};

// =====================================================
// Simulation parameters
constexpr int NUM_CELLS = 480;
constexpr double NOMINAL_CAPACITY_AH = 280.0;
constexpr double NOMINAL_IR = 0.0005;    // 0.5 mΩ
constexpr double SIM_DT_S = 0.1;         // 10Hz

int main() {
    init_ocv_table();

    std::cout << "Initializing 480 LiFePO4 cells with detailed SOC Kalman filter...\n";

    std::mt19937 gen(std::random_device{}());
    std::vector<Cell> cells;
    cells.reserve(NUM_CELLS);
    for (int i = 0; i < NUM_CELLS; i++) {
        cells.emplace_back(NOMINAL_CAPACITY_AH, NOMINAL_IR, gen);
    }

    // Random current pattern ±50A
    std::normal_distribution<double> current_dist(0.0, 50.0);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Run 100 simulation steps (~10 seconds)
    for (int step = 0; step < 100; step++) {
        double pack_current = current_dist(gen);
        for (auto &cell : cells) {
            cell.apply_current(pack_current, SIM_DT_S);
            cell.kalman_update(pack_current, SIM_DT_S);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    std::cout << "Simulated 480 cells for 10s with 2-state SOC Kalman filter in "
              << elapsed.count() << " ms\n";

    const auto &c0 = cells[0];
    std::cout << "Example cell true SOC=" << c0.true_soc*100 << "%, "
              << "Estimated SOC=" << c0.kalman.soc*100 << "%, "
              << "True V=" << c0.voltage << "V\n";

    return 0;
}
```

---

### ✅ **What’s new here?**

1. **High-res OCV table**

   * `init_ocv_table()` generates 101 points from SOC=0.00 → 1.00
   * Matches LiFePO₄’s very flat plateau around 3.29 V

2. **Kalman filter for SOC estimation**

   * Predicts SOC from **Coulomb counting**
   * Corrects SOC using **voltage deviation from OCV**
   * Includes a **voltage bias drift** state

3. **Outputs true vs estimated SOC**

   * So you can see how well the filter tracks after 10 s

---

### ✅ **Expected performance**

On an **i.MX93**:

* Still **<20 ms per 100 steps** for all 480 cells (10 Hz is trivial load).
* Output:

```
Simulated 480 cells for 10s with 2-state SOC Kalman filter in 8.3 ms
Example cell true SOC=49.7%, Estimated SOC=49.8%, True V=3.287V
```

---

### ✅ **Next Extensions**

Would you like me to:

* Add **individual cell current mismatch**?
* Add a **pack model** (sum voltage + IR + current sensor noise)?
* Include **temperature effects on OCV & IR**?

Should I also **log SOC error over time** to see how well the Kalman filter corrects?

Got it! Let’s **extend the cell model** so **temperature affects both OCV and internal resistance** realistically.

---

### ✅ **How temperature affects LiFePO₄ cells**

1. **OCV vs temperature**

   * Slight drop at **low temperatures (<10 °C)**.
   * Almost flat between **15–35 °C**.
   * Slight increase at **>40 °C** (but also increases aging).

   → We’ll scale OCV by a **small factor (±10 mV)** depending on T.

2. **Internal Resistance vs temperature**

   * At **0 °C** → IR ≈ **2× higher** than at 25 °C.
   * At **-20 °C** → IR ≈ **4× higher**.
   * At **>40 °C**, IR slightly **drops** (\~0.9×).

   → We’ll use an **exponential function**:

   ```
   IR(T) = nominal_IR * exp(k * (25 °C - T))
   ```

   where **k \~ 0.02/°C**.

---

### ✅ **Updated Changes**

1. `lookup_ocv(soc, temp)`

   * Adds a **temperature correction term**.

2. `ir_temp_factor(temp)`

   * Computes IR multiplier based on temperature.

3. `apply_current()`

   * Now adjusts **IR drop** based on temperature.
   * Warms cell when discharging/charging heavily.

---

### ✅ **New Code Snippets**

We **modify only a few places** in the previous simulation:

```cpp
// ================= Temperature Compensation =================

// Temperature effect on OCV: small deviation ±0.01V from nominal at 0°C & 50°C
double temp_ocv_correction(double temp_C) {
    if (temp_C < 25.0) {
        return -0.01 * (25.0 - temp_C) / 25.0; // ~-10 mV at 0°C
    } else if (temp_C > 35.0) {
        return +0.005 * (temp_C - 35.0) / 15.0; // ~+5 mV at 50°C
    }
    return 0.0; // flat near 25°C
}

// Temperature effect on IR (higher at low temp)
double ir_temp_factor(double temp_C) {
    // At 25°C → 1.0, at 0°C → ~2×, at -20°C → ~4×
    return exp((25.0 - temp_C) * 0.02);
}

// Modified OCV lookup with temp correction
double lookup_ocv_temp(double soc, double temp_C) {
    return lookup_ocv(soc) + temp_ocv_correction(temp_C);
}
```

Then update the **Cell model**:

```cpp
struct Cell {
    double true_soc;
    double capacity_Ah;
    double nominal_ir_ohm;
    double temperature;
    double voltage;
    SOC_Kalman kalman;

    Cell(double nominal_capacity_Ah, double nominal_ir, std::mt19937 &gen) {
        std::normal_distribution<double> cap_var(1.0, 0.02); // ±2%
        std::normal_distribution<double> ir_var(1.0, 0.05);  // ±5%
        capacity_Ah = nominal_capacity_Ah * cap_var(gen);
        nominal_ir_ohm = nominal_ir * ir_var(gen);

        true_soc = 0.5;
        temperature = 25.0;
        voltage = lookup_ocv_temp(true_soc, temperature);
        kalman.soc = 0.5; // initial SOC guess
    }

    void apply_current(double current_A, double dt_s) {
        // True SOC changes (Coulomb counting)
        double dAh = current_A * dt_s / 3600.0;
        true_soc -= dAh / capacity_Ah;
        if (true_soc < 0.0) true_soc = 0.0;
        if (true_soc > 1.0) true_soc = 1.0;

        // Temp-adjusted IR
        double effective_ir = nominal_ir_ohm * ir_temp_factor(temperature);

        // Terminal voltage = OCV(SOC, temp) - I * IR(temp)
        voltage = lookup_ocv_temp(true_soc, temperature) - current_A * effective_ir;

        // Simple thermal model: heat generation ∝ I² * R + ambient cooling
        double heat_gen = (current_A * current_A * effective_ir) * 0.01;
        double ambient = 25.0;
        double cooling = (ambient - temperature) * 0.001;
        temperature += (heat_gen + cooling) * dt_s;
    }

    double measure_voltage() {
        static thread_local std::default_random_engine gen(std::random_device{}());
        std::normal_distribution<double> noise(0.0, 0.002); // ±2mV noise
        return voltage + noise(gen);
    }

    void kalman_update(double current_A, double dt_s) {
        // Prediction
        double dAh = current_A * dt_s / 3600.0;
        kalman.predict(dAh, capacity_Ah);

        // Temp-adjusted IR drop for update
        double effective_ir = nominal_ir_ohm * ir_temp_factor(temperature);
        double ir_drop = current_A * effective_ir;

        // Measurement update
        double meas = measure_voltage();
        kalman.update(meas, current_A, ir_drop);
    }
};
```

---

### ✅ **Now the simulation reacts to temperature!**

* At **low temperature**, cells:

  * Have **higher IR**, more voltage sag, slower heating
  * Slightly **lower OCV**

* At **high temperature**, cells:

  * Slightly lower IR, small **+5 mV OCV bump**

---

### ✅ **Expected Behavior**

* If you start at **0 °C**, you’ll see:

  * More voltage drop under load
  * Kalman filter may **mis-estimate SOC initially**, then correct

* If you simulate **heavy discharge**, temperature rises → IR decreases → voltage improves slightly.

---

### ✅ **Want even more realism?**

We can:

* Add **capacity loss at low temp** (LiFePO₄ loses \~20% usable capacity at 0 °C)
* Add **temperature-dependent Coulombic efficiency**
* Add **ambient thermal coupling**

---

Would you like me to:

✅ **Integrate this into the full 480-cell benchmark simulation** (with temperature logging)?
✅ Also log **SOC error vs temperature** over time?

Or do you want a **pack-level thermal model** (cells heat each other + ambient)?
Good question! On an **i.MX93** (Cortex‑A55 + Cortex‑M33 cores), you can get a **big speedup** by using NEON SIMD for:

✅ **OCV lookup & interpolation**
✅ **IR temperature correction**
✅ **Kalman predict/update steps**
✅ **Thermal model (I²R heating)**

Let’s break it down:

---

## 🔹 **Where SIMD helps**

1. **OCV Lookup + temp correction**

   * Right now we do a **per-cell `upper_bound`** search → O(n).
   * With SIMD, we could **vectorize interpolation** for many cells at once (but still need scalar search).
   * Expected speedup: **\~2×–3×**

2. **IR temperature scaling (`exp(k*(25−T))`)**

   * NEON can compute multiple exponentials in parallel.
   * Or better: **approximate exp() with a polynomial** & vectorize.
   * Expected speedup: **\~4×** for this part

3. **Kalman filter math**

   * Kalman is **linear scalar ops**, NEON can easily parallelize **480 updates at once**.
   * Expected speedup: **\~5×**

4. **Coulomb counting + heating model**

   * Pure arithmetic, trivially vectorizable.
   * Expected speedup: **\~8×**

---

## 🔹 **Rough performance estimates**

For **480 cells × 10 Hz** = **4800 updates/sec**:

| Version               | Time/update/cell | Total (480)   | Notes                      |
| --------------------- | ---------------- | ------------- | -------------------------- |
| **Scalar (baseline)** | \~2–5 µs         | \~1–2 ms      | Works but eats \~5–10% CPU |
| **NEON SIMD**         | \~0.3–0.6 µs     | \~0.15–0.3 ms | \~5× faster                |

So you’d likely get a **5–7× total speedup**, meaning the simulation would use **<2% CPU** on the i.MX93 **even at 10 Hz**.

---

## 🔹 **How to vectorize**

Example for **IR temp factor**:

```cpp
#include <arm_neon.h>

// temp[]: 4 floats (Celsius)
// out_ir_factor[]: 4 floats
inline void neon_ir_temp_factor(const float* temp, float* out_ir_factor) {
    // k = 0.02f
    float32x4_t t = vld1q_f32(temp);         // Load 4 temps
    float32x4_t diff = vsubq_f32(vdupq_n_f32(25.0f), t); 
    float32x4_t kdiff = vmulq_n_f32(diff, 0.02f);
    // Approximate exp(x) with 1 + x + x^2/2 (fast enough for small deltas)
    float32x4_t x2 = vmulq_f32(kdiff, kdiff);
    float32x4_t approx = vaddq_f32(vaddq_f32(vdupq_n_f32(1.0f), kdiff),
                                   vmulq_n_f32(x2, 0.5f));
    vst1q_f32(out_ir_factor, approx);        // Store result
}
```

We’d apply this for **all 480 cells in chunks of 4**.

Same trick applies to:

* **OCV interpolation**
* **Kalman prediction equations**
* **Coulomb counting**

---

## 🔹 **Even faster?**

* **Batch process all 480 cells** → reduce cache misses
* Use **Eigen or Arm Compute Library** for vectorized math
* If you need **10× more speed**, offload to Cortex‑M33 (baremetal NEON) or even GPU.

---

## ✅ Next step

Would you like me to:

✅ **Show a NEON-vectorized version of the core simulation loop** (SOC + temp + voltage)?
✅ Or keep it scalar for now and just add a SIMD-friendly layout (SoA vs AoS)?

Also, should I:

* Benchmark **scalar vs NEON** on a synthetic dataset?
* Or prepare a **drop-in NEON kernel** you can test on the i.MX93?

