
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

