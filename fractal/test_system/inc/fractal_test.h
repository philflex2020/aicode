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
    std::vector<uint16_t> bitmask;    // Mask for each element
    std::vector<uint16_t> tolerance;  // Tolerance for each element
    std::vector<uint16_t> weight;     // Weight for each element
    std::map<int, std::vector<int>> matches;
    std::string name;
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


#endif