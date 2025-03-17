#include <cuda_runtime.h>
#include <iostream>
#include <cmath>
#include <limits>
//sudo apt-get install nvidia-cuda-dev=11.5.1-1ubuntu1

//nvcc -o my_cuda_program cuda_vecs.cpp -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcudart

__global__ void findNearest(float *matrix, float *target, int *resultIndex, float *resultDistance, int n, int dims) {
    int idx = threadIdx.x + blockDim.x * blockIdx.x;
    if (idx >= n) return;  // Ensure we do not go out of bounds

    float distance = 0;
    for (int d = 0; d < dims; d++) {
        float diff = matrix[idx * dims + d] - target[d];
        distance += diff * diff;
    }
    distance = sqrt(distance);

    // Use atomic operations to find the minimum distance
    atomicMin(resultDistance, distance);
    if (distance == *resultDistance) {
        *resultIndex = idx;
    }
}

void initializeData(float *data, int size) {
    for (int i = 0; i < size; i++) {
        data[i] = rand() / (float)RAND_MAX;  // Fill with random data
    }
}

int main() {
    const int numVectors = 1024;
    const int dims = 128;
    const int dataSize = numVectors * dims;
    float *h_matrix = new float[dataSize];
    float *h_target = new float[dims];
    int h_resultIndex = 0;
    float h_resultDistance = std::numeric_limits<float>::max();

    // Initialize data
    initializeData(h_matrix, dataSize);
    initializeData(h_target, dims);

    // Allocate device memory
    float *d_matrix, *d_target, *d_resultDistance;
    int *d_resultIndex;
    cudaMalloc(&d_matrix, dataSize * sizeof(float));
    cudaMalloc(&d_target, dims * sizeof(float));
    cudaMalloc(&d_resultIndex, sizeof(int));
    cudaMalloc(&d_resultDistance, sizeof(float));

    // Copy data to device
    cudaMemcpy(d_matrix, h_matrix, dataSize * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_target, h_target, dims * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_resultDistance, &h_resultDistance, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_resultIndex, &h_resultIndex, sizeof(int), cudaMemcpyHostToDevice);

    // Kernel launch parameters
    int blockSize = 256;
    int numBlocks = (numVectors + blockSize - 1) / blockSize;

    // Launch kernel
    findNearest<<<numBlocks, blockSize>>>(d_matrix, d_target, d_resultIndex, d_resultDistance, numVectors, dims);

    // Copy result back to host
    cudaMemcpy(&h_resultIndex, d_resultIndex, sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_resultDistance, d_resultDistance, sizeof(float), cudaMemcpyDeviceToHost);

    std::cout << "Nearest vector is at index: " << h_resultIndex << " with distance: " << h_resultDistance << std::endl;

    // Free resources
    cudaFree(d_matrix);
    cudaFree(d_target);
    cudaFree(d_resultIndex);
    cudaFree(d_resultDistance);
    delete[] h_matrix;
    delete[] h_target;

    return 0;
}