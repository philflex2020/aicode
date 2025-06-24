// pattern_match_simd.cpp
#include <iostream>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using json = nlohmann::json;

#if defined(__aarch64__)
#include <arm_neon.h>
#define PLATFORM_NEON
#elif defined(__x86_64__)
#include <immintrin.h>
#define PLATFORM_AVX
#else
#define PLATFORM_SCALAR
#endif

constexpr int SAMPLE_SIZE = 1024;
constexpr int NUM_SYSTEMS = 128;
constexpr int NUM_SAMPLES = 1000;
constexpr int THRESHOLD = 50000;
const int NUM_THREADS = std::max(1u, std::thread::hardware_concurrency());

inline const int16_t* get_sample(const std::vector<int16_t>& data, int system, int sample_idx) {
    return &data[(system * NUM_SAMPLES + sample_idx) * SAMPLE_SIZE];
}

#ifdef PLATFORM_NEON
int simd_sad_neon(const int16_t* a, const int16_t* b) {
    int32x4_t sum = vdupq_n_s32(0);
    for (int i = 0; i < SAMPLE_SIZE; i += 8) {
        int16x8_t va = vld1q_s16(a + i);
        int16x8_t vb = vld1q_s16(b + i);
        int16x8_t diff = vabdq_s16(va, vb);
        int32x4_t d0 = vmovl_s16(vget_low_s16(diff));
        int32x4_t d1 = vmovl_s16(vget_high_s16(diff));
        sum = vaddq_s32(sum, d0);
        sum = vaddq_s32(sum, d1);
    }
    return vaddvq_s32(sum);
}
#endif

#ifdef PLATFORM_AVX
int simd_sad_avx(const int16_t* a, const int16_t* b) {
    __m256i total = _mm256_setzero_si256();
    for (int i = 0; i < SAMPLE_SIZE; i += 16) {
        __m256i va = _mm256_loadu_si256((__m256i*)(a + i));
        __m256i vb = _mm256_loadu_si256((__m256i*)(b + i));
        __m256i diff1 = _mm256_subs_epi16(va, vb);
        __m256i diff2 = _mm256_subs_epi16(vb, va);
        __m256i abs_diff = _mm256_or_si256(diff1, diff2);
        total = _mm256_add_epi16(total, abs_diff);
    }
    alignas(32) int16_t temp[16];
    _mm256_store_si256((__m256i*)temp, total);
    int result = 0;
    for (int i = 0; i < 16; ++i) result += temp[i];
    return result;
}
#endif

#ifdef PLATFORM_SCALAR
int simd_sad_scalar(const int16_t* a, const int16_t* b) {
    int sad = 0;
    for (int i = 0; i < SAMPLE_SIZE; ++i) {
        sad += std::abs(a[i] - b[i]);
    }
    return sad;
}
#endif

int simd_sad(const int16_t* a, const int16_t* b) {
#ifdef PLATFORM_NEON
    return simd_sad_neon(a, b);
#elif defined(PLATFORM_AVX)
    return simd_sad_avx(a, b);
#else
    return simd_sad_scalar(a, b);
#endif
}

std::vector<std::tuple<int, int, int>> find_pattern_matches(
    const std::vector<const int16_t*>& patterns,
    const std::vector<int16_t>& data,
    int num_systems,
    int num_samples
) {
    std::vector<std::tuple<int, int, int>> results;
    std::mutex results_mutex;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int sys = t; sys < num_systems; sys += NUM_THREADS) {
                for (int s = 0; s < num_samples; ++s) {
                    const int16_t* candidate = get_sample(data, sys, s);
                    for (const auto* pattern : patterns) {
                        int score = simd_sad(pattern, candidate);
                        if (score < THRESHOLD) {
                            std::lock_guard<std::mutex> lock(results_mutex);
                            results.emplace_back(sys, s, score);
                        }
                    }
                }
            }
        });
    }

    for (auto& th : threads) th.join();
    return results;
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

int main() {
    std::vector<int16_t> data(NUM_SYSTEMS * NUM_SAMPLES * SAMPLE_SIZE);
    for (auto& val : data) val = rand() % 100;

    std::vector<std::vector<int16_t>> pattern_data;
    receive_json_patterns(pattern_data); // Load from JSON socket

    std::vector<const int16_t*> pattern_ptrs;
    for (const auto& p : pattern_data)
        pattern_ptrs.push_back(p.data());

    auto start = std::chrono::high_resolution_clock::now();
    auto matches = find_pattern_matches(pattern_ptrs, data, NUM_SYSTEMS, NUM_SAMPLES);
    auto end = std::chrono::high_resolution_clock::now();

    json result_json = json::array();
    for (const auto& [sys, t, score] : matches) {
        result_json.push_back({{"system", sys}, {"time", t}, {"score", score}});
    }

    std::cout << result_json.dump(2) << std::endl;
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed: " << elapsed.count() << " seconds\n";
    return 0;
}
