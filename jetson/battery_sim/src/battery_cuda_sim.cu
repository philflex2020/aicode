//nvcc -std=c++17 -ccbin g++-11 battery_cuda_sim.cu -o battery_cuda_sim
#include "battery_sim.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cuda_runtime.h>

#include <cuda_runtime.h>
//#include <math_functions.h>
#include <math.h>

#define SHM_NAME "/battery_sim_shm"
#define SHM_SIZE sizeof(BatterySim)

BatterySim* g_battery_sim = nullptr;

// CUDA kernel to simulate updating voltage and temperature
__global__ void update_cells(CellData* cells, int num_cells, float t) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < num_cells) {
        cells[idx].voltage += 0.001f * sinf(t + idx);
        cells[idx].temperature += 0.01f * cosf(t + idx);
    }
}

bool setup_shared_memory() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "❌ shm_open failed: " << strerror(errno) << "\n";
        return false;
    }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "❌ mmap failed: " << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    g_battery_sim = static_cast<BatterySim*>(ptr);
    close(fd);
    return true;
}

int main() {
    if (!setup_shared_memory()) return 1;

    std::cout << "🧪 CUDA battery simulator started\n";

    while (true) {
        for (int r = 0; r < NUM_RACKS; ++r) {
            for (int m = 0; m < NUM_MODULES; ++m) {
                CellData* dev_cells;
                auto& module = g_battery_sim->racks[r].modules[m];

                cudaMalloc(&dev_cells, sizeof(CellData) * NUM_CELLS);
                cudaMemcpy(dev_cells, module.cells, sizeof(CellData) * NUM_CELLS, cudaMemcpyHostToDevice);

                float t = static_cast<float>(time(nullptr));
                update_cells<<<(NUM_CELLS + 31)/32, 32>>>(dev_cells, NUM_CELLS, t);
                cudaDeviceSynchronize();

                cudaMemcpy(module.cells, dev_cells, sizeof(CellData) * NUM_CELLS, cudaMemcpyDeviceToHost);
                cudaFree(dev_cells);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}