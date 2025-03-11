#include <iostream>
#include <vector>
#include <thread>
#include <cstdint>
#include <cstring>  // For memset
#include <atomic>
#include "ref_time.h"

#if defined(__aarch64__)  // ARM (i.MX93)
    #define PLATFORM_ARM
    #include <arm_neon.h>
#elif defined(__x86_64__)  // x86 (Intel, AMD)
    #define PLATFORM_X86
    #include <immintrin.h>  // SSE/AVX
#else
    #error "Unsupported architecture"
#endif

// Automatically detect the number of available CPU cores
const size_t NUM_THREADS = std::thread::hardware_concurrency();


#include <vector>
#include <iostream>
#include <cstdint>

struct Diff {
    uint16_t index;
    uint16_t value;
};

#include <vector>
#include <arm_neon.h>
#include <iostream>
#include <cstdint>

struct Diff {
    uint16_t index;
    uint16_t value;
};

#if defined(PLATFORM_ARM)

void create_control_data_vectors(const std::vector<Diff>& diffs, std::vector<uint16_t>& control, std::vector<uint16_t>& data) {
    if (diffs.empty()) return;

    size_t last_offset = 0;
    size_t batch_start = 0;
    size_t size = diffs.size();

    for (size_t i = 0; i + 8 <= size; i += 8) {
        // Load 8 indices into NEON register
        uint16x8_t indices = vld1q_u16(reinterpret_cast<const uint16_t*>(&diffs[i].index));
        uint16x8_t next_indices = vld1q_u16(reinterpret_cast<const uint16_t*>(&diffs[i + 1].index));

        // Compute difference between consecutive indices
        uint16x8_t expected_next = vaddq_u16(indices, vdupq_n_u16(1));
        uint16x8_t seq_mask = vceqq_u16(next_indices, expected_next);

        // Extract sequentiality mask
        uint16_t mask[8];
        vst1q_u16(mask, seq_mask);

        for (size_t j = 0; j < 8; j++) {
            if (mask[j] == 0 || i + j == size - 1) {  // Non-sequential or last element
                size_t offset = diffs[batch_start].index - last_offset;
                size_t count = (i + j + 1) - batch_start;

                control.push_back(static_cast<uint16_t>(offset));
                control.push_back(static_cast<uint16_t>(count));

                for (size_t k = batch_start; k <= i + j; k++) {
                    data.push_back(diffs[k].value);
                }

                last_offset = diffs[i + j].index;
                batch_start = i + j + 1;
            }
        }
    }

    // Process remaining elements
    for (size_t i = batch_start; i < size; i++) {
        control.push_back(static_cast<uint16_t>(diffs[i].index - last_offset));
        control.push_back(1);
        data.push_back(diffs[i].value);
        last_offset = diffs[i].index;
    }
}
#endif

#if defined(PLATFORM_X86)
void create_control_data_vectors(const std::vector<Diff>& diffs, std::vector<uint16_t>& control, std::vector<uint16_t>& data) {
    if (diffs.empty()) return;

    size_t last_offset = 0;
    size_t batch_start = 0;

    for (size_t i = 1; i <= diffs.size(); ++i) {
        // If not sequential or last element, finalize the batch
        if (i == diffs.size() || diffs[i].index != diffs[i - 1].index + 1) {
            size_t offset = diffs[batch_start].index - last_offset;
            size_t count = i - batch_start;

            control.push_back(static_cast<uint16_t>(offset));
            control.push_back(static_cast<uint16_t>(count));

            for (size_t j = batch_start; j < i; ++j) {
                data.push_back(diffs[j].value);
            }

            last_offset = diffs[i - 1].index;
            batch_start = i;
        }
    }
}
#elif defined(PLATFORM_X86)

// Compare a segment of the array in parallel
void compare_arrays_part(const uint16_t* arr1, const uint16_t* arr2, size_t start, size_t end, std::vector<Diff>& diffs) {
    #if defined(PLATFORM_ARM)
        for (size_t i = start; i < end; i += 8) {
            uint16x8_t vec1 = vld1q_u16(arr1 + i);
            uint16x8_t vec2 = vld1q_u16(arr2 + i);
            uint16x8_t cmp = vceqq_u16(vec1, vec2);
            uint16x8_t mask = vmvnq_u16(cmp);

            uint16_t mask_result[8];
            vst1q_u16(mask_result, mask);

            for (size_t j = 0; j < 8; ++j) {
                if (mask_result[j] != 0) {
                    diffs.push_back({static_cast<uint16_t>(i + j), arr2[i + j]});
                }
            }
        }

    #elif defined(PLATFORM_X86)
        for (size_t i = start; i < end; i += 8) {
            __m128i vec1 = _mm_loadu_si128((__m128i*)(arr1 + i));
            __m128i vec2 = _mm_loadu_si128((__m128i*)(arr2 + i));
            __m128i cmp = _mm_cmpeq_epi16(vec1, vec2);
            __m128i mask = _mm_xor_si128(cmp, _mm_set1_epi16(-1));  // Invert

            uint16_t mask_result[8];
            _mm_storeu_si128((__m128i*)mask_result, mask);

            for (size_t j = 0; j < 8; ++j) {
                if (mask_result[j] != 0) {
                    diffs.push_back({static_cast<uint16_t>(i + j), arr2[i + j]});
                }
            }
        }

    #else
        for (size_t i = start; i < end; i++) {
            if (arr1[i] != arr2[i]) {
                diffs.push_back({static_cast<uint16_t>(i), arr2[i]});
            }
        }
    #endif
}

// Multi-threaded function to compare two 64K arrays
std::vector<Diff> find_differences_multithreaded(const uint16_t* arr1, const uint16_t* arr2, size_t size) {
    std::vector<Diff> diffs;
    std::vector<std::thread> threads;
    std::vector<std::vector<Diff>> thread_results(NUM_THREADS);

    size_t chunk_size = size / NUM_THREADS;

    for (size_t t = 0; t < NUM_THREADS; t++) {
        size_t start = t * chunk_size;
        size_t end = (t == NUM_THREADS - 1) ? size : start + chunk_size;

        threads.emplace_back(compare_arrays_part, arr1, arr2, start, end, std::ref(thread_results[t]));
    }

    for (auto& th : threads) {
        th.join();
    }

    for (const auto& thread_diff : thread_results) {
        diffs.insert(diffs.end(), thread_diff.begin(), thread_diff.end());
    }

    return diffs;
}

#include <random>

// Function to generate random differences
void introduce_random_differences(std::vector<uint16_t>& arr1, std::vector<uint16_t>& arr2, size_t size, int num_diffs) {
    std::random_device rd;
    std::mt19937 gen(rd());  // Seeded random generator
    std::uniform_int_distribution<size_t> dist_index(0, size - 1);  // Random indices
    std::uniform_int_distribution<uint16_t> dist_value(1, 65535);   // Random values (excluding zero)

    for (int i = 0; i < num_diffs; ++i) {
        size_t index = dist_index(gen);
        arr2[index] = dist_value(gen);
    }
}

int main() {
    constexpr size_t SIZE = 65536;
    constexpr int  NUM_DIFFS = 4098;

    std::vector<uint16_t> arr1(SIZE, 0);
    std::vector<uint16_t> arr2(SIZE, 0);
    
    
    std::cout << "Using " << NUM_THREADS << " threads.\n";
    // Introduce random differences
    introduce_random_differences(arr1, arr2, SIZE, NUM_DIFFS);

    double start_time = ref_time_dbl();
    std::vector<Diff> diffs = find_differences_multithreaded(arr1.data(), arr2.data(), SIZE);
    double end_time = ref_time_dbl();

    std::cout << "Found " << diffs.size() << " differences in " 
              << (end_time - start_time) << " seconds.\n";
 

    // Create control & data vectors
    std::vector<uint16_t> control, data;
    create_control_data_vectors(diffs, control, data);
    std::cout << " control size: " << control.size()<< ", data size:" << data.size() << std::endl;

    // Print first few values for debugging
    std::cout << "Control Vector (offset, count): ";
    for (size_t i = 0; i < std::min(control.size(), size_t(10)); i += 2) {
        std::cout << "(" << control[i] << "," << control[i + 1] << ") ";
    }
    std::cout << "\nData Vector: ";
    for (size_t i = 0; i < std::min(data.size(), size_t(10)); i++) {
        std::cout << data[i] << " ";
    }
    std::cout << "\n";
    return 0;
}