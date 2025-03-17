#include <vector>
#include <iostream>
#include <cmath>
#include <limits>
#include <chrono>
#include <algorithm>
#include <cstdlib>    // For rand()
//g++ -o arm_norm arm_norm.cpp -O3 -std=c++11
struct Vector {
    std::vector<float> elements;

    // Calculate the Euclidean distance between two vectors
    float distance(const Vector& other) const {
        float sum = 0.0f;
        for (size_t i = 0; i < elements.size(); i++) {
            float diff = elements[i] - other.elements[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }
};

// Function to find the index of the nearest vector in the dataset to the target vector
int findNearestVector(const std::vector<Vector>& dataset, const Vector& target) {
    int nearestIndex = -1;
    float nearestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < dataset.size(); i++) {
        float dist = target.distance(dataset[i]);
        if (dist < nearestDistance) {
            nearestDistance = dist;
            nearestIndex = i;
        }
    }

    return nearestIndex;
}

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
    int nearestIndex = findNearestVector(dataset, target);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Nearest vector index: " << nearestIndex << std::endl;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds." << std::endl;

    return 0;
}
