// pattern_match_cuda.cpp
#include <iostream>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cuda_runtime.h>
#include <taos.h>  // TDengine header
#include <thread>

using json = nlohmann::json;

constexpr int SAMPLE_SIZE = 1024;
constexpr int NUM_SYSTEMS = 128;
constexpr int NUM_SAMPLES = 1000;
constexpr int THRESHOLD = 50000;
constexpr int TOTAL_SAMPLES = NUM_SYSTEMS * NUM_SAMPLES;

__global__ void match_kernel(const int16_t* data, const int16_t* patterns, int* results, int total_samples, int num_patterns) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_samples) return;
    const int16_t* sample = &data[idx * SAMPLE_SIZE];

    for (int p = 0; p < num_patterns; ++p) {
        const int16_t* pattern = &patterns[p * SAMPLE_SIZE];
        int sad = 0;
        for (int i = 0; i < SAMPLE_SIZE; ++i) {
            sad += abs(sample[i] - pattern[i]);
        }
        results[idx * num_patterns + p] = sad;
    }
}

int receive_json_patterns(std::vector<std::vector<int16_t>>& patterns, int port = 9999) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);
    int client_fd = accept(server_fd, nullptr, nullptr);

    char buffer[65536] = {};
    read(client_fd, buffer, sizeof(buffer));
    close(server_fd);

    json j = json::parse(buffer);
    for (auto& arr : j["patterns"]) {
        std::vector<int16_t> pat(SAMPLE_SIZE);
        for (int i = 0; i < SAMPLE_SIZE && i < arr.size(); ++i)
            pat[i] = arr[i];
        patterns.push_back(std::move(pat));
    }
    return 0;
}

void save_results_to_file(const json& results, const std::string& filename) {
    std::ofstream out(filename);
    out << results.dump(2);
    out.close();
}

void save_results_to_tdengine(const json& results) {
    TAOS* taos = taos_connect(nullptr, nullptr, nullptr, "test", 0);
    if (!taos) {
        std::cerr << "Failed to connect to TDengine\n";
        return;
    }

    taos_query(taos, "CREATE DATABASE IF NOT EXISTS test");
    taos_query(taos, "USE test");
    taos_query(taos, "CREATE STABLE IF NOT EXISTS matches (ts TIMESTAMP, system INT, time INT, pattern INT, score INT) TAGS(tag INT)");

    for (const auto& r : results) {
        char query[256];
        snprintf(query, sizeof(query),
            "INSERT INTO d_match_%d USING matches TAGS(1) VALUES (now, %d, %d, %d, %d)",
            r["system"].get<int>(),
            r["system"].get<int>(),
            r["time"].get<int>(),
            r["pattern"].get<int>(),
            r["score"].get<int>());
        taos_query(taos, query);
    }

    taos_close(taos);
}

void stream_results_to_websocket(const json& results, const std::string& address = "127.0.0.1", int port = 9001) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Could not connect to WebSocket server on " << address << ":" << port << "\n";
        return;
    }

    std::string json_str = results.dump();
    send(sock, json_str.c_str(), json_str.size(), 0);
    close(sock);
}

int main() {
    std::vector<int16_t> host_data(TOTAL_SAMPLES * SAMPLE_SIZE);
    for (auto& val : host_data) val = rand() % 100;

    std::vector<std::vector<int16_t>> pattern_data;
    receive_json_patterns(pattern_data);

    int num_patterns = pattern_data.size();
    std::vector<int16_t> flat_patterns(num_patterns * SAMPLE_SIZE);
    for (int p = 0; p < num_patterns; ++p)
        std::memcpy(&flat_patterns[p * SAMPLE_SIZE], pattern_data[p].data(), SAMPLE_SIZE * sizeof(int16_t));

    int16_t *dev_data, *dev_patterns;
    int *dev_results;
    cudaMalloc(&dev_data, host_data.size() * sizeof(int16_t));
    cudaMalloc(&dev_patterns, flat_patterns.size() * sizeof(int16_t));
    cudaMalloc(&dev_results, TOTAL_SAMPLES * num_patterns * sizeof(int));

    cudaMemcpy(dev_data, host_data.data(), host_data.size() * sizeof(int16_t), cudaMemcpyHostToDevice);
    cudaMemcpy(dev_patterns, flat_patterns.data(), flat_patterns.size() * sizeof(int16_t), cudaMemcpyHostToDevice);

    std::vector<int> thread_sizes = {128, 256, 512};
    for (int threadsPerBlock : thread_sizes) {
        int blocks = (TOTAL_SAMPLES + threadsPerBlock - 1) / threadsPerBlock;

        auto start = std::chrono::high_resolution_clock::now();
        match_kernel<<<blocks, threadsPerBlock>>>(dev_data, dev_patterns, dev_results, TOTAL_SAMPLES, num_patterns);
        cudaDeviceSynchronize();
        auto end = std::chrono::high_resolution_clock::now();

        std::vector<int> host_results(TOTAL_SAMPLES * num_patterns);
        cudaMemcpy(host_results.data(), dev_results, host_results.size() * sizeof(int), cudaMemcpyDeviceToHost);

        json result_json = json::array();
        for (int s = 0; s < TOTAL_SAMPLES; ++s) {
            int sys = s / NUM_SAMPLES;
            int t = s % NUM_SAMPLES;
            for (int p = 0; p < num_patterns; ++p) {
                int score = host_results[s * num_patterns + p];
                if (score < THRESHOLD) {
                    result_json.push_back({{"system", sys}, {"time", t}, {"pattern", p}, {"score", score}});
                }
            }
        }

        std::string filename = "results_threads_" + std::to_string(threadsPerBlock) + ".json";
        save_results_to_file(result_json, filename);
        save_results_to_tdengine(result_json);
        stream_results_to_websocket(result_json);

        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Thread block size " << threadsPerBlock << ": " << elapsed.count() << " seconds\n";
    }

    cudaFree(dev_data);
    cudaFree(dev_patterns);
    cudaFree(dev_results);
    return 0;
}
