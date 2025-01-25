/**
 ******************************************************************************
 * @Copyright (C), 2025, Fractal EMS Inc. 
 * @file: fractal_test.h
 * @author: Phil Wilshire
 * @Description:
 *     Fractal BMS Test System
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2025.01.23
 ******************************************************************************
 */
#ifndef __FRACTAL_TEST_H
#define __FRACTAL_TEST_H
// Structs
struct InputVector {
    int offset;
    std::vector<uint16_t> data;
    double timestamp;
};

struct MatchObject {
    std::vector<uint16_t> vector;
    std::vector<uint16_t> mask;    // Mask for each element
    std::vector<uint16_t> tolerance;  // Tolerance for each element
    std::vector<uint16_t> weight;     // Weight for each element
    // std::vector<std::string> mask_defs;    // Mask  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
    // std::vector<std::string> tolerance_defs;    // tolerance  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
    // std::vector<std::string> weight_defs;    // weight  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
  
    std::map<int, std::vector<int>> matches;
    std::string name;
};

// map of variables
struct ConfigItem {
    std::string name;
    std::string system;
    std::string type;
    int offset;
    int size;
};

// Hash Function for Vectors
struct VectorHash {
    std::size_t operator()(const std::vector<uint16_t>& vec) const {
        std::size_t hash = 0;
        for (const auto& val : vec) {
            hash ^= std::hash<uint16_t>()(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

// Global ofstream for logging
std::ofstream log_file;

// Macro to open log file
#define log_open(path) do { log_file.open(path, std::ios::out | std::ios::app); if (!log_file.is_open()) std::cerr << "Failed to open log file: " << path << std::endl; } while (0)

// Macro to log messages
#define log_msg if (!log_file) std::cerr << "Log file is not open!" << std::endl; else log_file

// Macro to close log file
#define log_close() if (log_file.is_open()) log_file.close()

// Macro to delete log file
#define log_delete(path) do { \
    if (std::filesystem::exists(path)) { \
        std::filesystem::remove(path); \
    } \
} while (0)


#endif