#include <iostream>
#include <unordered_map>
#include <chrono>
#include <random>

// Function to populate the maps with random values
void populateMaps(std::unordered_map<std::string, int>& str_map, std::unordered_map<int, int>& int_map, int count) {
    for (int i = 0; i < count; ++i) {
        str_map["key" + std::to_string(i)] = i;
        int_map[i] = i;
    }
}

// Function to measure lookup time for string keys
int measureStringLookup(const std::unordered_map<std::string, int>& str_map, int count) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        std::string key = "key" + std::to_string(i);
        if (str_map.find(key) == str_map.end()) {
            std::cerr << "String key not found: " << key << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    int value = (int)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "      String key lookup time: "
              << value
              << " microseconds" << std::endl;
    return value;
    
}

// Function to measure lookup time for integer keys
int measureIntLookup(const std::unordered_map<int, int>& int_map, int count) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        if (int_map.find(i) == int_map.end()) {
            std::cerr << "Integer key not found: " << i << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    int value = (int)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
 
    std::cout << "      Integer key lookup time: "
              << value 
              << " microseconds" << std::endl;
    return value;
}

int main() {
    const int test_size = 10000;
    const int map_size = test_size*1000;

    std::unordered_map<std::string, int> str_map;
    std::unordered_map<int, int> int_map;

    auto start = std::chrono::high_resolution_clock::now();
    populateMaps(str_map, int_map, map_size);
    auto end = std::chrono::high_resolution_clock::now();
    int value = (int)std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double fval = value / 1000000.0;
    std::cout << "Populate Maps time for  map size : " << map_size << " is " 
              << fval
              << " microseconds" << std::endl;

    for (int cycle = 0 ; cycle < 20 ; cycle++)
    {
        std::cout << "Testing lookup "<< cycle << " performance for " << test_size << " keys...\n";

        int val1  = measureStringLookup(str_map, test_size);
        int val2 = measureIntLookup(int_map, test_size);

        std::cout << "            Testing lookup performance for " << val1 * 100/val2 << " percent...\n";
    }
    return 0;
}