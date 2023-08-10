#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// Sample PMU Data Structure (Frame Format)
struct PMUData {
    uint64_t timeStamp;
    float voltageMagnitude;
    float voltageAngle;
};

// Decode PMU Data
PMUData decodePMUData(const std::vector<uint8_t>& rawData) {
    PMUData pmuData;

    if (rawData.size() < sizeof(PMUData)) {
        std::cerr << "Invalid PMU data size" << std::endl;
        return pmuData;
    }

    //std::
    memcpy(&pmuData, rawData.data(), sizeof(PMUData));

    pmuData.timeStamp = be64toh(pmuData.timeStamp);

    return pmuData;
}

int main() {
    // Simulated PMU data (16 bytes)
    std::vector<uint8_t> rawData = {
        0x00, 0x00, 0x00, 0x00,  // High-order 32 bits
        0x00, 0x00, 0x00, 0x01, // Time Stamp
        0x00, 0x00, 0x80, 0x3F, // Voltage Magnitude (1.0)
        0x00, 0x00, 0x00, 0x00  // Voltage Angle (0.0)
    };

    // Decode PMU data
    PMUData pmuData = decodePMUData(rawData);
    // Convert timestamp to system_clock::time_point
    //auto timeStamp = std::chrono::system_clock::from_time_t(static_cast<time_t>(pmuData.timeStamp));

    // Format timestamp as string
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(reinterpret_cast<const time_t*>(&pmuData.timeStamp)), "%Y-%m-%d %H:%M:%S");

    //oss << std::put_time(std::gmtime(&pmuData.timeStamp), "%Y-%m-%d %H:%M:%S");
    std::string formattedTime = oss.str();


    // Print decoded values
    std::cout << "Time Stamp: " << pmuData.timeStamp << std::endl;
    std::cout << "Formatted Time Stamp: " << formattedTime << std::endl;
    std::cout << "Voltage Magnitude: " << pmuData.voltageMagnitude << std::endl;
    std::cout << "Voltage Angle: " << pmuData.voltageAngle << std::endl;

    return 0;
}