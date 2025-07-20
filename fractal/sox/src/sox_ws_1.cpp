// sox_ws.cpp
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>
#include <fstream>      // ✅ Required for std::ifstream
#include <sstream>      // ✅ Required for std::stringstream

#include <httplib.h>            // lightweight header-only HTTP server
#include <nlohmann/json.hpp>    // JSON library

using json = nlohmann::json;

// g++ -std=c++17 -o sox_ws -I . sox_ws.cpp  -pthread


#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "shared_mem.h"

SharedMemoryLayout *shm_ptr = nullptr;

void init_shared_memory() {
    int fd = shm_open("/sox_cells", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SharedMemoryLayout));
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
void attach_shared_memory() {
    int fd = shm_open("/sox_cells", O_RDONLY, 0666);
    if (fd < 0) {
        perror("shm_open failed");
        exit(1);
    }
    shm_ptr = (SharedMemoryLayout *)mmap(
        nullptr,
        sizeof(SharedMemoryLayout),
        PROT_READ,
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

void update_shared_memory(const std::vector<Cell> &cells, double pack_current) {
    double total_voltage = 0.0;
    double min_v = 9999, max_v = 0, sum_v = 0;
    double min_t = 9999, max_t = -9999, sum_t = 0;
    double min_soc = 1.0, max_soc = 0, sum_soc = 0;

    for (int i = 0; i < NUM_CELLS; i++) {
        const auto &c = cells[i];

        // Scale and store per-cell
        shm_ptr->cells[i].voltage_mV = (uint16_t)std::round(c.voltage * 1000.0);
        shm_ptr->cells[i].true_soc_d1 = (uint16_t)std::round(c.true_soc * 1000.0 / 10.0);
        shm_ptr->cells[i].est_soc_d1 = (uint16_t)std::round(c.kalman.soc * 1000.0 / 10.0);
        shm_ptr->cells[i].temp_dC = (uint16_t)std::round(c.temperature * 10.0);

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

    shm_ptr->stats.total_voltage_dV = (uint16_t)std::round(total_voltage * 100.0); // decivolts
    shm_ptr->stats.pack_current_dA  = (int16_t)std::round(pack_current * 10.0);

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
double pack_current_setpoint = 0.0;

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
                cell.apply_current(pack_current_setpoint, SIM_DT_S);
                cell.kalman_update(pack_current_setpoint, SIM_DT_S);
            }

            // for (auto &cell : cells) {
            //     cell.apply_current(pack_current, SIM_DT_S);
            //     cell.kalman_update(pack_current, SIM_DT_S);
            // }
        }

        // Sleep until next tick
        double now = ref_time_dbl();
        double delay = next_tick - now;
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        }
    }
}


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
// Simple HTTP/JSON server
void run_http_server() {
    httplib::Server svr;
    // Route for `/`
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        serve_index(res);
    });

    //double pack_current_setpoint = 0.0;
    svr.Get("/sm_cells", [](const httplib::Request &, httplib::Response &res) {
        nlohmann::json j;
        j["total_voltage"] = shm_ptr->stats.total_voltage_dV / 100.0;
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

        std::vector<nlohmann::json> cell_array;
        for (int i = 0; i < 10; i++) {
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


    svr.Get("/cells", [&](const httplib::Request&, httplib::Response &res) {
        nlohmann::json j;
        double total_v = 0.0;
        double min_v = 999.0, max_v = 0.0;
        double min_temp = 999.0, max_temp = -999.0;
        double min_soc = 1.0, max_soc = 0.0;
        double sum_v = 0.0, sum_temp = 0.0, sum_soc = 0.0;

        nlohmann::json cells_json = nlohmann::json::array();
        for (auto &c : cells) {
            double v = c.voltage;
            double t = c.temperature;
            double s = c.true_soc;

            total_v += v;
            sum_v += v; sum_temp += t; sum_soc += s;

            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
            if (t < min_temp) min_temp = t;
            if (t > max_temp) max_temp = t;
            if (s < min_soc) min_soc = s;
            if (s > max_soc) max_soc = s;

            cells_json.push_back({
                {"voltage", v},
                {"true_soc", s},
                {"est_soc", c.kalman.soc},
                {"temp", t}
            });
        }

        double avg_v = sum_v / cells.size();
        double avg_temp = sum_temp / cells.size();
        double avg_soc = sum_soc / cells.size();

        j["total_voltage"] = total_v;
        j["current_setpoint"] = pack_current_setpoint;
        j["cells"] = cells_json;

        j["stats"] = {
            {"max_voltage", max_v},
            {"min_voltage", min_v},
            {"avg_voltage", avg_v},
            {"max_temp", max_temp},
            {"min_temp", min_temp},
            {"avg_temp", avg_temp},
            {"max_soc", max_soc},
            {"min_soc", min_soc},
            {"avg_soc", avg_soc}
        };

        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/x2cells", [&](const httplib::Request&, httplib::Response &res) {
        nlohmann::json j;
        double total_v = 0.0;
        nlohmann::json cells_json = nlohmann::json::array();

        for (auto &c : cells) {
            total_v += c.voltage;
            cells_json.push_back({
                {"voltage", c.voltage},
                {"true_soc", c.true_soc},
                {"est_soc", c.kalman.soc},
                {"temp", c.temperature}
            });
        }
        j["total_voltage"] = total_v;
        j["current_setpoint"] = pack_current_setpoint;
        j["cells"] = cells_json;

        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/set_current", [&](const httplib::Request &req, httplib::Response &res) {
        auto j = nlohmann::json::parse(req.body);
        pack_current_setpoint = j["current"].get<double>();
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    svr.Get("/xcells", [&](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["cells"] = nlohmann::json::array();
        for (int i = 0; i < 10; i++) { // send first 10 cells
            j["cells"].push_back({
                {"id", i},
                {"soc", cells[i].kalman.soc * 100.0},
                {"voltage", cells[i].voltage},
                {"temp", cells[i].temperature}
            });
        }
        res.set_content(j.dump(), "application/json");
    });


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
