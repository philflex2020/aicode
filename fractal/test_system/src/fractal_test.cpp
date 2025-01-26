/**
 ******************************************************************************
 * @Copyright (C), 2025, Fractal EMS Inc. 
 * @file: fractal_test.cpp
 * @author: Phil Wilshire
 * @Description:
 *     Fractal BMS Test System
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2025.01.23
 ******************************************************************************
 */
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <thread>
#include <future>
#include <array>
#include <memory>
#include <ctime>
#include <stdexcept>
#include <filesystem>

#include <algorithm>
#include <list>


#include <nlohmann/json.hpp>

#include <fractal_test.h>
#include <fractal_assert.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

//g++ -std=c++17 -o ftest -I ./inc src/fractal_test.cpp
// ./ftest tyest_plan1

bool debug = false;

// use data lists to create queries for Matches and Tests


//void test_data_list(ConfigDataList&configData)

// after reading in the data list ypu can use the following to create a query list and an index to the returned data items
//    load_data_map(fileName, systems, reverseMap);

// specify the list of data items
    // configData.emplace_back(find_name("rtos","min_soc"), 0);
    // configData.emplace_back(find_name("rtos","max_soc"), 0);
    // configData.emplace_back(find_name("rtos","min_soc_num"), 0);
    // configData.emplace_back(find_name("rtos","max_voltage_num"), 0);
    // configData.emplace_back(find_name("rtos","min_voltage"), 0);
    // configData.emplace_back(find_name("rtos","max_voltage"), 0);
    // configData.emplace_back(find_name("rtos","alarms"), 0);


//    produce the index of the data values returned  this can be used to understand the monitor data vector in the query  response
//    groupQueries(sorted, configData);

//    ShowConfigDataList(sorted);

//    produce a list of set / get queries
//    GenerateQueries("set", 2, queries, sorted);

//     // Display sorted config data
//     displaySortedConfigData(configData);

//     return 0;

// }

//TODO 
// Test code for match detection
// allow -ve tolerance == delta from last  coded but we need to convert uint16_t to int16
// mask_defs   x:y:z    offset:value:count -1 for all of them mask_defs, tolerance_defs, weight_defs  
//
// DONE
// test number json/test_defs.json
// set up test file as args  done
// default duration tim+1  done
// combine multiple gets into a single match vector done
// allow match tweaks done
// bug on no connect  remember we are using 9003
// system clean up 
// sequence file output
// Qarray on test sets


// // Structs
// struct InputVector {
//     int offset;
//     std::vector<uint16_t> data;
//     double timestamp;
// };

// struct MatchObject {
//     std::vector<uint16_t> vector;
//     std::vector<uint16_t> mask;    // Mask for each element
//     std::vector<uint16_t> tolerance;  // Tolerance for each element
//     std::vector<uint16_t> weight;     // Weight for each element
//     std::map<int, std::vector<int>> matches;
//     std::string name;
// };

// // Hash Function for Vectors
// struct VectorHash {
//     std::size_t operator()(const std::vector<uint16_t>& vec) const {
//         std::size_t hash = 0;
//         for (const auto& val : vec) {
//             hash ^= std::hash<uint16_t>()(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
//         }
//         return hash;
//     }
// };
// struct MatchObject {
//     std::vector<uint16_t> vector;
//     std::vector<uint16_t> mask;    // Mask for each element
//     std::vector<uint16_t> tolerance;  // Tolerance for each element
//     std::vector<uint16_t> weight;     // Weight for each element
//     std::map<int, std::vector<int>> matches;
//     std::string name;
// };

int str_to_offset(const std::string& offset_str) {
    int neg = 1;
    size_t start_idx = 0;

    // Check if the number is negative
    if (offset_str[0] == '-') {
        neg = -1;
        start_idx = 1; // Start checking after the minus sign
    }

    try {
        // Adjust start index for hexadecimal notation, which may follow a negative sign
        if (offset_str.size() > start_idx + 2 && (offset_str[start_idx] == '0') && 
            (offset_str[start_idx + 1] == 'x' || offset_str[start_idx + 1] == 'X')) {
            return neg * std::stoi(offset_str.substr(start_idx), nullptr, 16); // Parse as hexadecimal
        } else {
            return neg * std::stoi(offset_str.substr(start_idx)); // Parse as decimal
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid offset value: " << offset_str << std::endl;
        throw;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Offset value out of range: " << offset_str << std::endl;
        throw;
    }
}
// int str_to_offset(const std::string& offset_str) {
//     int neg = 1;
//     if(offset_str.size() > 2 && (offset_str[0] == '-'))
//     {
//         neg = -1;
//     }

//     try {
//         // Check if the string starts with "0x" or "0X" for hexadecimal
//         if (offset_str.size() > 2 && (offset_str[0] == '0') && (offset_str[1] == 'x' || offset_str[1] == 'X')) {
//             return std::stoi(offset_str, nullptr, 16); // Parse as hexadecimal
//         } else {
//             return std::stoi(offset_str); // Parse as decimal
//         }
//     } catch (const std::invalid_argument& e) {
//         std::cerr << "Error: Invalid offset value: " << offset_str << std::endl;
//         throw;
//     } catch (const std::out_of_range& e) {
//         std::cerr << "Error: Offset value out of range: " << offset_str << std::endl;
//         throw;
//     }
// }
    // myassert(v1 > 24, "object.vars.set_value_with_rack", crash);
    // myassert(v2 == 30, "object.vars.check_initial_value", crash);
    // myassert(v1 + v2 > 50, "object.vars.sum_check", crash);
    // myassert(v1 < v2, "object.vars.compare_values", crash);

    // // Generate reports
    // assert_manager.generate_summary();
    // assert_manager.generate_report();

void test_str_to_offset() {
    bool crash = false;
    // Test valid decimal input
    myassert(str_to_offset("123") == 123,"str_to_offset_integer", crash);

    // Test valid hexadecimal input
    myassert(str_to_offset("0x1A3") == 419,"str_to_offset_hex_1", crash);

    // Test another valid hexadecimal input
    myassert(str_to_offset("0X1a3") == 419,"str_to_offset_hex_2", crash);

    // Test valid hexadecimal with capital letters
    myassert(str_to_offset("0XABC") == 2748,"str_to_offset_hex_3", crash);

    // Test valid negative decimal
    myassert(str_to_offset("-10") == -10,"str_to_offset_neg", crash);

    // Test valid negative hexadecimal
    myassert(str_to_offset("-0x1A") == -26,"str_to_offset_neg_hex", crash);

    // Test edge case of minimal non-zero integers
    myassert(str_to_offset("1") == 1,"str_to_offset_one", crash);
    myassert(str_to_offset("0x1") == 1,"str_to_offset_hex_one", crash);

    // Test zero
    myassert(str_to_offset("0") == 0,"str_to_offset_zero", crash);
    myassert(str_to_offset("0x0") == 0,"str_to_offset_hex_zero", crash);

    // // Test invalid inputs
    // try {
    //     str_to_offset("hello");
    //     myassert(false && "str_to_offset should have thrown an exception for 'hello'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }

    // try {
    //     str_to_offset("0x1G");
    //     myassert(false && "str_to_offset should have thrown an exception for '0x1G'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }
}
// Function to trim whitespace from both ends of a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

// Function to Get Current Time in Seconds
double ref_time_dbl() {
    using namespace std::chrono;
    static auto ref_time = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<duration<double>>(now - ref_time).count();
}



// Global Variables
std::map<int, std::vector<InputVector>> inputVecs;
std::unordered_map<std::vector<uint16_t>, int, VectorHash> matchIndexMap;
//std::vector<MatchObject> matchObjects;
// TODO start using this 
std::vector<std::shared_ptr<MatchObject>> sharedMatchObjects;

// Track active Expects and NotExpects
std::map<std::string, int> active_expects;
std::map<std::string, int> active_not_expects;


// Using nested maps to store configurations
// typedef std::map<int, ConfigItem> OffsetMap;
// typedef std::map<std::string, ConfigItem> ConfigMap;
// typedef std::map<std::string, ConfigMap> SystemMap;
// typedef std::map<std::string, ConfigItem> TypeMap;
// typedef std::map<std::string, std::string> ReverseMap;

// Using shared pointers for ConfigItems
typedef std::map<int, std::shared_ptr<ConfigItem>> OffsetMap;
typedef std::map<std::string, std::shared_ptr<ConfigItem>> ConfigMap;
typedef std::map<std::string, ConfigMap> SystemMap;
typedef std::map<std::string, std::shared_ptr<ConfigItem>> TypeMap;
typedef std::map<std::string, std::string> ReverseMap;
typedef std::pair<std::shared_ptr<ConfigItem>, int> ConfigDataPair;
typedef std::vector<ConfigDataPair> ConfigDataList;



SystemMap systems;
ReverseMap reverseMap;
TypeMap typeMap;



// Parse the configuration file using shared pointers
void load_data_map(const std::string& filename, SystemMap& systems, ReverseMap& reverseMap) {
    std::ifstream file(filename);
    std::string line;
    std::string currentSystem, currentType;
    int currentOffset;

    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    std::cout << "Opened file: " << filename << std::endl;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue; // Skip empty lines and comments

        if (line[0] == '[') { // New system configuration
            std::cout << "Found group: " << line << std::endl;
            line.erase(0, 1); // Remove leading '['
            line.pop_back();  // Remove trailing ']'
            std::istringstream iss(line);
            std::string part;
            std::vector<std::string> parts;
            while (getline(iss, part, ':')) {
                parts.push_back(trim(part));
            }
            if (parts.size() >= 3) {
                currentSystem = parts[0];
                currentType = parts[1];
                currentOffset = std::stoi(parts[2]);
            }
        } else { // Data item within the system
            std::istringstream iss(line);
            std::string part, name;
            int offset, size = 1;
            std::vector<std::string> parts;
            while (getline(iss, part, ':')) {
                parts.push_back(trim(part));
            }
            if (parts.size() > 1) {
                name = parts[0];
                offset = std::stoi(parts[1]);
            }            
            if (parts.size() > 2) {
                size = std::stoi(parts[2]);
            }
            std::cout << "Found name: " << name << std::endl;

            // Store configuration using shared_ptr
            auto item = std::make_shared<ConfigItem>(ConfigItem{name, currentSystem, currentType, currentOffset + offset, size});
            systems[currentSystem][name] = item;

            // Update typeMap and reverseMap
            std::ostringstream tname, osname, oss;
            tname << currentSystem << ":" << currentType << ":" << (currentOffset + offset);
            typeMap[tname.str()] = item;
            osname << currentSystem << ":" << name;
            oss << currentType << "|" << (currentOffset + offset) << (size > 1 ? ":" + std::to_string(size) : "");
            reverseMap[osname.str()] = oss.str();
        }
    }
}

// // Function to generate query strings from the configuration map
// void generate_query_strings(const SystemMap& systems) {
//     for (const auto& systemPair : systems) {
//         const auto& systemName = systemPair.first;
//         const auto& configs = systemPair.second;
//         for (const auto& configPair : configs) {
//             const auto& configItem = configPair.second;
//             std::ostringstream queryString;

//             queryString << "{";
//             queryString << "\"action\":\"get\", ";
//             queryString << "\"sm_name\":\"" << configItem->system << "\", ";
//             queryString << "\"reg_type\":\"" << configItem->type << "\", ";
//             queryString << "\"offset\":\"" << configItem->offset << "\", ";
//             queryString << "\"num\":" << configItem->size << ", ";
//             queryString << "\"data\":[0], "; // Placeholder for data
//             queryString << "\"note\":\"" << configItem->name << "\"";
//             queryString << "}";

//             std::cout << queryString.str() << std::endl;
//         }
//     }
// }
// int main() {
//     // Example of setting up the map
//     auto item = std::make_shared<ConfigItem>(ConfigItem{"online", "rack", "sm16", 26626, 1});
//     systems["rack"]["online"] = item;

//     // Now generate query strings
//     generate_query_strings(systems);

//     return 0;
// }
// Define a type for the pair of ConfigItem and corresponding data
// typedef std::pair<std::shared_ptr<ConfigItem>, int> ConfigDataPair;
// typedef std::vector<ConfigDataPair> ConfigDataList;

// // Custom comparison function for sorting
bool compareConfigDataPairs(const ConfigDataPair& a, const ConfigDataPair& b) {
    // Primary sort by system
    if (a.first->system != b.first->system)
        return a.first->system < b.first->system;
    // Secondary sort by type
    if (a.first->type != b.first->type)
        return a.first->type < b.first->type;
    // Tertiary sort by offset
    return a.first->offset < b.first->offset;
}

// Function to generate and display sorted config data pairs
void displaySortedConfigData(const ConfigDataList& data) {
    // Create a copy of the data to sort
    ConfigDataList sortedData = data;
    std::sort(sortedData.begin(), sortedData.end(), compareConfigDataPairs);

    // Display sorted data
    for (const auto& item : sortedData) {
        std::cout << "System: " << item.first->system
                  << ", Type: " << item.first->type
                  << ", Offset: " << item.first->offset
                  << ", Data: " << item.second << std::endl;
    }
}




// Generate query string for a group of ConfigDataPairs
std::string generateQuery(const std::string& action, int seq, const ConfigDataList& group, const std::string& system, const std::string& type, int startOffset) {
    std::ostringstream query;
    query << "{\"action\":\""<< action <<"\", \"sec\":" <<seq<< "\", \"sm_name\":\"" << system
          << "\", \"reg_type\":\"" << type
          << "\", \"offset\":\"" << startOffset
          << "\", \"num\":" << group.size()
          << ", \"data\":[";
    for (size_t i = 0; i < group.size(); ++i) {
        if (i > 0) query << ", ";
        query << group[i].second;
    }
    query << "]}";
    return query.str();
}
// Function to take the adhoc data list and sort it, this will be the index of the query data
void groupQueries(ConfigDataList& sorted, const ConfigDataList& data) {
    if (data.empty()) return;
    sorted = data;
    std::sort(sorted.begin(), sorted.end(), compareConfigDataPairs);
}

// Function to group and generate queries
//void groupQueries(ConfigDataList& sorted, const ConfigDataList& data) {
void GenerateQueries(const std::string& action, int seq, std::vector<std::string> &queries, const ConfigDataList& data) {
    if (data.empty()) return;

    ConfigDataList sortedData;
    groupQueries(sortedData, data);

    ConfigDataList currentGroup;
    std::string currentSystem = sortedData[0].first->system;
    std::string currentType = sortedData[0].first->type;
    int startOffset = sortedData[0].first->offset;

    for (const auto& item : sortedData) {
        if (item.first->system == currentSystem && item.first->type == currentType &&
            (item.first->offset == startOffset + currentGroup.size())) {
            currentGroup.push_back(item);
        } else {
            if (!currentGroup.empty()) {
                std::string query = generateQuery(action, seq, currentGroup, currentSystem, currentType, startOffset);
                std::cout << query << std::endl;
                queries.emplace_back(query);
            }
            currentGroup.clear();
            currentGroup.push_back(item);
            currentSystem = item.first->system;
            currentType = item.first->type;
            startOffset = item.first->offset;
        }
    }

    // Handle the last group
    if (!currentGroup.empty()) {
        std::string query = generateQuery(action, seq, currentGroup, currentSystem, currentType, startOffset);
        std::cout << query << std::endl;
        queries.emplace_back(query);
    }
}
// Function to group and generate queries
//void groupQueries(ConfigDataList& sorted, const ConfigDataList& data) {
void ShowConfigDataList(const ConfigDataList& data) {
    if (data.empty()) return;
    //std::string currentSystem = sortedData[0].first->system;
    //std::string currentType = sortedData[0].first->type;
    //int startOffset = sortedData[0].first->offset;
    ConfigDataList currentGroup;

    for (const auto& item : data) {
        std::string currentSystem = item.first->system;
        std::string currentType =item.first->type;
        int offset = item.first->offset;
        currentGroup.push_back(item);
        std::string query = generateQuery("get", 22, currentGroup, currentSystem, currentType, offset);
        currentGroup.clear();
        
        std::cout << query << std::endl;
    }
}



//doc/test_sbmu_mapping.txt

int test_data_map(std::string& fileName) {

    load_data_map(fileName, systems, reverseMap);
    return 0;
}

std::shared_ptr<ConfigItem>find_type(std::string sm_name, std::string reg_type, int offset)
{
    std::ostringstream tname;
    tname << sm_name << ":" << reg_type <<":" << offset;
    if(typeMap.find(tname.str()) != typeMap.end())
    {
        return typeMap[tname.str()];
    }
    return nullptr;
}

std::shared_ptr<ConfigItem>find_name(const std::string& sm_name, const std::string& name)
{
    std::ostringstream tname;
    tname << sm_name << ":" << name;
    if(systems.find(sm_name) != systems.end())
    {
        auto slist = systems[sm_name];
        if(slist.find(name) != slist.end())
        {
            return slist[name];
        }
    }
    return nullptr;
}

void test_data_list(ConfigDataList&configData)
{
    configData.emplace_back(find_name("rtos","min_soc"), 0);
    configData.emplace_back(find_name("rtos","max_soc"), 0);
    configData.emplace_back(find_name("rtos","min_soc_num"), 0);
    configData.emplace_back(find_name("rtos","max_voltage_num"), 0);
    configData.emplace_back(find_name("rtos","min_voltage"), 0);
    configData.emplace_back(find_name("rtos","max_voltage"), 0);
    configData.emplace_back(find_name("rtos","alarms"), 0);
    ConfigDataList sorted;
    std::vector<std::string>queries;

    std::cout <<" Unsorted list :" << std::endl;
    ShowConfigDataList(configData);


    std::cout <<" Sorted list :" << std::endl;
    groupQueries(sorted, configData);
    ShowConfigDataList(sorted);

    std::cout <<" Queries :" << std::endl;
    GenerateQueries("set", 2, queries, sorted);

    }

// Comparator for sorting
bool configItemComparator(const std::shared_ptr<ConfigItem>& a, const std::shared_ptr<ConfigItem>& b) {
    if (a->system != b->system)
        return a->system < b->system;
    if (a->type != b->type)
        return a->type < b->type;
    return a->offset < b->offset;
}

int test_item_sort() {
    // Example data
    std::vector<std::shared_ptr<ConfigItem>> items;
     //ConfigItem item{name, currentSystem, currentType, currentOffset + offset, size};
    items.push_back(std::make_shared<ConfigItem>("Item1", "SystemA", "Type1", 100, 1));
    items.push_back(std::make_shared<ConfigItem>("Item2", "SystemA", "Type2", 95, 1));
    items.push_back(std::make_shared<ConfigItem>("Item3", "SystemA", "Type1", 90, 1));
    items.push_back(std::make_shared<ConfigItem>("Item4", "SystemB", "Type1", 110, 1));
    items.push_back(std::make_shared<ConfigItem>("Item5", "SystemA", "Type1", 85, 1));

    // Sorting
    std::sort(items.begin(), items.end(), configItemComparator);

    // Output sorted items
    for (const auto& item : items) {
        std::cout << "System: " << item->system << ", Type: " << item->type
                  << ", Offset: " << item->offset << ", Name: " << item->name << std::endl;
    }

    return 0;
}

void splitIntoGroups(const std::vector<std::shared_ptr<ConfigItem>>& sortedItems) {
    std::list<std::vector<std::shared_ptr<ConfigItem>>> groups;
    std::vector<std::shared_ptr<ConfigItem>> currentGroup;

    if (!sortedItems.empty()) {
        auto currentOffset = sortedItems[0]->offset;
        for (const auto& item : sortedItems) {
            // Check if current item continues the sequence
            if (item->offset == currentOffset) {
                currentGroup.push_back(item);
            } else {
                // Start a new group
                groups.push_back(currentGroup);
                currentGroup.clear();
                currentGroup.push_back(item);
            }
            currentOffset = item->offset + item->size;  // update currentOffset to expect the next sequence
        }

        // Add the last group
        if (!currentGroup.empty()) {
            groups.push_back(currentGroup);
        }
    }

    // Output groups for demonstration
    int groupNumber = 1;
    for (const auto& group : groups) {
        std::cout << "Group " << groupNumber++ << ":" << std::endl;
        for (const auto& item : group) {
            std::cout << "  System: " << item->system << ", Type: " << item->type
                      << ", Offset: " << item->offset << ", Name: " << item->name << std::endl;
        }
    }
}


// show the whole thing
int show_data_map() {

    for (const auto& system : systems) {
        std::cout << "System: " << system.first << std::endl;
        for (const auto& item : system.second) {
            std::cout << "  Item: " << item.first << " Type: " << item.second->type
                      << " Offset: " << item.second->offset << " Size: " << item.second->size << std::endl;
        }
    }

    // Display reverse map 
    std::cout << "\nReverse Map:" << std::endl;
    for (const auto& entry : reverseMap) {
        std::cout << "Name: " << entry.first << " Path: " << entry.second << std::endl;
    }
    // Display offset map
    std::cout << "\nType Map:" << std::endl;
    for (const auto& entry : typeMap) {
            std::cout << "Type: " << entry.first <<  " Name: " << entry.second->name << std::endl;
    }
    //ConfigItem*
    auto item = find_type("rtos", "input", 3);
    if (item)
    {
        std::cout << " Found name: " << item->name << std::endl;
    }

    return 0;
}


// Load the metadata from a JSON file
json load_metadata(const std::string& filepath) {
    json metadata;
    std::ifstream file(filepath);
    if (file.is_open()) {
        file >> metadata;
    } else {
        std::cerr << "Failed to open metadata file.\n";
    }
    file.close();
    return metadata;
}

// Save the updated metadata back to the JSON file
void save_metadata(const std::string& filepath, const json& metadata) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << metadata.dump(4);  // Pretty printing with an indent of 4 spaces
    } else {
        std::cerr << "Failed to save metadata file.\n";
    }
    file.close();
}

// Handle the execution and logging of a test plan
int get_test_run(const std::string& test_name )
{
    const std::string metadata_path = "./json/test_meta.json";

    json metadata = load_metadata(metadata_path);
    if (!metadata.contains(test_name)) {
        metadata[test_name] = {{"runs", 0}, {"results", json::array()}};
    }

    // Increment the run count
    metadata[test_name]["runs"] = metadata[test_name]["runs"].get<int>() + 1;
    int current_run = metadata[test_name]["runs"];

    // Save updated metadata
    save_metadata(metadata_path, metadata);
    return current_run;
}


// Handle the execution and logging of a test plan
void handle_test_run(const std::string& test_name, const std::string& metadata_path) {
    json metadata = load_metadata(metadata_path);
    if (!metadata.contains(test_name)) {
        metadata[test_name] = {{"runs", 0}, {"results", json::array()}};
    }

    // Increment the run count
    metadata[test_name]["runs"] = metadata[test_name]["runs"].get<int>() + 1;
    int current_run = metadata[test_name]["runs"];

    // Simulate a test result
    bool test_passed = true;  // Simulate test pass condition

    // Record the result
    json result = {{"run", current_run}, {"passed", test_passed}};
    metadata[test_name]["results"].push_back(result);

    // Save updated metadata
    save_metadata(metadata_path, metadata);

    // Create result directory
    std::string result_dir = "results/" + test_name + "/" + std::to_string(current_run);
    fs::create_directories(result_dir);

    // Save individual test result
    std::ofstream result_file(result_dir + "/result.json");
    if (result_file.is_open()) {
        result_file << result.dump(4);
    } else {
        std::cerr << "Failed to save result file.\n";
    }
    result_file.close();
}

// Add Input Vector
void add_input_vector(int run, int offset, const std::vector<uint16_t>& data) {
    double timestamp = ref_time_dbl();
    inputVecs[run].push_back({offset, data, timestamp});
}


// Function to calculate match score
double calculate_match_score(const std::vector<uint16_t>& input,
                             std::shared_ptr<MatchObject> match) {
    if(debug)std::cout << "calculate match and score" << std::endl;
    double total_weight = 0.0;
    double match_score = 0.0;

    // Ensure vectors are of the same size
    if (input.size() != match->vector.size()) {
        return 0.0; // Not a valid match
    }
    if(debug)std::cout << "calculate match and score 1" << std::endl;

    for (size_t i = 0; i < input.size(); ++i) {
        if(debug)std::cout << "calculate match and score item "<< i << " size " << input.size() << std::endl;
        // Apply mask if it exists
        uint16_t masked_input = input[i];
        uint16_t masked_match = match->vector[i];
        if(debug)std::cout << "calculate match and score item "<< i << " input " << masked_input << " match " << masked_match << std::endl;

        if (!match->mask.empty() && match->mask.size() == match->vector.size()) {
            // special case mymask 0 = 0xffff allow all bits
            uint16_t mymask = match->mask[i];
            if (mymask == 0)
                mymask=  ~match->mask[i];

            masked_input &= mymask;
            masked_match &= mymask;

        }
        if(debug)std::cout << "calculate match and score item after mask  "<< i << " size " << input.size() << std::endl;

        // Calculate absolute difference
        uint16_t difference = std::abs(static_cast<int>(masked_input) - static_cast<int>(masked_match));
        if(debug) std::cout << "calculate match and score item after bitmap  "<< i << " doff " << difference << std::endl;

        // Check if within tolerance if tolerance vector exists
        bool within_tolerance = false;
        if (difference == 0)
        {
            within_tolerance = true;
        }

        if (!match->tolerance.empty() && match->tolerance.size() == match->vector.size()) {

            uint16_t tolerance = match->tolerance[i];
            if (match->tolerance[i] > 0x8000) {
                tolerance = match->tolerance[i] - 0x8000;
            }

            within_tolerance = (difference <= tolerance);
            if (within_tolerance) {
                // delta tolerance // update match
                if (match->tolerance[i] > 0x8000) {
                    match->vector[i] = input[i];
                }
            }

        }

        if (within_tolerance) {
            // Add weighted score or default to 1 if weights are not defined
            double weight = (!match->weight.empty() && match->weight.size() == match->vector.size())
                                ? static_cast<double>(match->weight[i])
                                : 1.0;
            match_score += weight;
            total_weight += weight;
        }
        else
        {
            return 0;
        }
    }
    if(debug) std::cout << "calculate match and score done" << std::endl;

    // Normalize match score by total weight
    return (total_weight > 0.0) ? (match_score / total_weight) : 0.0;
}
// we may need to find all matches
void find_all_matches(std::vector<std::string> & matches,int run, const std::vector<uint16_t>& input,
                                   double threshold) {
    int idx = 0;
    if(debug)
    {
        std::cout << " Looking for best match; run " << run<< std::endl;
        std::cout << "      matchObjects size "<< sharedMatchObjects.size()<< std::endl;
    }

    for (auto match : sharedMatchObjects) {
        if(debug)
            std::cout << "  checking  all matches   " << match->name << std::endl;
        double score = calculate_match_score(input, match);
        if(debug)
            std::cout << " Calculated match score   " << score << std::endl;

        if (score >= threshold) {
            std::cout << "  added  best match   " << match->name << std::endl;
            matches.push_back (match->name);
        }
    }

    return;
}

// Function to find the best match
const std::shared_ptr<MatchObject> find_best_match(int run, const std::vector<uint16_t>& input,
                                   double threshold) {
    std::shared_ptr<MatchObject> best_match;
    double best_score = 0.0;
    int idx = 0;
    if(debug)
    {
        std::cout << " Looking for best match; run " << run<< std::endl;
        std::cout << "      matchObjects size "<< sharedMatchObjects.size()<< std::endl;
    }

    for (auto match : sharedMatchObjects) {
        if(debug)
            std::cout << "  checking  best match   " << match->name << std::endl;
        double score = calculate_match_score(input, match);
        if(debug)
            std::cout << " Calculated match score   " << score << std::endl;

        if (score > best_score && score >= threshold) {
            std::cout << "  set best match   " << match->name << std::endl;
            best_score = score;
            best_match = match;
        }
    }

    return best_match;
}

// Create a new MatchObject wrapped in a std::shared_ptr
std::shared_ptr<MatchObject> create_new_match(int run, const std::vector<uint16_t>& data) {
    std::shared_ptr<MatchObject> new_match = std::make_shared<MatchObject>();
    new_match->vector = data;
    new_match->name = "Match_ID " + std::to_string(sharedMatchObjects.size() + 1);  // Assign ID based on the next index

    // Add the new match to the vector
    sharedMatchObjects.push_back(new_match);

    if (debug) {
        std::cout << "Created a new match object: " << new_match->name << std::endl;
    }

    return new_match;
}

  
// Test Matches for a Run
void test_json_matches(json& matches, int run) {
    if(debug)std::cout << " test json matches run " << run << ".\n";

    if (inputVecs.find(run) == inputVecs.end()) {
        std::cerr << "No input vectors for run " << run << ".\n";
        return;
    }
    // 
    json match_json;
    match_json["match_idx"] = json::array();

    for (size_t match_id = 0; match_id < sharedMatchObjects.size(); ++match_id) {
        const auto match = sharedMatchObjects.at(match_id);

        if (match->matches.count(run)) {

            std::vector<std::string>match_str_vec;
            for (auto idx : match->matches.at(run)) {
                //std::cout << idx << " ";
                auto name = sharedMatchObjects.at(idx)->name;
                //std::cout << name  << " \n";

                match_str_vec.push_back(name);
            }
            match_json["match_idx"]= match_str_vec;
        }
        matches.push_back(match_json);
    }
}

// Check Match Consistency
void check_match_consistency(int base_run = 0) {
    return;    
}

// Run WebSocket Command
std::string run_wscat(const std::string& url, const std::string& query_str) {
    std::string command = "wscat -c " + url + " -x '" + query_str + "' -w 0";
    std::array<char, 1024> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + command);
        log_msg << " Unable to process message "<< std::endl; 
        result = "Failed";
    }
    else
    {
        while (fgets(buffer.data(), buffer.size(), pipe.get())) {
            result += buffer.data();
        }
        log_msg << " wscat response ["<< result <<"]"<< std::endl; 

    }
    return result;
}


void load_test_plan(json& testPlan, const std::string& testName) 
{ 
    std::ostringstream filePath;
    filePath << "json/"<< testName<<".json";


    std::ifstream inputFile(filePath.str());
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << filePath.str() << std::endl;
        return;
    }
    try {
        inputFile >> testPlan; // Parse the JSON file
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return;
    }
    std::cout << "TestPlan opened from file: " << filePath.str() << std::endl;
}


void save_matches(const std::string& matchName) {

    std::ostringstream filePath;
    filePath << "json/matches_"<< matchName<<".json";

    // Load existing matches if the file exists
    std::ifstream inputFile(filePath.str());
    json existingMatches = json::array();
    if (inputFile.is_open()) {
        try {
            inputFile >> existingMatches; // Parse existing JSON data
        } catch (const std::exception& e) {
            std::cerr << "Error reading existing matches: " << e.what() << "\n";
        }
        inputFile.close();
    }

    // Create a set of existing match names to avoid duplicates
    std::unordered_set<std::string> existingNames;

    for (const auto& match : existingMatches) {
        if (match.contains("name")) {
            existingNames.insert(match["name"].get<std::string>());
        }
    }

    // Add new matches
    for (const auto& match : sharedMatchObjects) {
        if (existingNames.find(match->name) == existingNames.end()) {
            json newMatch;
            newMatch["name"] = match->name;
            newMatch["vector"] = match->vector;
            if(!match->tolerance.empty())
                newMatch["tolerance"] = match->tolerance;
            if(!match->weight.empty())
                newMatch["weight"] = match->weight;
            if(!match->mask.empty())
                newMatch["mask"] = match->mask;
            //newMatch["matches"] = match.matches;
            existingMatches.push_back(newMatch);
        }
    }

    //std::string outfile = filename;
    // Save the combined matches back to the file
    std::ofstream outputFile(filePath.str());
    if (!outputFile.is_open()) {
        std::cerr << "Error: Unable to open file [" << filePath.str() <<"] for saving matches.\n";
        return;
    }

    outputFile << "[\n"; 

    bool first = true;
    for ( auto &m : existingMatches )
    {
        if ( first)
        {
            first = false;
            outputFile << "\n";
        }
        else 
        {
            outputFile << ",\n";
        }
        outputFile << "{ \"name\": "<< m["name"].dump(); // Save with pretty-print
            outputFile <<",\n            \"vector\":"<< m["vector"].dump(); // Save with pretty-print
        if(m.contains("tolerance"))
        {
            outputFile <<",\n            \"tolerance\":"<< m["tolerance"].dump(); // Save with pretty-print
        }
        if(m.contains("weight"))
        {
            outputFile <<",\n            \"weight\":"<< m["weight"].dump(); // Save with pretty-print
        }
        if(m.contains("mask"))
        {
            outputFile <<",\n            \"mask\":  "<< m["mask"].dump(); // Save with pretty-print
        }
                if(m.contains("tolerance"))
        {
            outputFile <<",\n            \"tolerance\":"<< m["tolerance"].dump(); // Save with pretty-print
        }
        // these are all consumed on load , you never see them
        // if(m.contains("mask_defs"))
        // {
        //     outputFile <<",\n            \"mask_defs\":  "<< m["mask_defs"].dump(); // Save with pretty-print
        // }
        // if(m.contains("weight_defs"))
        // {
        //     outputFile <<",\n            \"weight_defs\":"<< m["weight_defs"].dump(); // Save with pretty-print
        // }
        // if(m.contains("tolerance_defs"))
        // {
        //     outputFile <<",\n            \"tolerance_defs\":  "<< m["tolerance_defs"].dump(); // Save with pretty-print
        // }

        outputFile << "}"; 
    }
    outputFile << "\n]\n"; 

    outputFile.close();

    std::cout << "Matches saved to " << filePath.str() << "\n";
}

// Function to parse tolerance definitions and apply them to a MatchObject
void applyMatchDefs(std::shared_ptr<MatchObject>match, std::vector<uint16_t>&matchItem, const std::vector<std::string>& defs) {
    for (const auto& def : defs) {
        std::istringstream ss(def);
        std::string token;
        int offset = 0;
        uint16_t value = 0;
        int count = 1;

        // Get the offset part
        std::getline(ss, token, ':');
        offset = std::stoi(token);

        // Get the value part
        if (std::getline(ss, token, ':')) {
            value = static_cast<uint16_t>(std::stoi(token));
        }
        auto vec_size = match->vector.size();

        // Optional: Get the count part
        if (std::getline(ss, token, ':')) {
            count = std::stoi(token);
        }
        matchItem.resize(vec_size);
        // Apply tolerance values starting from offset up to the specified count or the end of the vector
        for (int i = offset; i < offset + count && i < vec_size; ++i) {
            matchItem[i] = value;
        }
    }
}

void load_matches(const std::string& matchName) {

    std::ostringstream filePath;
    filePath << "json/matches_"<< matchName<<".json";


    std::ifstream inputFile(filePath.str());
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << filePath.str() << std::endl;
        return;
    }

    json matchData;
    try {
        inputFile >> matchData;  // Parse the JSON file
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parsing error at byte " << e.byte << ": " << e.what() << std::endl;
        inputFile.close();
        return;
    }

    inputFile.close();

    for (const auto& match : matchData) {
        std::string name = match.value("name", "Unnamed");
        auto data = match.value("vector", std::vector<uint16_t>{});
        std::cout << "Loaded match '" << name << "' with data vector of size: " << data.size() << std::endl;
        
        auto new_match = create_new_match(-1, data);
        if(match.contains("tolerance"))
        {
            new_match->tolerance = match.value("tolerance", std::vector<uint16_t>{});
        }
        if(match.contains("weight"))
        {
            new_match->weight = match.value("weight", std::vector<uint16_t>{});
        }
        if(match.contains("mask"))
        {
            new_match->mask = match.value("mask", std::vector<uint16_t>{});
        }
        if(match.contains("tolerance_defs"))
        {
            auto defs = match.value("tolerance_defs", std::vector<std::string>{});
            applyMatchDefs(new_match, new_match->tolerance, defs);

            // decode eack the defs  string
            // "offset:value[:count]""
            // first check if the tolerance has any values 
            // then set (or overwrite) the value at the offset (plus count up to the size of the match->input vector) 


        }
        if(match.contains("weight_defs"))
        {
            //new_match->weight_defs = match.value("weight_defs", std::vector<std::string>{});
            auto defs = match.value("tolerance_defs", std::vector<std::string>{});
            applyMatchDefs(new_match, new_match->weight, defs);
   
        }
        if(match.contains("mask_defs"))
        {
            //new_match->mask_defs = match.value("mask_defs", std::vector<std::string>{});
            auto defs = match.value("tolerance_defs", std::vector<std::string>{});
            applyMatchDefs(new_match, new_match->mask, defs);
        }

        new_match->name = name;
    }
}




// the data to a file in the data dir as a json object 
// also save in a base_dir
void save_run_data(std::string&target, int test_run, int run , int seq, std::vector<uint16_t>data, json& matches)
{

    std::string data_dir =  "data/" + target + "/"+ std::to_string(test_run)+"/";
    fs::create_directories(data_dir);

    std::string base_dir =  "data/" + target + "/";
    fs::create_directories(base_dir);

    // Build the file name
    std::ostringstream file_path;
    file_path << data_dir << "data_file.json";
    std::cout << " save data to " <<file_path.str() << " run " << run << std::endl;

    std::ostringstream base_path;
    base_path << base_dir << "data_file.json";

    // Prepare the JSON object to be appended
    json json_data = {
        {"run", run},
        {"data", data},
        {"matches", matches}
    };
//            {"matches", matches}
//       {"seq", seq},
 
    if(run >= 0)
    {
    if (run == 0) {
        if (std::filesystem::exists(file_path.str())) {
            std::filesystem::remove(file_path.str());
        }
        if (std::filesystem::exists(base_path.str())) {
            std::filesystem::remove(base_path.str());
        }
        std::ofstream out_file(file_path.str(), std::ios::trunc);
        if (!out_file) {
            std::cout << "Failed to create new test file: " << file_path.str() << "\n";
            return;
        }
        std::ofstream base_file(base_path.str(), std::ios::trunc);
        if (!base_file) {
            std::cout << "Failed to create new base file: " << base_path.str() << "\n";
            return;
        }
        out_file << "[\n"; // Start the JSON array
        out_file.close();
        base_file << "[\n"; // Start the JSON array
        base_file.close();
    }

    std::ofstream out_file(file_path.str(), std::ios::app);
    if (!out_file) {
        std::cerr << "Failed to open file: " << file_path.str() << "\n";
        return;
    }
    std::ofstream base_file(base_path.str(), std::ios::app);
    if (!out_file) {
        std::cerr << "Failed to open file: " << base_path.str() << "\n";
        return;
    }
    if (run > 0)
    {
        out_file << ",\n";
        base_file << ",\n";
    }

    out_file << json_data.dump();// << "\n"; // Write the JSON object as a single line
    out_file.close();
    base_file << json_data.dump();// << "\n"; // Write the JSON object as a single line
    base_file.close();

    // For run 0, close the JSON array at the end of the program or when appropriate
    }
    if (run == -1) { // Special case for finalizing the file
        std::ofstream out_file(file_path.str(), std::ios::app);
        out_file << "\n]\n";
        out_file.close();
        std::ofstream base_file(base_path.str(), std::ios::app);
        base_file << "\n]\n";
        base_file.close();
    }

    std::cout << "Data saved to " << file_path.str() << "\n";
}


// Process Expects and NotExpects for a given time
void process_expects_and_not_expects(const json& expects_json, const json& not_expects_json, int run, int tim) {
    // Add new Expects
    for (const auto& expect : expects_json) {
        int when = expect["when"].get<int>();
        if (when == run) {

            std::string match_name = expect["match_name"].get<std::string>();
            int duration = expect.contains("duration") ? expect["duration"].get<int>() : tim+1;
            active_expects[match_name] = duration;
            log_msg << "          Added Expect: " << match_name << " (Duration: " << duration << ")\n";
        }
    }

    // Add new NotExpects
    for (const auto& not_expect : not_expects_json) {
        int when = not_expect["when"].get<int>();
        if (when == run) {
            std::string match_name = not_expect["match_name"].get<std::string>();
            int duration = not_expect.contains("duration") ? not_expect["duration"].get<int>() : tim+1;
            active_not_expects[match_name] = duration;
            log_msg << "          Added NotExpect: " << match_name << " (Duration: " << duration << ")\n";
        }
    }

    // Update durations and remove expired Expects
    for (auto it = active_expects.begin(); it != active_expects.end();) {
        if (it->second == 0) {
            log_msg << "         Removing Expired Expect: " << it->first << "\n";
            it = active_expects.erase(it);
        } else {
            if (it->second > 0) --it->second; // Decrease duration if greater than 0
            ++it;
        }
    }

    // Update durations and remove expired NotExpects
    for (auto it = active_not_expects.begin(); it != active_not_expects.end();) {
        if (it->second == 0) {
            log_msg<< "         Removing Expired NotExpect: " << it->first << "\n";
            it = active_not_expects.erase(it);
        } else {
            if (it->second > 0) --it->second; // Decrease duration if greater than 0
            ++it;
        }
    }
}

void setup_matches(json& jexp, json& cexpect, json& testPlan, std::string key , int& e_idx, int& ewhen, int& edur, int tim) 
{
    if (testPlan.contains(key) && testPlan[key].is_array()) {   
        jexp = testPlan[key];
        std::cout << key << "  found" << std::endl;

        if (!jexp.empty()) {
            e_idx = 0;
            cexpect = jexp[e_idx]; // Access first test safely
            std::cout << "First " << key << " : " << cexpect.dump() << std::endl;

            // Get the "When" value or use a default
            ewhen = cexpect.contains("when") ? cexpect["when"].get<int>() : tim + 1;
            std::cout << "First Expect when: " << ewhen << std::endl;
            // Get the Duration value or use a default
            edur = cexpect.contains("duration") ? cexpect["duration"].get<int>() : tim + 1;
            std::cout << "First Expect duration: " << ewhen << " duration" << edur<< std::endl;

        } else {
            std::cerr << "Expect array is empty." << std::endl;
        }
    } else {
        std::cerr << "Expect field is missing or not an array." << std::endl;
    }
}
// testplan will have a monitor section
// this will scan the 
// std::string name = match.contains("name") ? match["name"].get<std::string>() : "Unnamed";
void printCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::cout << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << std::endl;
}

std::string getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void run_test_plan(json& testPlan, std::string& testName, int test_run) 
{
    std::string logname = "log/"+testName+"_log.txt";
    //log_delete("log/log.txt");
    //log_open("log/log.txt");
    log_delete(logname);
    log_open(logname);
    if(!testPlan.contains("Target"))
    {
        std::cout << " No field \"Target\" found in test plan" << std::endl;
        log_msg << " No field \"Target\" found in test plan" << std::endl;
        return ;
    }
    std::string target = testPlan["Target"].get<std::string>();
    log_msg  << " Test Target :" << target << std::endl;
    if(!testPlan.contains("Monitor"))
    {
        std::cout << " No field \"Monitor\" found in test plan" << std::endl;
        log_msg << " No field \"Monitor\" found in test plan" << std::endl;
        return ;
    }
    if(!testPlan.contains("Desc"))
    {
        testPlan["Desc"]="Demo test";
    }
    std::string desc = testPlan["Desc"].get<std::string>();
    if(!testPlan.contains("Release"))
    {
        testPlan["Release"]="version: test";
    }
    std::string release = testPlan["Release"].get<std::string>();
    // desc
    // release
    // target

    auto jmon = testPlan["Monitor"];
    if(!(jmon.contains("Query")||jmon.contains("QArray")))
    {
        std::cout << " No field \"Query\" found in Monitor section" << std::endl;
        log_msg << " No field \"Query\" found in Monitor section" << std::endl;
        return ;
    }

    json jmqa, jmq;
    if(jmon.contains("Query"))
        jmq = jmon["Query"];
    if(jmon.contains("QArray"))
    {
        jmqa = jmon["QArray"];
        //log_msg << "we got a query array " << std::endl;
    }

    json jtests, ctest, ntest; // next test
    int when = 1000;
    int test_idx = -1;
    json jexpects; // list of timed expects
    json cexpect; // current expect list
    int ewhen = 1000;
    int edur = 1000;
    int e_idx = -1;

    json jnexpects; // list of times unexpects
    json cnexpect; // current unexpect
    int newhen = 1000;
    int nedur = 1000;
    int ne_idx = -1;

    //std::cout << " Query :" << jmq.dump()<<std::endl;
    int  per = jmon.contains("Period") ? jmon["Period"].get<int>():1;
    int  tim = jmon.contains("Time") ? jmon["Time"].get<int>():10;
    std::string mfile = jmon.contains("MatchFile") ? jmon["MatchFile"].get<std::string>():"test_plan_matches";
    //mfile = "json/" + mfile+ ".json";

    std::string url = jmon.contains("Url") ? jmon["Url"].get<std::string>():"ws://192.168.86.209:9003";
    int seq = jmq.contains("sec") ? jmq["sec"].get<int>():1;
    if (testPlan.contains("Tests") && testPlan["Tests"].is_array()) {   
        jtests = testPlan["Tests"];
        std::cout << "Tests found" << std::endl;

        if (!jtests.empty()) {
            test_idx = 0;
            ntest = jtests[test_idx]; // Access first test safely
            std::cout << "First test: " << ctest.dump() << std::endl;

            // Get the "When" value or use a default
            when = ntest.contains("when") ? ntest["when"].get<int>() : tim + 1;
            std::cout << "First test When: " << when << std::endl;
        } else {
            std::cerr << "Tests array is empty." << std::endl;
        }
    } else {
        std::cerr << "Tests field is missing or not an array." << std::endl;
    }

    setup_matches(jexpects, cexpect, testPlan, "Expects", e_idx, ewhen, edur, tim);
    // if (testPlan.contains("Expects") && testPlan["Expects"].is_array()) {   
    //     jexpects = testPlan["Expects"];
    //     std::cout << "Expects found" << std::endl;

    //     if (!jexpects.empty()) {
    //         e_idx = 0;
    //         cexpect = jexpects[e_idx]; // Access first test safely
    //         std::cout << "First Expect: " << cexpect.dump() << std::endl;

    //         // Get the "When" value or use a default
    //         ewhen = cexpect.contains("when") ? cexpect["when"].get<int>() : tim + 1;
    //         std::cout << "First Expect when: " << when << std::endl;
    //         // Get the Duration value or use a default
    //         edur = cexpect.contains("duration") ? cexpect["duration"].get<int>() : tim + 1;
    //         std::cout << "First Expect duration: " << ewhen << " duration" << edur<< std::endl;

    //     } else {
    //         std::cerr << "Expect array is empty." << std::endl;
    //     }
    // } else {
    //     std::cerr << "Expect field is missing or not an array." << std::endl;
    // }

    setup_matches(jnexpects, cnexpect, testPlan, "NotExpects", ne_idx, newhen, nedur, tim);
    // if (testPlan.contains("NotExpects") && testPlan["NotExpects"].is_array()) {   
    //     jnexpects = testPlan["NotExpects"];
    //     std::cout << "NotExpects found" << std::endl;

    //     if (!jnexpects.empty()) {
    //         ne_idx = 0;
    //         cnexpect = jexpects[ne_idx]; // Access first test safely
    //         std::cout << "First NotExpect: " << cnexpect.dump() << std::endl;

    //         // Get the "When" value or use a default
    //         newhen = cnexpect.contains("when") ? cnexpect["when"].get<int>() : tim + 1;
    //         std::cout << "First NotExpect when: " << newhen << std::endl;
    //         // Get the Duration value or use a default
    //         nedur = cnexpect.contains("duration") ? cnexpect["duration"].get<int>() : tim + 1;
    //         std::cout << "First NotExpect duration: " << newhen << " duration" << nedur<< std::endl;

    //     } else {
    //         std::cerr << "NotExpect array is empty." << std::endl;
    //     }
    // } else {
    //     std::cerr << "NotExpect field is missing or not an array." << std::endl;
    // }

    std::cout << " read matches from " << mfile<<std::endl;
    load_matches(mfile);
    std::string query =  jmq.dump();
    std::cout << "now running query every " << per <<" seconds  for " << tim << " seconds "<<std::endl; 


    if(debug)std::cout << "Starting monitor now;  when " << when <<  std::endl;

    for (int run = 0; run < tim ; ++run)
    {
        //if(debug)
        log_msg << "\n ***** Start run  " << run << std::endl;
        process_expects_and_not_expects(jexpects, jnexpects, run, tim);
        if(debug)std::cout << " Process expects done " << run << " when "<< when<< std::endl;

        if (run >= when)
        {
            std::cout << "Sending test now " << run << " when " << when <<  std::endl;
            ctest = ntest;

            std::vector<std::string> qstrings;

            if(ctest.contains("QArray"))
            {
                for (auto qmaItem : ctest["QArray"])
                {
                    qstrings.push_back(qmaItem.dump());
                    //log_msg << " Test QArray for run [" << run << "] " << qmaItem.dump() <<  std::endl;
 
                }
            }
            else if (ctest.contains("Query"))
            {
                qstrings.push_back(ctest["Query"].dump());
                //log_msg << " Test Query for run [" << run << "] " << ctest["Query"].dump() <<  std::endl;
            }

            if(!qstrings.empty())
            {
                int qidx = 0;
                for (const auto& query : qstrings) {
                    //log_msg << " Test :" << test_idx <<" run [" << run << "]  idx [" << qidx++<< "]" << query <<  std::endl;
                    std::string response = run_wscat(url, query);
                    //log_msg << "            -> response " << response <<  std::endl;
                    if ( response.empty() || response == "Failed")
                    {
                        run = tim+1;
                        break;
                    }
                }
            }
            // Check if test_idx is within the bounds of jtests
            when = tim+1;
            test_idx++;
            if (test_idx < jtests.size()) {
                ntest = jtests[test_idx];
                when = ntest.contains("when") ? ntest["when"].get<int>() : tim+1;
            } else {
                std::cout << "No more tests to send. Test index out of bounds." << std::endl;
                // Handle the case where no more tests are available
                // Optionally, you could break the loop or perform other actions
                when = tim+1; // Set to a value indicating no more tests
            }
        }
        
        if(debug)std::cout << " Getting data run " << run <<  std::endl;

        std::vector<uint16_t> data;
        //log_msg << " Monitor Query " << query <<  std::endl;

        try {
            std::vector<std::string> qstrings;

            if ( !jmqa.empty() )
            {
                for ( auto qmaItem : jmqa)
                {
                    qstrings.push_back(qmaItem.dump());
                    //log_msg << " Monitor QArray at [" << run << "] " << qmaItem.dump() <<  std::endl;

                }
            }
            else
            {
                qstrings.push_back(jmq.dump());
            }
            // for each query string in qstrings run_wscat and append the result to data
            int qidx = 0;
            for (const auto& query : qstrings) {
                if(run == 0)
                    log_msg << "  Monitor  run [" << run << "] " << " query ["<<qidx++<<"] " << query <<  std::endl;
                std::string response = run_wscat(url, query);  // Run the query
                if ( response.empty() || response == "Failed")
                {
                    run = tim+1;
                    break;
                }

                try {
                    json parsed = json::parse(response);        // Parse the JSON response
                    auto newData = parsed["data"].get<std::vector<uint16_t>>();  // Extract data
                    data.insert(data.end(), newData.begin(), newData.end());  // Append new data to the main vector
                } catch (const std::exception& e) {
                    std::cerr << "Failed to parse JSON or run query: " << e.what() << std::endl;
                }
            }
            // std::string response = run_wscat(url, query);
            // json parsed = json::parse(response);
            // data = parsed["data"].get<std::vector<uint16_t>>();
            if ( data.empty())
            {
                run = tim+1;
                log_msg << " No Data  quitting" << std::endl;
                break;
            }
            add_input_vector(run, seq, data);
            //save_run_data(target, run , seq , data);
            //offset += data_size*2; //uint16_t 
        } catch (const std::exception& ex) {
            std::cerr << "Error during WebSocket command: " << ex.what() << "\n";
        }
        //if(debug)
        std::cout << "collect matches for this run " << run << std::endl;

        if ( data.empty() )
        {
            run = tim+1;
            log_msg << " No Data  quitting" << std::endl;
            break;
        }
        json match_json = json::array(); //
        auto mval = find_best_match(run, data,  0.8);
        if(debug)std::cout << "collect matches for this run done " << run << std::endl;
        if(!mval)
        {
            std::cout << " No match found here, create a new one"<<std::endl;
            mval  = create_new_match(run, data);
            if(debug)std::cout << " Match created"<<std::endl;
            //create_new_match(InputVector&current_vector, data);
        }
        std::vector<std::string> match_vec; 
        if(mval)
        {
            match_vec.push_back(mval->name);
            log_msg << "            Match found ["<<mval->name<<"]"<<std::endl;
            
            // Track active Expects and NotExpects
            //std::map<std::string, int> active_expects;
            //std::map<std::string, int> active_not_expects;
            // Check against active Expects
            if (active_expects.find(mval->name) != active_expects.end()) {
                log_msg << "              Expected match [" << mval->name << "] detected. Test " << test_idx << " passed for this part." << std::endl;
                //active_expects[mval->name] -= 1; // Decrease count for this expect
                //if (active_expects[mval->name] == 0) {
                    //active_expects.erase(mval->name); // possibly Remove when no longer expected
                //}
                int t_idx =  test_idx-1;
                std::cout << " Test Results for index :" << t_idx << std::endl;
                
                if (t_idx >= 0 && t_idx < jtests.size()) {
                    // Directly modify the element in the array
                    if(jtests[t_idx].contains("passes")) {
                        std::cout << " Incrementing passes  :" << t_idx << std::endl;
                        int  passes = jtests[t_idx]["passes"].get<int>(); 
                        passes  += 1; // Set fails to 1 if passes is not found
                        jtests[t_idx]["passes"] = passes;
                        std::cout << " done Incrementing passes  :" << t_idx << std::endl;
                    }
                    else
                    {
                        jtests[t_idx]["passes"]  = 1; // Set fails to 1 if passes is not found
                    }
                    std::cout << " Test Results for index :" << t_idx << " ->" << jtests[t_idx].dump() << std::endl;
                }
                //inc the passes for the current test
            }

            // Check against active NotExpects
            if (active_not_expects.find(mval->name) != active_not_expects.end()) {
                log_msg << "               Unexpected match [" << mval->name << "] detected. Test failed for this part." << std::endl;
                //active_not_expects[mval->name] -= 1; // Decrease count for this not-expect
                //if (active_not_expects[mval->name] == 0) {
                    //active_not_expects.erase(mval->name); // possibly Remove when no longer relevant
                int t_idx =  test_idx-1;
                if (t_idx >= 0 && t_idx < jtests.size()) {
                    // Directly modify the element in the array
                    if(jtests[t_idx].contains("fails")) {
                        int  fails = jtests[t_idx]["fails"].get<int>(); 
                        fails  += 1; // Set fails to 1 if passes is not found
                        jtests[t_idx]["fails"] = fails;
                    }
                    else
                    {
                        jtests[t_idx]["fails"]  =1; // Set fails to 1 if passes is not found
                    }
                }
 
            }

            test_json_matches(match_json, run);
        }
        json jmvec = match_vec; 
        save_run_data(testName, test_run, run, seq, data, jmvec);
        if(debug)std::cout <<" Run completed " << run << std::endl;
    }
    json jdummy;
    std::vector<uint16_t>dummy_data;
    save_run_data(testName, test_run , -1, 0 , dummy_data, jdummy);
 
    check_match_consistency();
    // Save matches to file
    save_matches(mfile);
    test_idx = 0;

    int total_passes = 0;
    int total_fails = 0;
    while (test_idx < jtests.size()) {
        // Directly modify the element in the array
        if(!jtests[test_idx].contains("passes")) {
            jtests[test_idx]["fails"] = 1; // Set fails to 1 if passes is not found
            jtests[test_idx]["passes"] = 0; // Set fails to 1 if passes is not found
        }
        if(!jtests[test_idx].contains("fails")) {
            jtests[test_idx]["fails"] = 0; // Set fails to 1 if passes is not found
        }
        total_passes += jtests[test_idx]["passes"].get<int>();
        total_fails += jtests[test_idx]["fails"].get<int>();
        test_idx++;
    }
    // desc
    // release
    // target
    log_msg <<"\n\n\n*************************************************";
    
    log_msg << "\nTest: "<< testName 
            << "\n     Description: " << desc;
    log_msg << "\n     Target: "<< target;
    log_msg << "\n     Release: "<< release;
    log_msg << "\n     Tested on: " << getCurrentDateTime();
    log_msg << "\n\n     results :-" <<std::endl;
    log_msg <<"                         Total Passes  "<< total_passes <<std::endl;
    log_msg <<"                         Total Fails  "<< total_fails <<std::endl;
    test_idx = 0;

    log_msg << "\n\n  Test Details :-" <<std::endl;

    while (test_idx < jtests.size()) {
        std::cout << "          Results for item :" << test_idx << " ->" << jtests[test_idx].dump() << std::endl;
        log_msg << "Test item  :"<< jtests[test_idx]["Desc:"].dump() << std::endl;
        log_msg << "                        Passes :"<<     jtests[test_idx]["passes"].dump() << std::endl;
        log_msg << "                        Fails :"<<     jtests[test_idx]["fails"].dump() << std::endl;
        test_idx++;
    }
    log_msg <<"*************************************************"<<std::endl;
    log_close();
}


// Main Function
int main(int argc, char* argv[]) {
    std::ostringstream filename;

  
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test_plan>\n";
        return 1;
    }
    
    if (argc > 2) {

        std::string dataMap = argv[2];
        dataMap="doc/test_sbmu_mapping.txt";
        test_data_map(dataMap);
        show_data_map();
        ConfigDataList configData;

        test_data_list(configData);
            // // Generate reports

        test_str_to_offset();
        std::cout << "\n\n**************************************"<<std::endl;
        assert_manager.generate_report();

        std::cout << "\n\n**************************************"<<std::endl;
        assert_manager.generate_summary();
        std::cout << "\n\n**************************************"<<std::endl;


        // assert_manager.generate_summary();
        // assert_manager.generate_report();

        return 0;
    } 

    std::string testname = argv[1];

    auto test_run = get_test_run(testname);

    std::string url = "ws://192.168.86.209:9003";

    filename << "json/"<< testname<<".json";

    json testPlan;
    load_test_plan(testPlan, testname);//filename.str()) ;
    run_test_plan(testPlan, testname, test_run);

    return 0;  
}
