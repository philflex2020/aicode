#include <arm_neon.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <complex>
#include <limits>
#include <chrono>

struct VectorStats {
    std::vector<float> data;

    // Calculate max, min, sum using NEON
    void calculateStats(float &maxVal, float &minVal, float &sumVal) {
        float32x4_t maxVec = vdupq_n_f32(std::numeric_limits<float>::lowest());
        float32x4_t minVec = vdupq_n_f32(std::numeric_limits<float>::max());
        float32x4_t sumVec = vdupq_n_f32(0.0f);

        for (size_t i = 0; i < data.size(); i += 4) {
            float32x4_t vec = vld1q_f32(&data[i]);
            maxVec = vmaxq_f32(maxVec, vec);
            minVec = vminq_f32(minVec, vec);
            sumVec = vaddq_f32(sumVec, vec);
        }

        // Reduce vectors to single values
        float32x2_t maxReduce = vpmax_f32(vget_low_f32(maxVec), vget_high_f32(maxVec));
        float32x2_t minReduce = vpmin_f32(vget_low_f32(minVec), vget_high_f32(minVec));
        float32x2_t sumReduce = vpadd_f32(vget_low_f32(sumVec), vget_high_f32(sumVec));

        maxVal = std::max(vget_lane_f32(maxReduce, 0), vget_lane_f32(maxReduce, 1));
        minVal = std::min(vget_lane_f32(minReduce, 0), vget_lane_f32(minReduce, 1));
        sumVal = sumReduce[0] + sumReduce[1];
    }

    // Placeholder for 3 Sigma calculation
    float calculateThreeSigma() {
        float mean = std::accumulate(data.begin(), data.end(), 0.0f) / data.size();
        float sq_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0f,
                        [](float acc, float x) { return acc + x; },
                        [mean](float x, float y) { return (x - mean) * (y - mean); });
        float stdev = std::sqrt(sq_sum / data.size());
        return 3 * stdev;
    }
};

int main() {
    // Assuming each vector has 10,000 elements
    std::vector<VectorStats> dataset(1024, VectorStats{std::vector<float>(10000, 0.0f)});
    
    // Initialize vectors with random data
    for (auto& vec : dataset) {
        std::generate(vec.data.begin(), vec.data.end(), []() { return static_cast<float>(rand()) / RAND_MAX * 100; });
    }
    int idx = 0;
    for (auto& vec : dataset) {
        float maxVal, minVal, sumVal;
        vec.calculateStats(maxVal, minVal, sumVal);
        float threeSigma = vec.calculateThreeSigma();

        std::cout<< " ["<< idx <<  "] Max: " << maxVal << ", Min: " << minVal << ", Sum: " << sumVal << ", 3 Sigma: " << threeSigma << std::endl;
        idx++;
    }


// Measure the performance
    auto start = std::chrono::high_resolution_clock::now();

    // Example usage of the statistics functions
    for (auto& vec : dataset) {
        float maxVal, minVal, sumVal;
        vec.calculateStats(maxVal, minVal, sumVal);
        float threeSigma = vec.calculateThreeSigma();

        //std::cout << "Max: " << maxVal << ", Min: " << minVal << ", Sum: " << sumVal << ", 3 Sigma: " << threeSigma << std::endl;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds." << std::endl;



    return 0;
}
