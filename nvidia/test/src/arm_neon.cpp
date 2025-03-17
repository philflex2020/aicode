#include <arm_neon.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <limits>
#include <chrono>
#include <algorithm>  // Include this for std::generate and other algorithms
//g++ -o arm_neon arm_neon.cpp -march=native -O3 -std=c++11
//no g++ -o arm_neon arm_neon.cpp -mfpu=neon -O3 -std=c++11
struct Vector {
    std::vector<float> elements;

    // Calculate distance using NEON SIMD instructions
    float neonDistance(const Vector& other) const {
        float32x4_t sum_vec = vdupq_n_f32(0.0f);  // Initialize sum vector to zero
        size_t limit = elements.size() / 4 * 4; // Ensure we only process full blocks of 4
        for (size_t i = 0; i < limit; i += 4) {
            float32x4_t v1 = vld1q_f32(&elements[i]);
            float32x4_t v2 = vld1q_f32(&other.elements[i]);
            float32x4_t diff = vsubq_f32(v1, v2);
            float32x4_t sq = vmulq_f32(diff, diff);
            sum_vec = vaddq_f32(sum_vec, sq);
        }
        // Accumulate remaining elements
        float sum = vaddvq_f32(sum_vec); // NEON fold reduction
        for (size_t i = limit; i < elements.size(); ++i) {
            float diff = elements[i] - other.elements[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }
};

int neonFindNearestVector(const std::vector<Vector>& dataset, const Vector& target) {
    int nearestIndex = -1;
    float nearestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < dataset.size(); ++i) {
        float dist = target.neonDistance(dataset[i]);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearestIndex = i;
        }
    }

    return nearestIndex;
}

// Main function to execute the NEON vector search
int main() {
    const size_t numVectors = 1024;
    const size_t dims = 10000;
    std::vector<Vector> dataset(numVectors, Vector{std::vector<float>(dims)});
    Vector target{std::vector<float>(dims)};

    // Initialize vectors with random data
    for (auto& vec : dataset) {
        std::generate(vec.elements.begin(), vec.elements.end(), []() { return static_cast<float>(rand()) / RAND_MAX; });
    }
    std::generate(target.elements.begin(), target.elements.end(), []() { return static_cast<float>(rand()) / RAND_MAX; });

    // Measure the performance
    auto start = std::chrono::high_resolution_clock::now();
    int nearestIndex = neonFindNearestVector(dataset, target);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Nearest vector index (NEON): " << nearestIndex << std::endl;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds." << std::endl;

    return 0;
}