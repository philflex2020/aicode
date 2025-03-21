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

 /*
 TODO today
 disable event dispatch done in smEvent.cpp (bit of a hack)

using collects results into a named var
or uses var to set values
show vars
show var



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
#include <queue>

#include <algorithm>
#include <list>
#include <regex>

#include <cstring> // for memset
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> // for close

#include <any>
#include <typeinfo>
#include <typeindex>  // Make sure to include this
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>


#include <fractal_test.h>
#include <fractal_assert.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

//g++ -std=c++17 -o ftest -I ./inc src/fractal_test.cpp
// ./ftest tyest_plan1

bool debug = false;
bool ops_debug = false;
bool ops_test = false;

std::string restart_string;
std::string mb_connect("192.168.86.209:502");

std::string g_url("ws://192.168.86.209:9001");
ModbusClient mbx_client;//("192.168.86.209", 502);
//ModbusClient mbx_client("192.168.86.180", 1502);

                                         
// src                 meth         dest              lim              notes

 // Creating a JSON array
 // Creating a JSON array
json sbms_trans_arr = json::array({
        {{"rtos:bits:201", "3sum", "sbmu:bits:1", "none", "total_undervoltage"}},
        {{"rtos:bits:204", "3sum", "sbmu:bits:4", "none", "total_overvoltage"}},
        {{"rtos:bits:200", "into", "sbmu:bits:49", "none", "ESBCM_lost_communication"}},
        {{"rtos:input:120", "3lim_max", "sbmu:bits:10", "sbmu:hold:120", "low_resistance"}},
        {{"rtos:input:117", "3lim_min", "sbmu:bits:13", "sbmu:hold:121", "module_low_temp"}},
        {{"rtos:input:117", "3lim_max", "sbmu:bits:16", "sbmu:hold:122", "module_high_temp"}},
        {{"rtos:input:125", "min", "sbmu:input:8", "sbmu:input:10", "max_battery_voltage"}},
        {{"rtos:input:123", "max", "sbmu:input:9", "sbmu:input:11", "min_battery_voltage"}},
        {{"rtos:input:101", "sum", "sbmu:input:33", "none", "max_charge_power"}},
        {{"rtos:input:102", "sum", "sbmu:input:32", "none", "max_discharge_power"}},
        {{"rtos:input:105", "sum", "sbmu:input:35", "none", "max_charge_current"}},
        {{"rtos:input:106", "sum", "sbmu:input:34", "none", "max_discharge_current"}},
        {{"rtos:input:0", "count", "sbmu:input:36", "none", "num_daily_charges"}},
        {{"rtos:input:0", "count", "sbmu:input:37", "none", "num_daily_discharges"}}
    });




using json = nlohmann::json;
using namespace std;
int tparse(std::ostringstream& oss);
int ops_parse(std::string filename ,std::ostringstream& oss);
int test_ops_parse(std::map<std::string, OpsTable>& ops_tables, const std::string &filename,std::ostringstream& oss);
void test_ops_run(std::map<std::string, OpsTable>& ops_tables,const std::string& ops_table, const std::string& op_desc, std::ostringstream& oss);



std::map<std::string, std::any> var_tables;

std::string vector_to_string(const std::any& v) {
    std::ostringstream oss;
    if (v.type() == typeid(std::vector<int>)) {
        for (auto& item : std::any_cast<std::vector<int>>(v)) {
            oss << item << " ";
        }
    } else if (v.type() == typeid(std::vector<std::vector<int>>)) {
        for (auto& vec : std::any_cast<std::vector<std::vector<int>>>(v)) {
            oss << "[";
            for (auto& item : vec) {
                oss << item << " ";
            }
            oss << "] ";
        }
    } else if (v.type() == typeid(std::vector<std::vector<std::vector<int>>>)) {
        for (auto& vec2d : std::any_cast<std::vector<std::vector<std::vector<int>>>>(v)) {
            oss << "[";
            for (auto& vec : vec2d) {
                oss << "[";
                for (auto& item : vec) {
                    oss << item << " ";
                }
                oss << "] ";
            }
            oss << "] ";
        }
    }
    return oss.str();
}

// Function to show specific variable
void showvar(std::ostringstream &oss, const std::string &name) { 
    auto it = var_tables.find(name);
    if (it != var_tables.end()) {
        oss << it->first << " : ";
        if (it->second.type() == typeid(int)) {
            oss << std::any_cast<int>(it->second);
        } else if (it->second.type() == typeid(std::string)) {
            oss << std::any_cast<std::string>(it->second);
        } else if (it->second.type() == typeid(std::vector<int>) ||
                   it->second.type() == typeid(std::vector<std::vector<int>>) ||
                   it->second.type() == typeid(std::vector<std::vector<std::vector<int>>>)) {
            oss << vector_to_string(it->second);
        }
        oss << std::endl;
    } else {
        oss << "Variable '" << name << "' not found." << std::endl;
    }
}

void showvars(std::ostringstream &oss) {
    for (const auto& table_pair : var_tables) {
        oss << table_pair.first << " : ";
        if (table_pair.second.type() == typeid(int)) {
            oss << std::any_cast<int>(table_pair.second);
        } else if (table_pair.second.type() == typeid(std::string)) {
            oss << std::any_cast<std::string>(table_pair.second);
        } else if (table_pair.second.type() == typeid(std::vector<int>) ||
                   table_pair.second.type() == typeid(std::vector<std::vector<int>>) ||
                   table_pair.second.type() == typeid(std::vector<std::vector<std::vector<int>>>)) {
            oss << vector_to_string(table_pair.second);
        }
        oss << std::endl;
    }
}

void parseAndSetVar(const std::string& input) {
    std::istringstream iss(input);
    std::string command, name, data;

    iss >> command >> name;
    std::getline(iss, data);
    data.erase(data.begin(), std::find_if(data.begin(), data.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    if (data.front() == '\'' && data.back() == '\'') {
        // It's a string
        data = data.substr(1, data.size() - 2);
        var_tables[name] = data;
    } else if (data.find(',') != std::string::npos || data.find(';') != std::string::npos) {
        // It's a vector or more complex structure
        std::vector<std::vector<std::vector<int>>> matrix;
        std::stringstream ss(data);
        std::string segment;

        while (std::getline(ss, segment, ';')) {
            std::vector<std::vector<int>> matrix2d;
            std::stringstream ss2(segment);
            std::string segment2;

            while (std::getline(ss2, segment2, ',')) {
                std::vector<int> vec;
                std::stringstream ss3(segment2);
                int number;

                while (ss3 >> number) {
                    vec.push_back(number);
                }

                matrix2d.push_back(vec);
            }

            matrix.push_back(matrix2d);
        }

        if (matrix.size() == 1 && matrix[0].size() == 1) {
            var_tables[name] = matrix[0][0];
        } else if (matrix.size() == 1) {
           var_tables[name] = matrix[0];
        } else {
           var_tables[name] = matrix;
        }
    } else {
        // It's a simple number or space-separated list of numbers
        std::vector<int> vec;
        std::stringstream ss(data);
        int num;
        while (ss >> num) {
            vec.push_back(num);
        }

        if (vec.size() == 1) {
            var_tables[name] = vec[0];
        } else {
            var_tables[name] = vec;
        }
    }
}

void parseAndSetVar(const std::string& name, const std::string& value) {
    // Trim value from leading/trailing whitespaces and check for quotes
    std::string trimmed = value;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v")); // Left trim
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r\f\v") + 1); // Right trim

    if (trimmed.front() == '\'' && trimmed.back() == '\'') { // Check for single-quoted string
        trimmed = trimmed.substr(1, trimmed.size() - 2);
        var_tables[name] = trimmed;
    } else if (std::all_of(trimmed.begin(), trimmed.end(), ::isdigit)) { // Check if all are digits
        var_tables[name] = std::stoi(trimmed);
    } else { // Parse as vector
        std::vector<std::vector<std::vector<int>>> matrix;
        std::stringstream ss(trimmed);
        std::string segment;

        while (std::getline(ss, segment, ';')) { // parse 3D segments
            std::vector<std::vector<int>> matrix2d;
            std::stringstream ss2(segment);
            std::string segment2;

            while (std::getline(ss2, segment2, ',')) { // parse 2D segments
                std::vector<int> vec;
                std::stringstream ss3(segment2);
                int number;

                while (ss3 >> number) {
                    vec.push_back(number);
                }

                matrix2d.push_back(vec);
            }

            matrix.push_back(matrix2d);
        }

        if (matrix.size() == 1 && matrix[0].size() == 1) {
            var_tables[name] = matrix[0][0]; // Single vector
        } else if (matrix.size() == 1) {
            var_tables[name] = matrix[0]; // 2D vector
        } else {
            var_tables[name] = matrix; // 3D vector
        }
    }
}

int test_setvar(std::ostringstream &oss) 
{
    parseAndSetVar("name1", "42");
    parseAndSetVar("name2", "1 2 3 4");
    parseAndSetVar("name3", "1 2 3 4,5 6 7 8");
    parseAndSetVar("name4", "'some string'");
    parseAndSetVar("name5", "1 2 3 4,5 6 7 8;21 22 23 24,25 26 27 28;1 2 3 4,4 3 2 1");

    showvars(oss);
    return 0;
}


int test_parse_setvar(std::ostringstream &oss) 
{
    parseAndSetVar("setvar namex 42");
    parseAndSetVar("setvar name2 1 2 3 4");
    parseAndSetVar("setvar name3 1 2 3 4,5 6 7 8");
    parseAndSetVar("setvar name4 'some string'");
    parseAndSetVar("setvar name5 1 2 3 4,5 6 7 8;21 22 23 24,25 26 27 28;1 2 3 4,4 3 2 1");

    showvars(oss);
    return 0;
}

template<typename T>
T getVar(const std::string& name) {
    auto it = var_tables.find(name);
    if (it != var_tables.end()) {
        if (it->second.type() == typeid(T)) {
            return std::any_cast<T>(it->second);
        } else {
            throw std::runtime_error("Type mismatch for variable: " + name);
        }
    } else {
        throw std::runtime_error("Variable not found: " + name);
    }
}
std::type_index getVarTypeId(const std::string& name) {
    auto it = var_tables.find(name);
    if (it != var_tables.end()) {
        return std::type_index(it->second.type());
    } else {
        throw std::runtime_error("Variable not found: " + name);
    }
}

int test_var_types() {
    // Setup example variables
    var_tables["int_var"] = 42;
    var_tables["string_var"] = std::string("hello world");

    try {
        int value = getVar<int>("int_var");
        std::cout << "Value of int_var: " << value << std::endl;

        std::type_index type = getVarTypeId("string_var");
        std::cout << "Type of string_var: " << type.name() << std::endl;

        // This will throw because of type mismatch
        std::string wrongType = getVar<std::string>("int_var");
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}


int read_json_array(json& jdata, const std::string& filename) {
    ifstream file(filename);  // Open the file
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return 1;  // Return 1 to indicate failure
    }

    jdata = json::array();  // Initialize jdata as a JSON array

    string line;
    bool firstLine = true;
    while (getline(file, line)) {  // Read the file line by line
        // Remove leading and trailing whitespaces
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        // Skip empty lines or lines that are just array brackets
        if (line.empty() || line == "[" || line == "]") continue;

        // Remove trailing commas that are part of the array syntax
        if (line.back() == ',') {
            line.pop_back();
        }

        try {
            json j = json::parse(line);  // Parse each line into a JSON object
            jdata.push_back(j);  // Append the parsed object to the array
        } catch (const json::parse_error& e) {
            cerr << "Parse error at byte " << e.byte << ": " << e.what() << endl;
            // Optionally, continue to the next line instead of stopping completely
        }
    }

    return 0;  // Return 0 to indicate success
}

int test_read_array( json &jdata, const std::string& filename, std::ostringstream &oss) {
    //string filename = "json/test_tdefs.json";
    if (read_json_array(jdata, filename) == 0) {
        oss << "Aggregated JSON data:" << std::endl << jdata.dump(4) << std::endl;
    } else {
        oss << "Failed to read JSON data." << std::endl;
    }

    return 0;
}

bool decode_id(MemId &mem_id, RegId& reg_id, int& rack_num, int&offset, uint32_t id)
{
        // Extract fields from the ID
    mem_id = (MemId)((id >> 28) & 0xF);       // 4 bits for mem_id
    reg_id = (RegId)((id >> 24) & 0xF);       // 4 bits for reg_id
    rack_num = (id >> 16) & 0xFF;    // 8 bits for rack_num
    offset = id & 0xFFFF;            // 16 bits for oenffset
    return true;
}

bool code_id(MemId &mem_id, RegId& reg_id, int& rack_num, int&offset, uint32_t id)
{
        // Extract fields from the ID
    id  = (mem_id & 0xF)<<28       // 4 bits for mem_id
        +  (reg_id & 0xf)<< 24
        +  (rack_num & 0xff)<< 16
        +  (offset & 0xffff);
    return true;
}


// Simulated memory storage
std::map<int, std::map<int, std::vector<uint16_t>>> simulatedMemory;
#define NUM_RACKS 12
#define NUM_VALUES_PER_RACK 512

bool sim_init = false;
// Initialize memory simulation
void init_memory_simu() {
    if(!sim_init)
    {
        for (int rack = 0; rack < NUM_RACKS; ++rack) {
            for ( int mt = 0 ; mt < 4; mt++) {
                for ( int rt = 0 ; rt < 4; rt++) {
                    int mid = (mt <<4 + rt);
                    simulatedMemory[mid][rack] = std::vector<uint16_t>(NUM_VALUES_PER_RACK, 0);  // Initialize with zeroes
                }
            }
        }
        sim_init = true;
    }
}

// Dummy placeholders for functions that need to be defined
bool rack_online(int rack_num) { return true; }  // Placeholder for checking if a rack is online

void get_mem_int_val_id(uint32_t id, int num, std::vector<int>& values) {
    MemId mem_id;
    RegId reg_id; 
    int rack_num; 
    int offset;
    decode_id(mem_id, reg_id, rack_num, offset, id);
    int mid = (mem_id <<4 + reg_id);
    for (int i = 0 ; i < num; i++)
        values.push_back(simulatedMemory[mid][rack_num][offset+i]);

}

bool set_value_debug = false;
void set_mem_int_val_id(uint32_t id, int num, int value) {
    MemId mem_id;
    RegId reg_id; 
    int rack_num; 
    int offset;
    decode_id(mem_id, reg_id, rack_num, offset, id);
    int mid = (mem_id <<4 + reg_id);
    int idx = 0;
    if(set_value_debug)printf( " set value to rack %d offset %d value %d\n", rack_num, offset, value );
    simulatedMemory[mid][rack_num][offset+idx] = value;
}

bool set_debug = false;

void set_mem_int_val_id(uint32_t id, int num, const std::vector<int>& values) {
    MemId mem_id;
    RegId reg_id; 
    int rack_num; 
    int offset;
    if(set_debug)printf( " handling id  0x%x \n", id);
    decode_id(mem_id, reg_id, rack_num, offset, id);
    int mid = (mem_id <<4 + reg_id);
    int idx = 0;
    for (int val : values)
    {
        if(set_debug)printf( " mem_id  0x%x \n", mem_id);
        if(set_debug) std::cout<< "setting memory id ["<< (int)mid << "] rack ["<< rack_num <<"] offset ["<<offset+idx<<"]\n";
        if(simulatedMemory.find(mid) == simulatedMemory.end())
            simulatedMemory[mid][rack_num] = std::vector<uint16_t>(NUM_VALUES_PER_RACK, 0);  // Initialize with zeroes

        simulatedMemory[mid][rack_num][offset+idx] = val;
        idx++;
    }
}

// Sets the rack number within the ID by manipulating specific bits
void setRackNum(uint32_t& id, int rack_num) {
    rack_num &= 0xff;
    id &= 0xFF00FFFF;             // Clear the bits where the rack number will be set
    id |= (rack_num << 16) & 0x00FF0000;  // Set the rack number in its designated bit positions
}

// given a list of values and a single id 
// set those values for s=the same id on each rack
// set_rack_values(0x11020010, 10, { 1,0,0,1,0});
void set_rack_values(uint32_t id,  int max_rack, std::vector<int>values)
{
    // Sets the rack number within the ID by manipulating specific bits
    int rack_num = 0;
    for(auto value : values)
    {
        if(rack_num >= max_rack)
            break;
        setRackNum(id, rack_num);
        set_mem_int_val_id(id, 0, value);
        rack_num++;
    }
}

// recover the same id across all racks
void get_rack_values(uint32_t id,  int max_rack, std::vector<int>&values)
{
    // Sets the rack number within the ID by manipulating specific bits
    values.clear();
    for(int rack_num = 0; rack_num < max_rack ; rack_num++)
    {
        setRackNum(id, rack_num);
        get_mem_int_val_id(id, 1, values);
    }
}

// given a list of ids set the values for the next n offsets
void set_idvec_value(std::vector<uint32_t>ids,  int rack, const std::vector<int>&values)
{
    int idx = 0;
    int max_idx = values.size();
    for (auto id : ids )
    {   
        if(idx>max_idx) break;
        // Sets the rack number within the ID by manipulating specific bits
        if(rack>=0) setRackNum(id, rack);
        //std::cout << " setting value " << values[idx]<< std::endl;
        set_mem_int_val_id(id, 0, values[idx]);
        idx++;
    }
}
// given a list of ids get the values for the next n offsets
void get_idvec_value(std::vector<uint32_t>ids,  int rack, std::vector<int>&values)
{
    for (auto id : ids )
    {   
        // Sets the rack number within the ID by manipulating specific bits
        if(rack>=0) setRackNum(id, rack);
        get_mem_int_val_id(id, 1, values);
    }
}

// given an id set the values for the next n offsets
void set_values(uint32_t id,  int rack, std::vector<int>values)
{
    // Sets the rack number within the ID by manipulating specific bits
    if(rack>=0)setRackNum(id, rack);
    set_mem_int_val_id(id, 0, values);
}


void test_sim_mem()
{
    init_memory_simu();
    uint32_t id = 0x11020010;
    // starting with 0x11020010 set a list of values  
    // if rack is >=0 then select rack
    set_values(0x11020010, 0, { 23,45,67,12,45});
    set_rack_values(0x11020020, 10, { 1,0,0,1,0});

    // int num = 0;
    std::vector<int>values;
    // set_mem_int_val_id(id, num, values);
    values.clear();
    get_mem_int_val_id(id, 5, values);
    std::cout << " same rack , fixed id 0x11020010 +5 values ->"; 
    for (auto val : values)std::cout<<val<<" ";std::cout<<std::endl;  
    // change the value on 2
    std::cout << " all racks , fixed id 0x11020020 5 values ->"; 
    values.clear();
    get_rack_values(0x11020020, 5, values);
    for (auto val : values) std::cout<<val<<" "; std::cout <<std::endl;  

    set_values(0x11020020, 2, {3});
    std::cout << " all racks , fixed id 0x11020020 5 values one changed ->"; 
    values.clear();
    get_rack_values(0x11020020, 5, values);
    for (auto val : values) std::cout<<val<<" "; std::cout <<std::endl;  
    // given a list of ids and a list of values set each id to the vaue
    //void set_idvec_value(std::vector<uint32_t>ids,  int rack, std::vector<int>values)
    set_idvec_value( {0x12020020,0x11020032,0x13020014,0x11020023, 0x11020123}, -1, {1,2,3,4,5});
    values.clear();
    get_idvec_value( {0x12020020,0x11020032,0x13020014,0x11020023,0x11020123}, -1, values);
    std::cout << " scatter idlist   => "; 
    for (auto val : values) std::cout<<val<<" "; std::cout <<std::endl;  
    

}


//void set_mem_int_val_id(uint32_t dest_id,int idx, std::vector<int>& values) {}  // Placeholder
void use_rack_mem(int rack_num, void* base_mem, int max, size_t rtos_size) {}  // Placeholder
void release_rack_mem() {}  // Placeholder


// Extracts the MemId from a given uint32_t ID by shifting and masking
MemId getMemId(uint32_t id) {
    return static_cast<MemId>((id >> 28) & 0xF);
}


void transfer_rack(const std::vector<ModbusTransItem>& items_vec, int rack_max) {
    std::map<int, uint32_t> rack_sum_map, rack_max_map, rack_min_map;
    std::map<int, int> rack_max_num_map, rack_min_num_map;
    uint32_t src_id;
    for (int rack_num = 0; rack_num < rack_max; ++rack_num) {
        if (!rack_online(rack_num)) {
            continue;
        }

        //use_rack_mem(rack_num);

        for (const auto& item : items_vec) {
            std::vector<int> values;

            if (getMemId(item.src_id) != MEM_RTOS) {
                continue;
            }
            src_id = item.src_id;

            if (item.meth_str == "sum" ) {
                setRackNum(src_id, rack_num);
                get_mem_int_val_id(src_id, 0, values);

                if (rack_num == 0) {
                    rack_sum_map[item.dest_id] = 0;
                }

                if (!values.empty()) {
                    //uint32_t sum = item.meth_str == "sum" ? values[0] : (values[0] << 16) + values[1];
                    int sum = values[0];
                    rack_sum_map[item.dest_id] += sum;
                }

                if (rack_num == rack_max - 1) {
                    //values = {static_cast<uint16_t>(rack_sum_map[item.dest_id] & 0xFFFF), static_cast<uint16_t>(rack_sum_map[item.dest_id] >> 16)};
                    values = {static_cast<int>(rack_sum_map[item.dest_id])};
                    set_mem_int_val_id(item.dest_id, 0, values);
                }
            }
            else if (item.meth_str == "sum2" ) {
                setRackNum(src_id, rack_num);
                get_mem_int_val_id(src_id, 0, values);

                if (rack_num == 0) {
                    rack_sum_map[item.dest_id] = 0;
                }

                if (!values.empty()) {
                    uint32_t sum = (values[0] << 16) + values[1];
                    rack_sum_map[item.dest_id] += sum;
                }

                if (rack_num == rack_max - 1) {
                    values = {static_cast<int>(rack_sum_map[item.dest_id] & 0xFFFF), static_cast<int>(rack_sum_map[item.dest_id] >> 16)};
                    //values = {static_cast<uint16_t>(rack_sum_map[item.dest_id])};
                    set_mem_int_val_id(item.dest_id, 0, values);
                }
            } 
            else if (item.meth_str == "max" || item.meth_str == "min") {
                if (rack_num == 0) {
                    rack_max_map[item.dest_id] = 0;
                    rack_min_map[item.dest_id] = 0xFFFF;
                }

                uint16_t value = values.empty() ? 0 : values[0];
                if (value > rack_max_map[item.dest_id]) {
                    rack_max_map[item.dest_id] = value;
                    rack_max_num_map[item.dest_id] = rack_num;
                }
                if (value < rack_min_map[item.dest_id]) {
                    rack_min_map[item.dest_id] = value;
                    rack_min_num_map[item.dest_id] = rack_num;
                }
                if (rack_num == rack_max - 1) {
                    values = {static_cast<int>(item.meth_str == "max" ? rack_max_map[item.dest_id] : rack_min_map[item.dest_id])};
                    set_mem_int_val_id(item.dest_id, 0, values);
                }
            }
        }

        //release_rack_mem();
    }
}

// void transfer_rack(const std::vector<ModbusTransItem>& items_vec) {
// 	// sadly we need one of these for each item
// 	std::map<int,uint16_t> rack_max_map;
// 	std::map<int,uint16_t> rack_min_map;
// 	std::map<int,uint16_t> rack_max_num_map;
// 	std::map<int,uint16_t> rack_min_num_map;
// 	std::map<int,uint16_t> rack_max_group;
// 	std::map<int,uint16_t> rack_min_group;
// 	std::map<int,uint32_t> rack_sum_map;
// 	uint16_t rack_min = 0xffff;
// 	uint16_t rack_max_num=0xff;
// 	uint16_t rack_min_num=0xff;

//     for (int rack_num = 0; rack_num < rack_max; ++rack_num) {
//         //use_rack_mem(rack_num, rtos_node->base_mem, rack_max, rtos_size);
//         for (const auto& item : items_vec) {
// 			// create a sum of all the input values into item.dest_reg
//             uint32_t src_id = item.src_id;
//             if (item.meth_str == "sum" && getMemId(item.src_id) == MEM_RTOS) 
//             { 
// 				if ((rack_num == 0)) {
// 					rack_sum_map[item.dest_id] = 0;
// 				}
//                 std::vector<int>values;
//                 uint16_t rack_value = 0;
// 				if(rack_online(rack_num)) 
// 				{
//                     setRackNum(src_id, rack_num);
//                     get_mem_int_val_id(src_id, 0, values);
// 					rack_sum_map[item.dest_id] += values[0];
// 				}
// 				if ((rack_num == rack_max -1 )) {
// 					rack_value = (uint16_t)rack_sum_map[item.dest_id];
//                     values.clear();
//                     values.push_back(rack_value);
//                     set_mem_int_val_id(item.dest_id, 0, values);
// 				}

// 			}
//             // 			// 32 bit summaries
//             else if (item.meth_str == "sum2")
// 			{
//                 std::vector<int>values;
// 				if ((rack_num == 0)) {
// 					rack_sum_map[item.dest_reg]= 0;
// 				}

// 				uint16_t rack_lo_value = 0;
// 				uint16_t rack_hi_value = 0;
//                 uint16_t rack_ret = 0;
// 				if(rack_online(rack_num) && item.src_reg != -1) 
// 				{
//                     // this may work
//                     get_mem_int_val_id(src_id, 0, values);
//                     get_mem_int_val_id(src_id, 1, values);
// 					rack_sum_map[item.dest_reg] += ((values[0]<<16)+(values[1]));
// 				}
// 				if ((rack_num == rack_max -1 )) {
//                     rack_sum = rack_sum_map[item.dest_id];
// 					rack_hi_value = (uint16_t)(rack_sum>>16);
// 					rack_lo_value = (uint16_t)(rack_sum_map[item.dest_id] & 0xffff);
//                     values.clear();
//                     values.push_back(rack_lo_value);
//                     values.push_back(rack_hi_value);
//                     set_mem_int_val_id(item.dest_id, 0, values);
// 				}
// 			}
//             else if (item.meth_str == "max" && getMemId(item.src_id) == MEM_RTOS) 
//             { 
//                 std::vector<int>values;
//                 values.clear();
//                 uint16_t rack_value = 0;
// 				if ((rack_num == 0)) {
// 					rack_max_map[item.dest_id]= 0;
// 					rack_max_num_map[item.dest_id]= 0;
// 				}


// 				if(rack_online(rack_num)) 
// 				{
//                     uint16_t rack_max_num = rack_max_num_map[item.dest_id];
//                     uint16_t rack_max_val = rack_max_map[item.dest_id];					
//                     setRackNum(src_id, rack_num);
//                     get_mem_int_val_id(src_id, 0, values);
//                     rack_value = values[0];
//                     if(rack_max_val < rack_value)
//                     {
//                         rack_max_map[item.dest_id] = rack_value;
//                         rack_max_num_map[item.dest_id] = rack_num;
//                     }
// 				}
// 				if ((rack_num == rack_max -1 )) {
//                     values.clear();
//                     values.push_back(rack_max_map[item.dest_id]);
//                     set_mem_int_val_id(item.dest_id, 0, values);
// 				}

// 			}
//             else if (item.meth_str == "min" && getMemId(item.src_id) == MEM_RTOS) 
//             { 
//                 std::vector<int>values;
//                 values.clear();
//                 uint16_t rack_value = 0;
// 				if ((rack_num == 0)) {
// 					rack_min_map[item.dest_id]= 0xffff;
// 					rack_min_num_map[item.dest_id]= 0;
// 				}


// 				if(rack_online(rack_num)) 
// 				{
//                     uint16_t rack_min_num = rack_min_num_map[item.dest_id];
//                     uint16_t rack_min_val = rack_min_map[item.dest_id];					
//                     setRackNum(src_id, rack_num);
//                     get_mem_int_val_id(src_id, 0, values);
//                     rack_value = values[0];
//                     if(rack_min_val > rack_value)
//                     {
//                         rack_min_map[item.dest_id] = rack_value;
//                         rack_min_num_map[item.dest_id] = rack_num;
//                     }
// 				}
// 				if ((rack_num == rack_max -1 )) {
//                     values.clear();
//                     values.push_back(rack_min_map[item.dest_id]);
//                     set_mem_int_val_id(item.dest_id, 0, values);
// 				}

// 			}
//         }
//         release_rack_mem();
//     }
// }

// 			// 32 bit summaries
//             else if (item.meth_str == "sum2" && item.src_str == "rinput") 
// 			{
// 				if ((rack_num == 0)) {
// 					rack_sum_map[item.dest_reg]= 0;
// 				}

// 				uint16_t rack_lo_value = 0;
// 				uint16_t rack_hi_value = 0;
//                 uint16_t rack_ret = 0;
// 				int read_mode = READ_INPUT_REG;
// 				int write_mode = READ_INPUT_REG; // if item.dest_str == input
// 				if(rack_online(rack_num) && item.src_reg != -1) 
// 				{
// 					Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg, &rack_hi_value);
// 					Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg+1, &rack_lo_value);

// 					rack_sum_map[item.dest_reg] += (rack_hi_value<<16);
// 					rack_sum_map[item.dest_reg] += rack_lo_value;
// 				}
// 				if ((rack_num == rack_max -1 )) {
// 					rack_hi_value = (uint16_t)(rack_sum_map[item.dest_reg]>>16);
// 					rack_lo_value = (uint16_t)(rack_sum_map[item.dest_reg] & 0xffff);
// 					Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg, rack_hi_value, &rack_ret);
// 					Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg+1, rack_lo_value, &rack_ret);
// 				}

// 			}
//             // 3sum
//             else if (item.meth_str == "3sum" && item.src_str == "rbits") {
//                 bool output_set[3] = {false, false,false};

//                 for (int count = 0; count < 3; ++count) {
//                     uint16_t rack_value = 0;
//                     uint16_t sum_value = 0;
//                     uint16_t ret_value = 0;
// 					int read_mode = READ_BITS_REG;
// 					int write_mode = READ_BITS_REG;
// 					if(rack_online(rack_num) && item.src_reg != -1) {

//                     	// if not alreasdy set; read the incoming value from the rack
// 						if(!output_set[count])
// 						{
//                     		Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg + count, &rack_value);

//                     		if (rack_value > 0) 
// 							{
//                         		sum_value = 1;
//                         		output_set[count] = true;
//                     			// Write the sum_value to the destination register
//                     			Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg + count, sum_value, &ret_value);
// 							}
//                     	}
//                 	}

//                 	// If none of the input values were > 0, ensure the destination registers are cleared
//                 	if ((rack_num == rack_max -1)) {
// 						if(!output_set[count]) {
//                         	sum_value = 0;
//                         	uint16_t ret_value = 0;
//                         	Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg + count, sum_value, &ret_value);
//                     	}
// 					}
//                 }
//             }
// 			// 3lim_max 3lim_min
// 			else if ((item.meth_str == "3lim_max" || item.meth_str == "3lim_min") && item.dest_str == "bits") 
// 			{
// 				bool output_set[3] = {false, false,false};

//                 for (int count = 0; count < 3; ++count) {
//                     uint16_t rack_value = 0;
//                     uint16_t sum_value = 0;
//                     uint16_t ret_value = 0;
//                     uint16_t lim_value = 0;
//                     uint16_t diff_value = 0;
// 					int read_mode = READ_INPUT_REG;
// 					int write_mode = READ_BITS_REG;
// 					if(rack_online(rack_num) && item.src_reg != -1) {

//                     	// if not already set; read the incoming value from the rack inputs
// 						// also read the diff value assume that this is always lim_reg+3
// 						if(!output_set[count])
// 						{
//                     		Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg + count, &rack_value);
//                     		Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.lim_reg + count, &lim_value);
//                     		Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.lim_reg + 3, &diff_value);

// 							if (item.meth_str == "3lim_max")
// 							{
//                     			if (rack_value > lim_value + diff_value) 
// 								{
//                         			output_set[count] = true;
// 								}
// 							}
// 							else if (item.meth_str == "3lim_max")
// 							{
//                     			if (rack_value < lim_value - diff_value) 
// 								{
//                         			output_set[count] = true;
//                     				// Write the sum_value to the destination register
// 								}
// 							}
// 							if (output_set[count])
// 							{
//                     			// Write the sum_value to the destination register
//                         		sum_value = 1;
//                     			Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg + count, sum_value, &ret_value);
// 							}

//                     	}
//                 	}

//                 	// If none of the input values outside limits, ensure the destination registers are cleared
//                 	if ((rack_num == rack_max -1)) {
// 						if(!output_set[count]) {
//                         	sum_value = 0;
//                         	uint16_t ret_value = 0;
//                         	Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg + count, sum_value, &ret_value);
//                     	}
// 					}
//                 }
// 			}
// 			// into (bits)
// 			else if (item.meth_str == "into" && item.src_str == "rbits") 
// 			{
// 				bool output_set = false;

// 				uint16_t rack_value = 0;
// 				uint16_t sum_value = 0;
// 				uint16_t ret_value = 0;
// 				int read_mode = READ_BITS_REG;
// 				int write_mode = READ_BITS_REG;
// 				if(rack_online(rack_num) && item.src_reg != -1) 
// 				{
// 					// if not alreasdy set; read the incoming value from the rack
// 					if(!output_set)
// 					{
// 						Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg, &rack_value);

// 						if (rack_value > 0) 
// 						{
// 							sum_value = 1;
// 							output_set = true;
// 							// Write the sum_value to the destination register
// 							Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg, sum_value, &ret_value);
// 						}
// 					}

//                 	// If none of the input values were > 0, ensure the destination registers are cleared
//                 	if ((rack_num == rack_max -1)) {
// 						if(!output_set) {
//                         	sum_value = 0;
//                         	uint16_t ret_value = 0;
//                         	Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg, sum_value, &ret_value);
//                     	}
// 					}
//                 }
// 			}
// 			// input max or min  assume the rack num is to be placed into the lim reg ( for now)
// 			// we may also need the group or module num
// 			else if ((item.meth_str == "max" || item.meth_str == "min" ) 
// 								&& item.src_str == "rinput") 
// 			{
// 				bool output_set = false;

// 				uint16_t rack_value = 0;
// 				uint16_t sum_value = 0;
// 				uint16_t ret_value = 0;
// 				uint16_t sum_num = 0;

// 				int read_mode = READ_BITS_REG;
// 				int write_mode = READ_BITS_REG;
// 				if(rack_num == 0)
// 				{
// 					rack_max_map[item.dest_reg] = 0;
// 					rack_min_map[item.dest_reg] = 0xffff;
// 					rack_max_num_map[item.dest_reg] = 0xffff;
// 					rack_min_num_map[item.dest_reg] = 0xffff;

// 				}
// 				if(rack_online(rack_num) && item.src_reg != -1) 
// 				{
// 					Esmu_ModbusSlave_ReadCallback(read_mode, rack_num, item.src_reg, &rack_value);

// 					if (rack_value > rack_max_map[item.dest_reg]) 
// 					{
// 						rack_max_map[item.dest_reg] =  rack_value;
// 						rack_max_num_map[item.dest_reg] = rack_num;
// 					}
// 					if (rack_value < rack_min_map[item.dest_reg]) 
// 					{
// 						rack_min_map[item.dest_reg] =  rack_value;
// 						rack_min_num_map[item.dest_reg] = rack_num;
// 					}
//                     // we could use the lim reg to capture the numbers
//                 	// If none of the input values were > 0, ensure the destination registers are cleared
//                 	if ((rack_num == rack_max -1)) 
// 					{
// 						if (item.meth_str == "max")
// 						{
// 							// did we find one
// 							if (rack_max_num_map[item.dest_reg] != 0xffff)
// 							{
//                        			sum_value = rack_max_map[item.dest_reg];
// 								sum_num =  rack_max_num_map[item.dest_reg];
//                         		uint16_t ret_value = 0;
//                         		Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg, sum_value, &ret_value);
//                         		Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.lim_reg, sum_num, &ret_value);
// 							}
//                     	}
// 						else if (item.meth_str == "min")
// 						{
// 							// did we find one
// 							if (rack_min_num_map[item.dest_reg] != 0xffff)
// 							{
//                        			sum_value = rack_min_map[item.dest_reg];
// 								sum_num =  rack_min_num_map[item.dest_reg];
//                         		uint16_t ret_value = 0;
//                         		Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.dest_reg, sum_value, &ret_value);
//                         		Esmu_ModbusSlave_WriteCallback(write_mode, rack_num, item.lim_reg, sum_num, &ret_value);
// 							}
//                     	}
// 					}
//                 }
// 			}
	
//         }
//         release_rack_mem();
//     }
// }



// using data keys. we compress sbmu:bits:234[:num] into a var key 
//    sm_name 4 bits
//    reg_type 4 bits
//    zone 8 bits ( rack num perhaps)
//    offset  16 bits
// this is all encoded into the DataItem key
// uint32_t key;
// this will give us the concept of compressed set_key and get_key options

// notes 
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

std::map<std::string, TestTable> test_tables;
std::map<std::string, DataTable> data_tables;
std::map<std::string, DataTable> test_data_tables;
std::map<std::string, OpsTable> test_ops_tables;
std::map<std::string, DataMap> data_maps;
std::map<std::string, DataMap> test_data_maps;

// Map to store method names and their corresponding functions
std::map<std::string, MethStruct> meth_dict;

//TODO 
// Integrate test Queries
// Integrate Match Queries
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
// Added Rack Number to generateQueries


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

RegType get_key_reg_type(uint32_t key)
{
    return (RegType)((key & 0x0F000000) >> 24);
}

System get_key_sm_name(uint32_t key)
{
    return (System)((key&0xf000000)>>28);    
    
}

uint16_t get_key_offset(uint32_t key)
{
    return key&0xffff;    
}

uint16_t get_key_zone(uint32_t key)
{
    return key&0xFF0000>>16;    
}

#define RACK_MEM_SIZE (32*1024)
#define RTOS_MEM_SIZE (64*1024)
uint8_t* sbmu_mem  = nullptr;
uint8_t* rack_mem  = nullptr;
uint8_t* rtos_mem  = nullptr;

uint8_t* get_shm_mem(System sm_name, int rack_num)
{
    if(sm_name == System::SBMU)
        return sbmu_mem;
    else if(sm_name == System::RACK)
    {
        uint32_t offset = RACK_MEM_SIZE * rack_num;
        return &rack_mem[offset];
    }
    else if(sm_name == System::RTOS)
    {
        uint32_t offset = RTOS_MEM_SIZE * rack_num;
        return &rtos_mem[offset];
    }
    return nullptr;    
}

uint16_t set_dataItem(uint32_t key, uint16_t value, int rack_num)
{
    // item->key holds the clues
    RegType reg_type = get_key_reg_type(key);
    System sm_name = get_key_sm_name(key);
    uint16_t offset = get_key_offset(key);
    if (reg_type == RegType::SM16 || reg_type == RegType::SM8)
    {
        if (sm_name == System::SBMU)
        {
            if(reg_type == RegType::SM8)
            {
                uint8_t*shm_mem = get_shm_mem(System::SBMU, rack_num);
                if(shm_mem)
                    shm_mem[offset] = uint8_t(value);             
            }
            else
            {
                uint16_t*shm_mem = (uint16_t*)get_shm_mem(System::SBMU, rack_num);
                offset /= 2;                
                if(shm_mem)
                    shm_mem[offset] = value;             
            }
        }
        else if (sm_name == System::RTOS || sm_name == System::RACK)
        {
            if(reg_type == RegType::SM8)
            {
                uint8_t*shm_mem = get_shm_mem(sm_name, rack_num);
                if(shm_mem)
                    shm_mem[offset]= (uint8_t)value;             
            }
            else
            {
                uint16_t*shm_mem = (uint16_t*)get_shm_mem(sm_name, rack_num);
                offset /= 2;                
                if(shm_mem)
                    shm_mem[offset] = value;             
            }
        }
    }
    else if (reg_type == RegType::BITS || reg_type == RegType::COIL || reg_type == RegType::HOLD || reg_type == RegType::INPUT)
    {
        if (sm_name == System::SBMU)
        {
            //TODO getesmu stuff
            // or we can use modbus remote get ctx
            return value;            
        }
        else if (sm_name == System::RTOS || sm_name == System::RACK)
        {
            //use_rack_mem to switch racks
            // 
            // release rackmem
            return value;            
        }
    }
    return value;
}

// this will honor mappedkeys just one level
uint16_t set_dataItem(DataItem* item, uint16_t value, int rack_num)
{
    if (item->mappedkeys.size() > 0 )
    {
        for ( auto mitem : item->mappedkeys)
        {
            set_dataItem(mitem, value, rack_num);
        }
        return value;
    }
    set_dataItem(item->mykey, value, rack_num);
    return value;
}


uint16_t get_dataItem(DataItem* item, int rack_num)
{
    // item->key holds the clues
    RegType reg_type = get_key_reg_type(item->mykey);
    System sm_name = get_key_sm_name(item->mykey);
    uint16_t offset = get_key_offset(item->mykey);
    uint16_t val = 0;
    if (reg_type == RegType::SM16 || reg_type == RegType::SM8)
    {
        if (sm_name == System::SBMU)
        {
            if(reg_type == RegType::SM8)
            {
                uint8_t*shm_mem = get_shm_mem(System::SBMU, rack_num);
                return (shm_mem[offset]);             
            }
            else
            {
                uint16_t*shm_mem = (uint16_t*)get_shm_mem(System::SBMU, rack_num);
                offset /= 2;                
                return (shm_mem[offset]);             
            }
        }
        else if (sm_name == System::RTOS || sm_name == System::RACK)
        {
            if(reg_type == RegType::SM8)
            {
                uint8_t*shm_mem = get_shm_mem(sm_name, rack_num);
                return (shm_mem[offset]);             
            }
            else
            {
                uint16_t*shm_mem = (uint16_t*)get_shm_mem(sm_name, rack_num);
                offset /= 2;                
                return (shm_mem[offset]);             
            }
        }
    }
    else if (reg_type == RegType::BITS || reg_type == RegType::COIL || reg_type == RegType::HOLD || reg_type == RegType::INPUT)
    {
        if (sm_name == System::SBMU)
        {
            //TODO getesmu stuff
            // or we can use modbus remote get ctx
            return 0;            
        }
        else if (sm_name == System::RTOS || sm_name == System::RACK)
        {
            //use_rack_mem to switch racks
            // 
            // release rackmem
            return 0;            
        }
    }
    return 0xFFFF;
}


// struct TransItem {
//     std::string src_str;
//     std::string meth_str;
//     std::string dest_str;
//     std::string lim_str;    // where we find the limits if there are any , used in the 3lim method 
//     std::string name;
// };

// Define how to serialize TransItem to JSON
void to_json(nlohmann::json& j, const TransItem& item) {
    j = nlohmann::json{
        {"src", item.src_str},
        {"meth", item.meth_str},
        {"dest", item.dest_str},
        {"lim", item.lim_str},
        {"name", item.name}
    };
}

std::string TransItemtoJsonString(const TransItem& item)
{
    json js;
    js = item;
    return js.dump();
}
// maxs
// rtos:input:11:max_temp_num
// rtos:input:12:max_temp
// rtos:input:13:max_temp_mod
// rtos:input:14:max_temp_module
// rtos:input:21:max_voltage
// rtos:input:22:max_mod_volt
// rtos:input:23:max_mod_volt_num
// rtos:input:26:max_mod_volt
// rtos:input:28:max_soc
// rtos:input:29:max_soc_mod
// rtos:input:30:max_soc_mod_num
// rtos:input:35:max_soh
// rtos:input:36:max_soh_mod
// rtos:input:37:max_soh_mod_num
// rtos:input:48:post_temp_max
// rtos:input:49:post_temp_max_mod
// rtos:input:50:post_temp_max_mod_num
// rtos:input:55:max_module_volt
// rtos:input:56:max_module_volt_num
// rtos:input:57:max_module_volt
// rtos:input:58:max_module_volt_num
// rtos:input:154:volt_max_num
// rtos:input:156:temp_max_num
// rtos:input:158:soc_max_num
// rtos:input:160:soh_max_num
// sbmu:input:7:max_pack_voltage
// sbmu:input:9:max_battery_voltage
// sbmu:input:12:max_battery_temp
// sbmu:input:13:max_battery_temp_pack_number
// sbmu:input:14:max_battery_temp_group
// sbmu:input:32:max_discharge_power
// sbmu:input:33:max_charge_power
// sbmu:input:34:max_discharge_current
//  sbmu:input:35:max_charge_current
//mins
// 


// transfer Items

std::map<std::string, TransItemTable>trans_tables;

// json/system_config.json 
// Function to parse JSON and populate the structures
void parseTransJson(const std::string& filepath) {
    // Open the JSON file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        return;
    }

    // Parse the JSON file
    json jsonData;
    file >> jsonData;

    // Iterate through the JSON array
    for (const auto& tableData : jsonData) {
        TransItemTable table;
        table.name = tableData["table"];
        table.desc = tableData["desc"];

        // Iterate through the items in the table
        for (const auto& itemData : tableData["items"]) {
            TransItem item;
            item.src_str = itemData["src"];
            item.meth_str = itemData["meth"];
            item.dest_str = itemData["dest"];
            item.lim_str = itemData["lim"];
            item.name = itemData["name"];
            table.items.push_back(item);
        }

        // Add the table to the map
        trans_tables[table.name] = table;
    }
}

// Function to print the structures (for verification)
void printTransTables() {
    for (const auto& [tableName, table] : trans_tables) {
        std::cout << "Table: " << tableName << "\n";
        std::cout << "Description: " << table.desc << "\n";
        std::cout << "Items:\n";
        for (const auto& item : table.items) {
            std::cout << "  - Src: " << item.src_str << "\n";
            std::cout << "    Meth: " << item.meth_str << "\n";
            std::cout << "    Dest: " << item.dest_str << "\n";
            std::cout << "    Lim: " << item.lim_str << "\n";
            std::cout << "    Name: " << item.name << "\n";
        }
        std::cout << "\n";
    }
}

// Function signature for methods
using MethFunction = std::function<int(const TransItem&)>;

// Mock functions for get_var and set_var
uint16_t get_trans_var(const std::string& var_name, int rack_num) {
    // Replace this with actual logic to read the variable
    std::cout << "Reading variable: " << var_name << "\n";
    return 0; // Mock value
}

void set_trans_var(const std::string& var_name, uint16_t value, int rack_num) {
    // Replace this with actual logic to write the variable
    std::cout << "Writing variable: " << var_name << " with value: " << value << "\n";
}


// Method implementations
int method_into(const TransItem& item) {
    int rack_num = 0;
    uint16_t src_value = get_trans_var(item.src_str, rack_num);
    set_trans_var(item.dest_str, src_value, rack_num);
    return 0; // Success
}

int method_3sum(const TransItem& item) {
    int rack_num = 0;
    uint16_t src_value1 = get_trans_var(item.src_str + "_1", rack_num);
    uint16_t src_value2 = get_trans_var(item.src_str + "_2", rack_num);
    uint16_t src_value3 = get_trans_var(item.src_str + "_3", rack_num);
    uint16_t sum = src_value1 + src_value2 + src_value3;
    set_trans_var(item.dest_str, sum, rack_num);
    return 0; // Success
}

// Add more methods as needed...

// Function to initialize the method dictionary
void initializeMethodDictionary() {
    meth_dict["into"] = {"into","transfer data from src to dest", method_into};
    meth_dict["3sum"] = {"3sum", "find any values set in any rack", method_3sum};
    // Add more methods here...
}

// Function to process a TransItem using the method dictionary
int processTransItem(const TransItem& item) {
    auto it = meth_dict.find(item.meth_str);
    if (it != meth_dict.end()) {
        return it->second.func(item); // Call the corresponding method
    } else {
        std::cerr << "Error: Method '" << item.meth_str << "' not found.\n";
        return -1; // Failure
    }
}

// Function to process all items in a table
void processTransTable(const TransItemTable& table) {
    std::cout << "Processing table: " << table.name << "\n";
    for (const auto& item : table.items) {
        int result = processTransItem(item);
        if (result == 0) {
            std::cout << "  - Item '" << item.name << "' processed successfully.\n";
        } else {
            std::cout << "  - Item '" << item.name << "' failed to process.\n";
        }
    }
}

std::vector<std::string> StringWords(const std::string& str) {
    std::istringstream iss(str);
    std::vector<std::string> words;
    std::string word;
    // Extract words from the stream, ignoring extra spaces automatically
    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

// tested
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
        //std::cerr << "Error: xxxx Invalid offset value: [" << offset_str <<"]"<< std::endl;
        return -1;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Offset value out of range: " << offset_str << std::endl;
        //throw;
    }
    return 0;
}
// Function to trim whitespace and specific control characters from both ends of a string
std::string trim(const std::string& str) {
    auto is_not_trim_char = [](char c) {
        return !std::isspace(c) && c != '\a'; // Check for space and bell character
    };

    size_t first = std::find_if(str.begin(), str.end(), is_not_trim_char) - str.begin();
    if (first == str.size()) return ""; // All characters are trimmable

    size_t last = std::find_if(str.rbegin(), str.rend(), is_not_trim_char).base() - str.begin();
    return str.substr(first, last - first);
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



// Generate query string uding the new ID interfave
// note that the rack will already be embedded in the id
std::string generateIdQuery(const std::string& action, int seq, uint32_t id, const std::string& type, int num, const std::vector<int>&values) {
    std::ostringstream query;
    std::ostringstream os_name;
    os_name << "0x" << std::hex << std::uppercase << id << std::dec;

    query << "{\"action\":\""<< action <<"\", \"seq\":" <<seq<< ", \"sm_name\":\"" << os_name.str()
          << "\", \"reg_type\":\"" << type
          << "\", \"offset\":\"" << (int)(id&0xffff)
          << "\", \"num\":" << num
          << ", \"data\":[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) query << ", ";
        query << values[i];
    }
    query << "]}";
    return query.str();
}


// Generate query string for a group of ConfigDataPairs
std::string generateGroupQuery(const std::string& action, int seq, int rack, const ConfigDataList& group, const std::string& system, const std::string& type, int startOffset) {
    std::ostringstream query;
    std::ostringstream os_name;
    os_name << system;
    if (rack>=0)
    {
        os_name << "_" << rack;
    }
    query << "{\"action\":\""<< action <<"\", \"seq\":" <<seq<< "\", \"sm_name\":\"" << os_name.str()
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
void GenerateQueries(const std::string& action, int seq, int rack, std::vector<std::string> &queries, const ConfigDataList& data) {
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
                std::string query = generateGroupQuery(action, seq, rack, currentGroup, currentSystem, currentType, startOffset);
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
        std::string query = generateGroupQuery(action, seq, rack, currentGroup, currentSystem, currentType, startOffset);
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
        std::string query = generateGroupQuery("get", 22, 1, currentGroup, currentSystem, currentType, offset);
        currentGroup.clear();
        
        std::cout << query << std::endl;
    }
}



//data/sbmu_mapping.txt
// deprecated
int test_data_map(std::string& fileName) {

    load_data_map(fileName, systems, reverseMap);
    return 0;
}

// decode sbmu:bits:345 into a component type
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

// "rtos:min_soc[2]"
void decode_name(std::string &sm_name, std::string &item, int &index, const std::string &input) {
    // Regex to match the format "sm_name:item[:[index]]"
    std::regex pattern(R"(([^:]+):([^:\[\]]+)(?:\[(\d+)\])?)");
    std::smatch matches;

    // Default index to -1 if not found
    index = -1;

    if (std::regex_match(input, matches, pattern)) {
        if (matches.size() > 1) {
            sm_name = matches[1].str();
            item = matches[2].str();
            if (matches.size() == 4 && matches[3].matched) {
                index = std::stoi(matches[3].str());
            }
        }
    } else {
        std::cerr << "Input string does not match the expected format." << std::endl;
        sm_name = "";
        item = "";
    }
}


std::shared_ptr<ConfigItem>find_name(const std::string& item_name)
{
    // split 
    std::string sm_name, item;
    int index;
    decode_name(sm_name, item, index, item_name);
    return find_name(sm_name, item);
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
// merge into grpups for comsequative sets / gets
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
// we may not use these  since the dataitem type may overrule the whole thing
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
    double result = 0.0;

    // Ensure vectors are of the same size
    if (input.size() != match->vector.size()) {
        std::cout << "calculate match error in sizes input " << input.size() << " match " << match->vector.size()<<std::endl;
        return result; // Not a valid match
    }
    if(debug)std::cout << "calculate match and score 1" << std::endl;

    for (size_t i = 0; i < input.size(); ++i) {
        if(debug)std::cout << "calculate match and score item "<< i << " size " << input.size() << std::endl;
        // Apply mask if it exists
        uint16_t masked_input = input[i];
        uint16_t masked_match = match->vector[i];
        if(debug)
            std::cout << "calculate match and score item before mask "<< i << " input " << masked_input << " match " << masked_match << std::endl;

        if (!match->mask.empty() && match->mask.size() == match->vector.size()) {
            // special case mymask 0 = 0xffff allow all bits
            uint16_t mymask = match->mask[i];
            // if (mymask == 0)
            //     mymask=  ~match->mask[i];

            masked_input &= mymask;
            masked_match &= mymask;
            if(debug)
                std::cout << "calculate match and score item after mask "<< i << " input " << masked_input << " match " << masked_match << std::endl;

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

        double weight = (!match->weight.empty() && match->weight.size() == match->vector.size())
                                ? static_cast<double>(match->weight[i])
                                : 100.0;
        total_weight += 100;
        if (within_tolerance) {
            // Add weighted score or default to 1 if weights are not defined
            match_score += weight;
        }
        else
        {
            if(debug) 
                std::cout << "calculate match and score done  #2 result " << result << std::endl;
            if(match->weight.empty())
                return result;
        }
    }
    result = (total_weight > 0.0) ? (match_score / total_weight) : 0.0;
    if(debug) 
        std::cout << "calculate match and score done #3 result " << result << std::endl;

    // Normalize match score by total weight
    return result;
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
const std::shared_ptr<MatchObject>find_best_match(int run, const std::vector<uint16_t>& input,
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
std::shared_ptr<MatchObject>create_new_match(int run, const std::vector<uint16_t>& data) {
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
    std::unique_ptr<FILE, std::function<void(FILE*)>> pipe(
        popen(command.c_str(), "r"), 
        [](FILE* f) { if (f) pclose(f); }
    );
    
 //   std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
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

// Determine if the query is for Modbus based on register type
bool is_modbus_query(const std::string& query_str) {
    try {
        auto query_json = json::parse(query_str);
        std::string reg_type = query_json.value("reg_type", "");
//        std::string sm_name = query_json.value("sm_name", "");
        return (reg_type == "hold" || reg_type == "bits" || reg_type == "input" || reg_type == "coil");
    } catch (...) {
        return false;
    }
}

// Run Query Command via ws or modbus
std::string run_query(const std::string& url, ModbusClient& mb_client, const std::string& query_str) {
     
    if (mb_client.is_connected() && is_modbus_query(query_str))
     //and  query string field reg_type is hold, bits, input or coil then run the mb_client
        return mb_client.run_mb(query_str); 
    else 
        return run_wscat(url, query_str);
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
    ModbusClient mb_client; // Example IP and port

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
    std::string mb_ctx;
    bool mb_ok = false;
    if (testPlan.contains("Modbus"))
    {
        mb_ctx = testPlan["Modbus"].get<std::string>();
        mb_ok = mb_client.connect(mb_ctx);
        if (mb_ok)
            log_msg << " Connected to Modbus " << mb_ctx << std::endl;
        else 
            log_msg << " Not connected to Modbus " << mb_ctx << std::endl;
    }
    else
    {
        log_msg << " No Modbus Connection detected " << std::endl;
    }

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


    int seq = jmq.contains("seq") ? jmq["seq"].get<int>():1;
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
    setup_matches(jnexpects, cnexpect, testPlan, "NotExpects", ne_idx, newhen, nedur, tim);
    std::cout << " read matches from " << mfile<<std::endl;  
    load_matches(mfile);

    // TODO config_matches

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


// load up the memory map 
// auto data_file = "src/data_definition.txt";
void parse_definitions(std::map<std::string, DataTable>& tables, const std::string& filename) {
    //std::map<std::string, DataTable> tables;
    std::ifstream file(filename);
    std::string line;
    std::regex table_pattern(R"(\[(.+?):(.+?):(\d+)\])");
    std::smatch match;
    std::string current_table;
    int current_offset = 0;
    int base_offset = 0;

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    while (std::getline(file, line)) {
        std::string trimmed = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1"); // trim and reduce spaces
        if (trimmed.empty() || trimmed[0] == '#') continue; // Skip comments and empty lines

        if (std::regex_search(trimmed, match, table_pattern)) {
            // System match[1].str()
            // Regtype match[2].str()
            // offset match[3].str()
            current_table = match[1].str() + ":" + match[2].str();
            base_offset = std::stoi(match[3].str());
            if(tables.find(current_table) == tables.end())
            {
                std::cout << "****creating a new table [" <<current_table<<"]  offset : " << current_offset << std::endl;
                tables[current_table] = DataTable(current_offset);
                tables[current_table].base_offset = base_offset;// = DataTable(current_offset);
            }
            continue;
        }
        // continue to parse a data item
        size_t colon_pos = trimmed.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string name = trimmed.substr(0, colon_pos);
        // the offset in the map is relative to the base offset
        int offset = std::stoi(trimmed.substr(colon_pos + 1));
        offset += base_offset;
        int size = 1; // default size
        size_t second_colon_pos = trimmed.find(':', colon_pos + 1);
        if (second_colon_pos != std::string::npos) {
            size = std::stoi(trimmed.substr(second_colon_pos + 1));
        }

        if (!tables[current_table].items.empty()) {
            int expected_offset = tables[current_table].base_offset + tables[current_table].calculated_size;
            std::cout <<" actual offset " << offset << " expected " << expected_offset << std::endl; 
            if (offset != expected_offset) {
                std::cout <<"Offset mismatch for " <<name << " in table " << current_table
                << " actual offset " << offset << " expected " << expected_offset 
                << " diff :" <<offset - expected_offset<< std::endl;
                tables[current_table].calculated_size += (offset - expected_offset);
                //throw std::runtime_error("Offset mismatch for " + name + " in table " + current_table);
            }
        }
        // System/Regtype in current_table
        DataItem item(name, offset, size);
        tables[current_table].items.push_back(item);
        tables[current_table].calculated_size += size;
    }

    return;
}

bool verify_offsets(const std::map<std::string, DataTable>& tables) {
    bool all_correct = true;
    for (const auto& table_pair : tables) {
        const auto& table = table_pair.second;
        int expected_offset = table.base_offset;
        
        for (const auto& item : table.items) {
            if (item.offset != expected_offset) {
                std::cerr << "Offset error in table " << table_pair.first << ": "
                          << "Item " << item.name << " should start at " << expected_offset
                          << " but starts at " << item.offset << std::endl;
                all_correct = false;
            }
            expected_offset += item.size;
        }
    }
    return all_correct;
}

// #include <vector>
// #include <string>
// #include <sstream>
void restartSystem() {
    std::cout << " restart string = ["<<restart_string<<"]";

    // This command reboots the system. It needs appropriate permissions.
    // int result = system("sleep 0.2 &&  &  ./ftest test_plan1 ffff&");
    // // Optionally, you can add a safe exit if needed.
    // exit(EXIT_SUCCESS); // Exit the program successfully after issuing the reboot command
}

// Function to split a string by a delimiter
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
// decode_shmdef Function to decode shmdef into rack_num, mem_id, reg_id, and offset
// sbmu:bits[:rack_num]:offset (name or integer)
bool decode_shmdef(MemId &mem_id, RegId &reg_id, int& rack_num, uint16_t &offset, const std::string &shmdef) {

    // Split the string by ':'
    std::vector<std::string> parts = split(shmdef, ':');
    if (parts.size() < 2) {
        return false; // Invalid format
    }

    std::string smname = parts[0];
    std::string reg_type = parts[1];
    std::string table_name = smname + ":" + reg_type;
    std::string rack_num_str;
    std::string offset_str;

    // sbmu:bits:offset

    if (parts.size() == 3) {
        offset_str = parts[2];
        rack_num = 0;  // use default as not defined
    }
    if (parts.size() >= 4) {
        rack_num_str = parts[2];
        offset_str = parts[3];
        rack_num = static_cast<uint16_t>(std::stoi(rack_num_str));
        std::cout << "Note rack_num  " << rack_num << std::endl; 
    }

    // Convert smname to mem_id
    if (smname == "none") {
        mem_id = MEM_NONE;
    } else if (smname == "sbmu") {
        mem_id = MEM_SBMU;
    } else if (smname == "rack") {
        mem_id = MEM_RACK;
    } else if (smname == "rtos") {
        mem_id = MEM_RTOS;
    } else {
        mem_id = MEM_UNKNOWN;
    }

    // Convert reg_type to reg_id
    if (reg_type == "none") {
        reg_id = REG_NONE;
    } else if (reg_type == "bits") {
        reg_id = REG_BITS;
    } else if (reg_type == "input") {
        reg_id = REG_INPUT;
    } else if (reg_type == "hold") {
        reg_id = REG_HOLD;
    } else if (reg_type == "coil") {
        reg_id = REG_COIL;
    } else if (reg_type == "sm16") {
        reg_id = REG_SM16;
    } else if (reg_type == "sm8") {
        reg_id = REG_SM8;
    } else {
        reg_id = REG_NONE;
    }

    // Extract offset
    offset = str_to_offset(offset_str);
    if (offset == 65535)
    {
        if(0)std::cout << "  Note offset value: [" << offset_str 
                        <<"] offset: ["<< offset << "] table_name :["<< table_name << "]" <<  std::endl;
        if(data_tables.find(table_name)!= data_tables.end())
        {
            if(0)std::cout <<" Found table ["<< table_name <<"]"<< std::endl;
            const auto& items = data_tables.at(table_name).items;
            for (const auto& item : items) {
                if (item.name == offset_str) {
                    if(0)std::cout << "Note offset item found : " << offset_str 
                            <<" offset: "<< item.offset << " table_name :"<< table_name <<  std::endl;

                    offset = item.offset;
                    return true;
                }
            }
        }
        else
        {
            if(1)std::cout << __func__ << " >>>>Error  offset item NOT found : " 
                            <<" offset_str: ["<< offset_str << "] table_name :"<< table_name <<  std::endl;
        }
    }
    return true;
}



// Function to encode shmdef and rack into a 32-bit ID
uint32_t encode_id(const std::string &shmdef, int rack) {
    MemId mem_id;
    RegId reg_id;
    uint16_t offset;
    int rack_num;

    if (!decode_shmdef(mem_id, reg_id, rack_num, offset, shmdef)) {
        printf(" invalid shmdef [%s]\n", shmdef.c_str());
        return 0; // Invalid shmdef
    }
    if(rack_num >=0)
    {
        // decode found it !!
        rack = rack_num;
    }
    // Encode the ID
    uint32_t id = 0;
    id |= (static_cast<uint32_t>(mem_id) & 0xF) << 28;  // 4 bits for mem_id
    id |= (static_cast<uint32_t>(reg_id) & 0xF) << 24;  // 4 bits for reg_id
    id |= (static_cast<uint32_t>(rack) & 0xFF) << 16;   // 8 bits for rack
    id |= (offset & 0xFFFF);                            // 16 bits for offset

    return id;
}


std::pair<int, std::vector<int>> get_modbus(uint32_t id, int num, std::vector<int>data)
{

    std::ostringstream oss;
    MemId mem_id = (MemId)((id & 0xF0000000) >> 28);
    RegId reg_id = (RegId)((id & 0x0F000000) >> 24);

    uint16_t rack_num = (uint16_t)((id & 0x00FF0000)>>16);
    int offset = (int)((id & 0x0FFFF));

    oss << " MemId " <<  mem_id
        << " RegId " <<  reg_id
        << " rack_num " << rack_num
        << " offset " << offset ;

    modbus_t * ctx = mbx_client.get_ctx();
    uint16_t rack_select[] = { static_cast<uint16_t>(rack_num) };
/*
    0x01	Read Coils	Reads 1-bit output coil values.
    0x02	Read Discrete Inputs	Reads 1-bit input status values.
    0x03	Read Holding Registers	Reads multiple 16-bit holding registers.
    0x04	Read Input Registers	Reads multiple 16-bit input registers.
    0x05	Write Single Coil	Writes a single 1-bit output coil.
    0x06	Write Single Register	Writes a single 16-bit register.
    0x0F (15)	Write Multiple Coils	Writes multiple 1-bit output coils.
    0x10 (16)	Write Multiple Registers	Writes multiple 16-bit holding registers.
    0x07	Read Exception Status	Reads 8-bit device status.
    0x08	Diagnostics	Special diagnostics function.
    0x11	Report Server ID	Reads device information.
    0x16	Mask Write Register	Mask-based write to a single holding register.
    0x17	Read/Write Multiple Registers	Reads & writes multiple registers in one request.
*/

    // if(offset >= 1000 && reg_id == REG_HOLD)
    // {
    //     auto ret = modbus_write_registers(ctx, 500, 1, rack_select);  // Assuming register 500 is used to select the rack
    // }
    std::vector<uint16_t> response(num);
    std::vector<uint8_t> response_bits(num);
    int res = -1;
    if(!ctx)
    {
        printf(" no modbus ctx; try reconnect\n");
        ctx = mbx_client.reconnect();
        if(!ctx)
        {
            printf(" reconnect failed \n");
            oss << " no modbus ctx; reconnect failed";
            return {-1, {}};
        }
    }
    if (reg_id ==  REG_INPUT) {
        res = modbus_read_input_registers(ctx, offset, num, response.data());
    } else if (reg_id == REG_HOLD) {
        res = modbus_read_registers(ctx, offset, num, response.data());
    } else if (reg_id == REG_BITS) {
        oss << " modbus read input bits offset ["<<offset<<"] num ["<<num<<"]\n"; 
        res = modbus_read_input_bits(ctx, offset, num, response_bits.data());
    }

    if (res == -1) {
        int err = errno;
        std::cerr << " Failed to read: errno "<< err << " -> " << modbus_strerror(err) << std::endl;
        oss << " Failed to read: " << modbus_strerror(err) << std::endl;
        std::cout << oss.str();
        if((err ==  22) || (err == 104)|| (err == 32))
            ctx = mbx_client.reconnect();
        return {-1, {}};
    } 
    else 
    {
        if (reg_id == REG_BITS) {
            std::vector<int> int_response(response_bits.begin(), response_bits.end());
            std::cout << oss.str();
            return {0, int_response};
        }
        else
        {
            std::vector<int> int_response(response.begin(), response.end());
            std::cout << oss.str();
            return {0, int_response};
        }
    }
    std::cout << oss.str();
    return {-1, {}};
} // Run the query

std::pair<int, std::vector<int>> set_modbus(uint32_t id, int num, std::vector<int>data)
{

    std::ostringstream oss;
    MemId mem_id = (MemId)((id & 0xF0000000) >> 28);
    RegId reg_id = (RegId)((id & 0x0F000000) >> 24);

    uint16_t rack_num = (uint16_t)((id & 0x00FF0000)>>16);
    int offset = (int)((id & 0x0FFFF));

    oss << " MemId " <<  mem_id
        << " RegId " <<  reg_id
        << " rack_num " << rack_num
        << " offset " << offset ;

    modbus_t * ctx = mbx_client.get_ctx();
    uint16_t rack_select[] = { static_cast<uint16_t>(rack_num) };

    // if(offset >= 1000 && reg_id == REG_HOLD) 
    // {
    //     auto ret = modbus_write_registers(ctx, 500, 1, rack_select);  // Assuming register 500 is used to select the rack
    // }

    std::vector<uint16_t> response(num);
    std::vector<uint8_t> response_bits(num);
    int res = -1;

    int idx = 0;
    
    //if (reg_type == "hold") {
    if (reg_id == REG_HOLD) {
         int num = data.size();
         std::vector<uint16_t> vals;
          int idx = 0;
         for ( auto& val : data)
         {
            std::cout << " idx " << idx << " val " << val << std::endl;
            idx++;
             vals.push_back((uint16_t)val);
         }
         if(num>1)
         {
            res = modbus_write_registers(ctx, offset, num, vals.data());
         }
         else
         {
            res = modbus_write_register(ctx, offset, vals[0]);
         }

    } else if (reg_id == REG_COIL) {
        for ( auto& val : data)
        {
            std::cout << " sending val "<< (uint16_t) val << std::endl;
            if (reg_id == REG_HOLD) {
                res = modbus_write_register(ctx, offset + idx, (uint16_t)val);
            //} else if (reg_type == "coil") {
            } else if (reg_id == REG_COIL) {
                res = modbus_write_bit(ctx, offset+ idx, (uint8_t)val);  // Assuming single bit write for coils
            }
            idx++;
        }
    }

    if (res == -1) {
        std::cerr << "Failed to write: " << modbus_strerror(errno) << std::endl;
        return {-1, data};
    } else {
        std::cout << "write result: " << modbus_strerror(errno) << std::endl;
        return {0, data};
    }
    return {-1, data};
}

bool find_field(json & js, const std::string& key , const std::string& obj )
{
    try {
        json j = json::parse(obj);
        if (j.contains(key))
        {
            js = j[key];
            return true;
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
    }
    return false;
}

std::string extractDataField(const std::string& response) {
    std::regex pattern(R"(\s*"data": \[([0-9,\s]+)\])");  // Regex to find "data": [numbers,...]
    std::smatch matches;

    if (std::regex_search(response, matches, pattern)) {
        if (matches.size() > 1) {
            return matches[1].str();  // Return the contents of the brackets (the numbers)
        }
    }
    return "Data not found";  // Return a default or error message if no match is found
}

uint32_t  show_id(const json &item, const std::string&def, std::ostringstream& oss )
{
    uint32_t id = 0; 
    if (item.contains(def)) {
        oss << def << ": "<< item[def];
        id = encode_id(item[def], 0); 
        oss << "  0x" << std::hex << std::uppercase << id << std::dec<<"  ";
    }
    return id;
}


std::vector<ModbusTransItem> process_json_data(const json& jdata) {
    // std::ifstream file(filename);
    // if (!file.is_open()) {
    //     throw std::runtime_error("Failed to open file.");
    // }

    // json jdata = json::parse(file);

    std::vector<ModbusTransItem> items;
    std::ostringstream oss;

    for (const auto& item : jdata) {
        ModbusTransItem transItem;
        transItem.src_str = item.value("src", "");
        transItem.src_id = encode_id(transItem.src_str, 0);

        transItem.dest_str = item.value("dest", "");
        transItem.dest_id = encode_id(transItem.dest_str, 0);

        transItem.lim_str = item.value("lim", "");
        transItem.lim_id = encode_id(transItem.lim_str, 0);

        transItem.meth_str = item.value("meth", "");
        transItem.meth_id = meth_str_to_id(transItem.meth_str,"no desc");

        transItem.name = item.value("desc", "");  // Using "desc" as it seems to be the descriptive field

        items.push_back(transItem);
    }

    return items;
}

//std::map<std::string, TestTable> test_tables;

std::string handle_query(const std::string& qustr, std::map<std::string, DataTable>& data_tables, int sock);

void tests_run(const std::string& tname, std::ostringstream& oss)
{
    if(test_tables.find(tname) == test_tables.end())
    {
        oss << " no test called [" << tname<<"] found" << std::endl;
        return;
        //"action"
    }
    bool isJson = false;
    
    auto test_item = test_tables[tname];
    oss << " test called [" << tname<<"] found" << std::endl;
    int passes = 0;
    int fails = 0;

    for ( auto& item : test_item.items)
    {

        if (item.action.find("pause") == 0)
        {
            std::istringstream iss(item.action);
            std::vector<std::string> parts;
            std::string part;
            while (std::getline(iss, part, ':')) {
                parts.push_back(part);
            }
            int pause = 1000;
            if(parts.size() > 1)
            {
                pause = std::stoi(parts[1].c_str());
            }
            // Pause execution for the specified duration
            std::this_thread::sleep_for(std::chrono::milliseconds(pause));
        }
        else
        {
            auto resp = handle_query(item.action, data_tables, 0);
            std::cout << "got resp [" << resp <<"]\n";
            std::cout << "parsing resp \n";
            isJson = false;
            json j;
            try {
                j = json::parse(resp);
                isJson = true;
            } catch (json::parse_error& e) {
                std::cout << "resp is not json \n";
            }

            //std::cout << "parsed resp [" << resp <<"]\n";
            // Extract the 'data' array from the JSON object
            if (isJson)
            {
                std::ostringstream toss;

                std::vector<int> respvec = j["data"];
                if(item.data.size() > 0 )
                {
                    int pass = 0;
                    int fail = 0;
                    if(item.data == respvec)
                    {
                        toss << " [PASS] ";
                        toss << "[" << item.action << "] ";
                        toss << " result: " << json(respvec).dump() << " ";
                        pass++;
                    }
                    else
                    {
                        toss << " [FAIL] ";
                        toss << "[" << item.action<<"] ";
                        toss << " result: " << json(respvec).dump() << " ";
                        toss << " expected: " << json(item.data).dump() << " ";
                        fail++;
                    }
                    parse_test_result(toss, tname, pass, fail, item.cat );
                    passes+=pass;
                    fails+=fail;

                }
                else 
                {
                    //oss << " [    ] ";
                }
                //oss << item.desc;
                oss << toss.str() << std::endl;
            }
            else
            {
                oss << resp;
            }
            //oss<< item.desc << " resp " << resp << endl;
            //oss << resp;
        }
    }
    if(isJson)
    {
        if ( fails > 0)
        {
            oss << " Test Results pass " << passes <<" fail " << fails<< std::endl ;
        }
        else
        {
            oss << " Test Results all tests passed " << std::endl ;
        }        
    }

}

std::string unescapeJSON(const std::string& input) {
    std::string output;
    output.reserve(input.size()); // Reserve original size to avoid reallocations

    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length() && input[i + 1] == '"') {
            // Skip the backslash if it escapes a double quote
            continue;
        }
        output += input[i];
    }

    return output;
}

std::string jsonTypeToString(const json::value_t type) {
    switch (type) {
        case json::value_t::null:
            return "null";
        case json::value_t::object:
            return "object";
        case json::value_t::array:
            return "array";
        case json::value_t::string:
            return "string";
        case json::value_t::boolean:
            return "boolean";
        case json::value_t::number_integer:
            return "number_integer";
        case json::value_t::number_unsigned:
            return "number_unsigned";
        case json::value_t::number_float:
            return "number_float";
        case json::value_t::binary:
            return "binary";
        case json::value_t::discarded:
            return "discarded";
        default:
            return "unknown";
    }
}

void printJsonKeys(const json& jmsg, const std::string& key, std::ostringstream& oss) {
    if (jmsg.contains(key)) {
        oss << "Found key '" << key << "'." << std::endl;
        const json& j = jmsg[key];

        // If the value is a string that needs to be unescaped or re-parsed:
        if (j.is_string()) {
            std::string escdata = unescapeJSON(j.get<std::string>());
            try {
                json parsed = json::parse(escdata);
                if (parsed.is_object()) {
                    for (auto it = parsed.begin(); it != parsed.end(); ++it) {
                        if ( it.key() == "RBMS")
                        {
                            oss << " Get keys from : " << it.key() << std::endl;

                            printJsonKeys(parsed, "RBMS", oss);
                        }
                    } 
                } else if (parsed.is_array()) {
                        oss << "Array items under key '" << key << "':" << std::endl;
                        for (const auto& jitem : parsed) {
                        if (jitem.is_object()) {
                            for (auto it = jitem.begin(); it != jitem.end(); ++it) {
                                oss << "  - " << it.key() << ": " << it.value() << std::endl;
                            }
                        } else {
                            // If the array elements are not objects, just output their value
                            oss << "  - " << jitem << std::endl;
                        }
                    }
                } else {
                    oss << "The parsed string is not an object but a " << jsonTypeToString(parsed.type()) << std::endl;
                }
            } catch (const json::parse_error& e) {
                oss << "Error parsing string: " << e.what() << std::endl;
            }
        } else if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                oss << " Key: " << it.key() << std::endl;
            }
        } else if (j.is_array()) {
            oss << "Array items under key '" << key << "':" << std::endl;
            for (const auto& jitem : j) {
                if (jitem.is_object()) {
                    for (auto it = jitem.begin(); it != jitem.end(); ++it) {
                        oss << "  - " << it.key() << ": " << it.value() << std::endl;
                    }
                } else {
                    // If the array elements are not objects, just output their value
                    oss << "  - " << jitem << std::endl;
                }
            }
        } else {
            oss << "Value at key '" << key << "' is not an object but a " << jsonTypeToString(j.type()) << std::endl;
        }
    } else {
        oss << "Key '" << key << "' not found in the JSON object." << std::endl;
    }
}


std::string processWebSocketMessage(const json& message, std::ostringstream& oss) {
    try {
        if (message.contains("data")) {
            std::string rawdata = message["data"];
            std::string escdata = unescapeJSON(rawdata);
            auto data = json::parse(escdata);

            if (data.contains("Configuration")) {
                oss << "Configuration Data Found:\n" << data["Configuration"].dump(4) << std::endl;
            }
            if (data.contains("RBMS")) {
                oss << "RBMS Data Found:\n";
                for (const auto& rack : data["RBMS"]) {
                    oss << "Rack ID: " << rack["RackID"] << ", Total Voltage: " << rack["TotalVoltage"] << " V\n";
                }
            }
        }
    } catch (const json::exception& e) {
        oss << "Error processing message: " << e.what() << std::endl;
    }
    return oss.str();
}


std::string decode_ws(std::string logFileName, std::ostringstream& oss) 
{

    // std::ostringstream oss;
    // std::string logFileName = "config/json/ws_example.json"; // Adjust the file name/path as necessary
    std::ifstream logFile(logFileName);

    if (!logFile.is_open()) {
        //std::cerr 
        oss << "Failed to open log file." << std::endl;
        return oss.str();
    }

    try {
        json logEntries = json::parse(logFile);
        oss << "  parsed logfile." << std::endl;
        int entries  = 0;
        // Iterate through the array of WebSocket messages
        if (logEntries.contains("log"))
        {
            oss << "found a log." << std::endl;
            auto jlog = logEntries["log"];
            if (jlog.contains("entries"))
            {
                oss << "found entries." << std::endl;
                auto jentries = jlog["entries"];
                for (auto& jentry : jentries)
                {
                    if (jentry.contains("_webSocketMessages"))
                    {
                        oss << "found messages" << std::endl;
                        auto jmsgs = jentry["_webSocketMessages"];
                        for (auto& jmsg : jmsgs)
                        {
                            printJsonKeys(jmsg, "data", oss);
                            //processWebSocketMessage(jmsg, oss);
                            entries++;
                        }
                    }
                }
            }
        }
        // for (const auto& log : logEntries["log"]) {
        //     for (const auto& entry : log["entries"]) {
        //         for (const auto& wsm : entry["_webSocketMessages"]) {
        //             processWebSocketMessage(wsm, oss);
        //             entries ++;
        //         }
        //     }
        // }
        oss << "  processed ["<<entries<<"] logfile entries." << std::endl;

    } catch (const json::exception& e) {
        //std::cerr 
        oss << "Failed to parse log file: " << e.what() << std::endl;
        //return 2;
    }

    return oss.str();
}

std::map<std::string, TestResult> tests;

// Function to print all categories breadth-first to an ostringstream
//void printCats(std::map<std::string, TestResult>& tests, std::ostringstream& oss) {
void printCats(std::ostringstream& oss) {
    std::queue<std::pair<std::string, std::map<std::string, TestResult>*>> queue;

    // Enqueue all top-level categories
    for (auto& test : tests) {
        queue.push({test.first, &test.second.subtests});
    }

    // Breadth-first traversal
    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        std::string categoryName = current.first;
        std::map<std::string, TestResult>* subtests = current.second;

        oss << categoryName << " (Pass: " << tests[categoryName].pass_count
            << ", Fail: " << tests[categoryName].fail_count << ")\n";

        for (auto& subtest : *subtests) {
            queue.push({categoryName + "." + subtest.first, &subtest.second.subtests});
        }
    }
}

// Recursive helper function
void showCatsRecursive(const std::map<std::string, TestResult>& tests, std::ostringstream& oss, const std::string& prefix = "") {
    for (const auto& test : tests) {
        // Construct the category name with the prefix
        std::string categoryName = prefix.empty() ? test.first : prefix + "." + test.first;

        // Append the test results to the output stream
        oss << categoryName << " (Pass: " << test.second.pass_count
            << ", Fail: " << test.second.fail_count << ")\n";

        // Recursively call the function to handle subtests
        if (!test.second.subtests.empty()) {
            showCatsRecursive(test.second.subtests, oss, categoryName);
        }
    }
}


// Recursive function to update the test result counts
void updateCounts(std::map<std::string, TestResult>& tests, std::string& line, const std::string& title, const std::vector<std::string>& categories, int index, bool isPass) {
    if (index >= categories.size()) return;
    TestResult &result = tests[categories[index]];
    if (isPass) {
        result.pass_count++;
    } else {
        result.fail_count++;
    }
    // Save input at the deepest level of the category
    if (index == categories.size() - 1) {
        result.title = title; // Store the input string here
        result.test_lines.push_back(line); // Store each line here
        //result.details = input; // Store the input string here
    }

    updateCounts(result.subtests, line, title, categories, index + 1, isPass);
}


void parse_test_result( std::ostringstream& toss, const std::string& title, int pass, int fail, const std::string& category )
{
    std::vector<std::string> categories = split(category, '.');

    std::istringstream iss(toss.str());
    std::string line;
    while (std::getline(iss, line)) {
        bool isPass = line.find("[PASS]") != std::string::npos;
        bool isFail = line.find("[FAIL]") != std::string::npos;
        if (isPass || isFail) {
            // Example: tests.inputs.rack_online
            updateCounts(tests, line, title, categories, 0, isPass);
        }
    }
}

// Helper function to parse numbers from strings, handling both decimal and hex
int parse_number(const std::string& num_str) {
    std::istringstream iss(num_str);
    int num = 0;
    if (num_str.find("0x") == 0) { // Check for hex format
        iss >> std::hex >> num;
    } else {
        iss >> num;
    }
    return num;
}

// Function to parse the "repeat" string and populate offsets
void parse_repeat(const std::string& repeat_str, std::vector<int>& offsets) {
    int base = 0, increment = 1, start_offset = 0;
    std::vector<std::string> parts;
    std::stringstream ss(repeat_str);
    std::string item;

    while (std::getline(ss, item, ':')) {
        parts.push_back(item);
    }

    if (!parts.empty()) {
        base = parse_number(parts[0]);
    }
    if (parts.size() > 1) {
        increment = parse_number(parts[1]);
    }
    if (parts.size() > 2) {
        start_offset = parse_number(parts[2]);
    }

    for (int i = 0; i < base; ++i) {
        offsets.push_back(start_offset + i * increment);
    }
}

// Updated function to accept a vector by reference and populate it
void parse_repeat(std::vector<int>& offsets, const json& jitem ) {
    if (jitem.contains("repeat") && !jitem["repeat"].is_null()) {
        if(jitem["repeat"].is_string())
        {
            std::string repeat_str = jitem["repeat"].get<std::string>();
            parse_repeat(repeat_str, offsets);
        }
        else if(jitem["repeat"].is_number())
        {
            int count = jitem["repeat"].get<int>();
            for ( int i = 0 ; i < count; i++ )
            offsets.push_back(i); // Populate with 0 if no repeat field or null
        }
    } else {
        offsets.push_back(0); // Populate with 0 if no repeat field or null
    }
}

void resp_to_2dvec(std::vector<std::vector<int>>&data2D, std::stringstream& ossr)
{
    
    // Parsing JSON data
    json j = json::parse(ossr.str());
    
    // Extracting each "data" array and adding it to the 2D vector
    for (const auto& item : j) {
        std::vector<int> dataVec = item["data"].get<std::vector<int>>();
        data2D.push_back(dataVec);
    }
}

void resp_to_1dvec(std::vector<int>&data1D, const std::string& resp)
{
    
    // Parsing JSON data
    json j = json::parse(resp);
    
    data1D = j["data"].get<std::vector<int>>();
}


std::string handle_query(const std::string& qustr, std::map<std::string, DataTable>& data_tables, int sock) {
    std::string query = trim(qustr);
    std::istringstream iss(query);
    std::vector<std::string> parts;
    std::string part;
    std::string use_str;

    while (std::getline(iss, part, ':')) {
        parts.push_back(part);
    }

    //std::cout << " Query ["<<query<<"] parts size "<<parts.size() << std::endl;
    if (parts.size() > 0)
    {
        auto swords = StringWords(query);
        if ((swords[0] == "using") && (swords.size() >2))
        {
            use_str = swords[1];
            // Erase the first two elements to make swords[0] become what was originally swords[2]
            swords.erase(swords.begin(), swords.begin() + 2);
        }

        //std::cout << " swords size " << swords.size() << std::endl;
        //std::cout << " parts[0] " << parts[0] << std::endl;
        if ( swords[0] == "help")
        {
            std::ostringstream oss;
            oss <<"help :-\n";
            oss <<"      show - show stuff\n";
            oss <<"      json - show stuff as json\n";
            oss <<"      tables - show tables\n";
            oss <<"      find - items by offset or name match\n"; 
            oss <<"      get  - websocket: get a value, define the dest by name or table:offset\n";
            oss <<"      using myvar get  - websocket: get a value, define the dest by name or table:offset  save result in myvar\n";
            oss <<"      set  - websocket: set a value, define the dest by name or table:offset\n";
            oss <<"      getmb  - sbmu modbus: get a value, define the dest by name or table:offset\n";
            oss <<"      setmb  - modbus set a value, (all also) define the dest by name or table:offset\n";
            oss <<"      tdef [file]  - test transfer defs\n";
            oss <<"      tparse [file]  - test table parsen";
            oss <<"      test [file]  - more stuff\n";
            oss <<"      tops [desc, table]  - run a test op (load defs with tdef  first) \n";
            oss <<"      run  [test]         - run a test from test_tables \n";
            oss <<"      ws_log  [log_name]         - decode a ws_log \n";
            oss <<"      cats  [clear]        - show teet results (cats)\n";
            oss <<"      setvar [test][show][name <value>]]  - sets up a named value";
            
            return oss.str();
        }
        // else if ( swords[0] == "res")
        // {
        //     close(sock);
        //     std::ostringstream oss;
        //     // Use system to execute the command
        //     int result = system(restart_string.c_str());
        //     if (result) {
        //         std::cerr << "Failed to execute command." << std::endl;
        //     }
        //     oss << " restarting \n";
        // }
        else if ( swords[0] == "ws_log")
        {
            std::string decode_ws(std::string logFileName, std::ostringstream& oss) ;
            // decode_ws
            std::string logName = "config/json/ws_example.json"; // Adjust the file name/path as necessary
            if (swords.size() > 1)
                logName = swords[1];

            std::ostringstream oss;
            oss << " decoding logfile";
            decode_ws(logName, oss);
            //oss<< " tests [" <<tname<<"] complete\n";
            return oss.str();
        }
        else if ( swords[0] == "cats")
        {
            std::ostringstream oss;
            printCats(oss);
            //showCatsRecursive(tests, oss);
            return oss.str();

        }
        else if ( swords[0] == "scats")
        {
            std::ostringstream oss;
            //printCats(oss);
            showCatsRecursive(tests, oss);
            return oss.str();

        }
        else if ( swords[0] == "run")
        {
            std::string tname = "first";
            if (swords.size() > 1)
                tname = swords[1];

            std::ostringstream oss;
            tests_run(tname, oss);
            oss<< " tests [" <<tname<<"] complete\n";


            return oss.str();
        }
        else if ( swords[0] == "tparse")
        {
            
            std::ostringstream oss;
            tparse(oss);
            oss<< " file parsed\n";
            return oss.str();
        }
        else if ( swords[0] == "tdef")
        {
            std::string filename = "json/test_tdefs.json";
            if (swords.size() < 1)
                filename = swords[1];

            std::ostringstream oss;
            
            //std::string filename = "json/test_tdefs.json";
            ops_parse(filename, oss);
            // test_read_array(jdata, filename, oss);
            // // Iterate through the JSON array
            // for (const auto& item : jdata) {
            //     // Check if 'src' key exists to avoid potential exceptions
            //     show_id(item, "src",oss );
            //     show_id(item, "dest",oss );
            //     show_id(item, "lim",oss );
            //     show_id(item, "meth",oss );
            //     show_id(item, "descr",oss );
            //     oss << std::endl;

            // }
            // auto items = process_json_data(jdata);
            // test_sim_mem();

            return oss.str();


        }
        else if ( swords[0] == "tops")
        {
            std::ostringstream oss;
            std::string op_table ="rack_input_complete";
            std::string op_desc = "terminal_undervoltage";

            if (swords.size() >1)
                op_desc = swords[1];
            if (swords.size() >2)
                op_table = swords[2];
            test_ops_run(test_ops_tables , op_table, op_desc, oss);

            return oss.str();
        }
        else if ( swords[0] == "test")
        {
            std::ostringstream oss;
            //std::vector<std::pair<std::string,std::string>> tests;
            // struct QueryTest {
            //     std::string query;
            //     std::string desc;
            //     std::string resp;
            // };
            std::vector<QueryTest> tests;
            // set / get hold
            tests.push_back({"setmb sbmu:hold:501 251 252 253 254", " setmb sbmu:hold:501 to 2", "{\"data\":[251,252,253,254]}"    });
            tests.push_back({"getmb sbmu:hold:501 4",               " getmb sbmu:hold:501 4", "{\"data\":[251,252,253,254]}"    });
            // set get rack 0 direct
            tests.push_back({"setmb sbmu:hold:2052 1251 1252 1253 1254", " setmb sbmu:hold:2052", "{\"data\":[1251,1252,1253,1254]}"    });
            tests.push_back({"getmb sbmu:hold:2052 4", " getmb sbmu:hold:2052", "{\"data\":[1251,1252,1253,1254]}"    });
            // set get rack 1 direct
            tests.push_back({"setmb sbmu:hold:3052 2251 2252 2253 2254", " setmb sbmu:hold:3052", "{\"data\":[2251,2252,2253,2254]}"    });
            tests.push_back({"getmb sbmu:hold:3052 4", " getmb sbmu:hold:3052", "{\"data\":[2251,2252,2253,2254]}"    });
            // set get rack 12 direct
            tests.push_back({"setmb sbmu:hold:13052 12251 12252 12253 12254", " setmb sbmu:hold:13052", "{\"data\":[12251,12252,12253,12254]}"    });
            tests.push_back({"getmb sbmu:hold:13052 4", " getmb sbmu:hold:13052", "{\"data\":[12251,12252,12253,12254]}"    });
            //check rack 0 access 
            tests.push_back({"setmb sbmu:hold:500 0 ", " setmb sbmu:hold:500 to 0", "{\"data\":[0]}"    });
            tests.push_back({"getmb sbmu:hold:500 1 ", " getmb sbmu:hold:500 1", "{\"data\":[0]}"    });
            tests.push_back({"getmb sbmu:hold:1052 4", " getmb sbmu:hold:1052", "{\"data\":[1251,1252,1253,1254]}"    });

            //check rack 1 access gets 
            tests.push_back({"setmb sbmu:hold:500 1 ", " setmb sbmu:hold:500 to 0", "{\"data\":[1]}"    });
            tests.push_back({"getmb sbmu:hold:500 1 ", " getmb sbmu:hold:500 1", "{\"data\":[1]}"    });
            tests.push_back({"getmb sbmu:hold:1052 4", " getmb sbmu:hold:1052", "{\"data\":[2251,2252,2253,2254]}"    });

            tests.push_back({"setmb sbmu:hold:1052 2251 2252 2253 2254", " setmb sbmu:hold:1052", "{\"data\":[2251,2252,2253,2254]}"    });
            tests.push_back({"getmb sbmu:hold:1052 4", " getmb sbmu:hold:1052", "{\"data\":[2251,2252,2253,2254]}"    });

            // set rack 2 but read 1052
            tests.push_back({"setmb sbmu:hold:3052 3251 3252 3253 3254", " setmb sbmu:hold:1052", "{\"data\":[3251,3252,3253,3254]}"    });
            tests.push_back({"getmb sbmu:hold:1052 4", " getmb sbmu:hold:1052", "{\"data\":[3251,3252,3253,3254]}"    });

            tests.push_back({"setmb sbmu:hold:510 48233", " setmb sbmu:hold:510 to 48223", "{\"data\":[48233]}"    });
            tests.push_back({"getmb sbmu:hold:510 1", " getmb sbmu:hold:510 ", "{\"data\":[48233]}"    });
            // push an event set rack to 5
            tests.push_back({"setmb sbmu:hold:500 5 ", " setmb sbmu:hold:500 to 5", "{\"data\":[5]}"    });
            tests.push_back({"setmb sbmu:hold:1002 34233", " setmb sbmu:hold:1002 to 34233", "{\"data\":[34233]}"    });
            // now rack 5 1002 should be 34233 as should 6002 after the event is serviced by the rack

            // tests.push_back({"setmb sbmu:hold:500 2", " setmb sbmu:hold:500 to 2", "{\"data\":[2]}"    });
            // tests.push_back({"getmb sbmu:hold:500 5", " getmb sbmu:hold:500 5",    "{\"data\":[2, 5 , 6, 7, 0]}"    });//"data":[2, 5, 6, 7, 0]
            // tests.push_back({"getmb sbmu:hold:1001 5", " getmb sbmu:hold:1001 5",  "{\"data\":[0,0,0,0,0]}"    });
            // tests.push_back({"get sbmu:bits:1 10",              " get 10 starting at sbmu:bits:1",          "1,0,0,0,0,0,0,0,0,0"});
            // tests.push_back({"set sbmu:bits:8 1 1",             " set sbmu:bits:8,9 to 1",                  "1,1"});
            // tests.push_back({"get sbmu:bits:1 10",              " get 10 sbmu:bits:1 ",                     "1,0,0,0,0,0,0,1,1,0"});
            // tests.push_back({"set sbmu:hold:500 8 ",            " set sbmu:hold:500 to 8",                  "8"});
            // tests.push_back({"get sbmu:hold:500 ",              " get sbmu:hold:500",                       "8"});
            // tests.push_back({"set sbmu:hold:1001 108 208 308 18 ", " set sbmu:hold:1001 to 108 208 308 18", "108,208,308,18"});
            // tests.push_back({"get sbmu:hold:1001 4 ",           "get 4 sbmu:hold:1001  should query rack 8", "108,208,308,18"});
            // tests.push_back({"set sbmu:hold:500 2 ",            "set sbmu:hold:500 to 2",                    "2"});
            // tests.push_back({"get sbmu:hold:500 ",              " get sbmu:hold:500",                        "2"});
            // tests.push_back({"set sbmu:hold:1001 102 202 302 12 ", " set sbmu:hold:1001 to 102 202 302 12", "102,202,302,12"});
            // tests.push_back({"get sbmu:hold:1001 4 ",          " get 4 sbmu:hold:1001  should query rack 2", "102,202,302,12"});
            // tests.push_back({"get sbmu:hold:at_total_v_over 10", " get 10 starting at_total_v_over",          "102,202,302,12,0,0,0,0,0,0"});
            
            for (auto & test : tests)
            {
               //oss << " running test[" << test.query << "] desc ["<< test.desc <<"]";
            // std::ostringstream oss;
            // oss << " Tet 
                json jresp;
                json jtest;
                auto resp = handle_query(test.query, data_tables, sock);
                bool pass = false;
                if(find_field(jresp, "data", resp))
                {
                    if(find_field(jtest, "data", test.resp))
                    {
                        if(jtest == jresp)
                        {
                            pass = true;
                        }
                    }
                }
 
                // //std::string extractDataField(resp)
                if (pass)
                    oss << "[PASS]";
                else
                    oss << "[FAIL]";
                oss << " test[" << test.query << "] desc ["<< test.desc <<"] jresp ["<< jresp.dump() << "]";
                //oss << std::endl;
                
                //std::cout <<"["<< test.resp <<"]" << std::endl;
                //std::cout <<"["<< resp <<"]" << std::endl;
                //oss <<   resp;
                oss << std::endl;

            }

            // oss <<"help :-\n";
            // oss <<"      show - show stuff\n";
            // oss <<"      json - show stuff as json\n";
            // oss <<"      tables - show tables\n";
            // oss <<"      find - items by offset or name match\n"; 
            // oss <<"      get(todo)  - get a value , define the dest by name or table:offset\n";
            // oss <<"      set(todo)  - set a value , define the dest by name or table:offset\n";
            return oss.str();
        }
        else if ( swords[0] == "setvar")
        {
            std::ostringstream oss;
            oss << " swords.size :" << swords.size() << std::endl;
            if (swords.size() < 2)
            {
                oss
                    << "setvar  set a variable as a value of some kind"
                    << std::endl
                    << " examples \n" 
                        <<    "setvar namex 42\n" 
                        << "setvar name2 1 2 3 4\n"
                        << "setvar name3 1 2 3 4,5 6 7 8\n"
                        << "setvar name4 'some string'\n"
                        << "setvar name5 1 2 3 4,5 6 7 8;21 22 23 24,25 26 27 28;1 2 3 4,4 3 2 1\n"
                        << "setvar test \n"
                        << "setvar show \n"
                    << std::endl;
            }
            else //if (swords.size() >= 2)
            {
                if (swords[1] == "test")
                {
                    test_parse_setvar(oss);
                }
                else if (swords[1] == "show")
                {
                    showvars(oss);
                }
                else
                {
                    parseAndSetVar(query);
                    oss << " var parsed" << std::endl;
                }
            }
            return oss.str();
        }
        else if ( swords[0] == "set")
        {
            // set sprcial for testing use rack 20 forall racks
            std::ostringstream oss;
            if (swords.size() < 2)
            {
                oss
                    << "set a value <table>:<name|offset>[:rack] <value>"
                    << std::endl
                    << " examples \n" 
                    << "    set sbmu:bits::1 0         set bits:1 to 0 rack is not needed and defaults to 0\n" 
                    << "    set sbmu:bits:1 0         set bits:1 to 0 rack is not needed and defaults to 0\n" 
                    << "    set sbmu:bits::summary_total_undervoltage  1\n"
                    << "    set sbmu:input:7:State  123    sets rack 7 state to 123\n"
                    << "    set rtos:hold:20:max_volts  3456    sets all racks  max_volts to 3456\n"
                    << std::endl;
            }
            else if ((swords.size() > 2) || (swords.size() > 1 && !use_str.empty()))
            {
                // oss
                //     << "set a value  <"<<swords[1]
                //     << std::endl;
                int rack=0;// = std::stoi(swords[2]);
                // try {
                //     rack = std::stoi(swords[2]);
                // } catch (const std::exception& ex) {
                //     oss <<  " rack must be a number -1 or rack num\n";
                //     return oss.str();
                // }
                uint32_t id;
                id = encode_id(swords[1], rack);
                // oss
                // id = 0x33nnaaaa
                int rack_num = (id & 0x00ff0000) >> 16;
                //printf("id 0x%x extracted rack_num %d \n", id, rack_num);
                if(rack_num == 20)
                {
                    printf("running on all racks \n");
                }
                //     << "id  <0x"<<std::hex << id << std::dec << "> " << std::endl;
                std::vector<int>values;

                if(use_str.empty())
                {                
                    try {
                        for (int i = 2; i < swords.size(); i++)
                        {
                            values.push_back(std::stoi(swords[i]));
                        }
                    }  catch (const std::exception& ex) {
                            oss <<  " values must be numbers\n";
                            return oss.str();
                    }
                }
                else
                {
                    // Check if use_str is a valid key and the type is correct
                    auto it = var_tables.find(use_str);
                    if (it != var_tables.end()) {
                        try {
                        // Attempt to use std::any_cast to retrieve the vector<int>
                            values = std::any_cast<std::vector<int>>(it->second);
                        } catch (const std::bad_any_cast& e) {
                            oss << "Type mismatch for key '" << use_str << "': Expected std::vector<int>\n";
                            std::cerr << oss.str();
                            return oss.str();
                        }
                    } else {
                        oss << "No such name found : " << use_str << "\n";
                        return  oss.str();
                    }
                }

                if(rack_num == 20)
                {
                    for ( auto i = 0 ; i < 20 ; i++)
                    {
                        setRackNum(id,i);
                        std::string qstring  = generateIdQuery("set", 202+i, id, "any", (int)values.size(), values);
                        std::string response = run_wscat(g_url, qstring);  // Run the query
                        oss  << response;
                    }
                }
                else
                {
                std::string qstring  = generateIdQuery("set", 202, id, "any", (int)values.size(), values);
                std::string response = run_wscat(g_url, qstring);  // Run the query
                oss  << response;
                    // << " url  ["<<g_url<<"] \n"
                    // << " query  ["<<qstring<<"] \n"
                    // << " response  ["<<response<<"] \n"
                    // << std::endl;
                }
            }

            return oss.str();
        }
        else if ( swords[0] == "setall")
        {
            uint32_t id = 0;
            std::vector<int>values = {0};
            std::ostringstream oss;
            if (swords.size() == 1)
            {
                oss
                    << "setall set a value <table><name|offset using the modbus connectipn"
                    << std::endl
                    << " examples \n" 
                    << "    setall sbmu:bits[:rack_num]:1  <num> <val>\n" ;
            }
            else if (swords.size() >= 4)
            {
                // oss
                //     << "set a value 2 <"<<swords[1]
                //     << std::endl;
                id = encode_id(swords[1],0);    
                oss
                        << "{\"id\":\"0x"<<std::hex << id << std::dec <<"\",";
                    //<< std::endl;
                std::vector<int>values;
                try {
                    int num  = std::stoi(swords[2]);
                    int val  = std::stoi(swords[3]);
                    for (int i = 0; i < num; i++)
                    {
                        values.push_back(val);
                    }
                }  catch (const std::exception& ex) {
                    oss <<  " values must be numbers\n";
                    return oss.str();
                }               

                std::string qstring  = generateIdQuery("set", 202, id, "any", (int)values.size(), values);
                std::string response = run_wscat(g_url, qstring);  // Run the query
                oss
                    << " url  ["<<g_url<<"] \n"
                    << " query  ["<<qstring<<"] \n"
                    << " response  ["<<response<<"] \n"
                    << std::endl;

            }
            return oss.str();
        }
        else if ( swords[0] == "get")
        {
            uint32_t id = 0;
            std::vector<int>values = {0};
            std::ostringstream oss;
            if (swords.size() == 1)
            {
                oss
                    << "get a value <table><name|offset"
                    << std::endl
                    << " examples \n" 
                    << "    get sbmu:bits[:rack_num]:1\n" 
                    << "    get sbmu:bits[:rack_num]:1  <num of bits> \n" 
                    << "    sbmu:bits:summary_total_undervoltage\n"
                    << "    get rtos:bits:20:1\n" ;
            }
            else if (swords.size() == 2)
            {
                // oss
                //     << "get a value 2 <"<<swords[1]
                //     << std::endl;
                uint32_t id;
                id = encode_id(swords[1], 0);
                // oss
                // id = 0x33nnaaaa
                int rack_num = (id & 0x00ff0000) >> 16;
                //printf("id 0x%x extracted rack_num %d \n", id, rack_num);
                if(rack_num == 20)
                {
                    printf("running on all racks \n");
                }
                // oss
                //     << "id  <0x"<<std::hex << id << std::dec <<"> "
                //     << std::endl;
                if(rack_num == 20)
                {
                    oss << "[";
                    for ( auto i = 0 ; i < 20 ; i++)
                    {
                        if (i> 0)
                            oss <<",";
                        oss << "\n";
                        setRackNum(id,i);
                        std::string qstring  = generateIdQuery("get", 202+i, id, "any", (int)values.size(), values);
                        std::string response = run_wscat(g_url, qstring);  // Run the query
                        oss  << response;
                    }
                    oss << "]";
                }
                else
                {
                    std::string qstring  = generateIdQuery("get", 202, id, "any", 1, values);
                    std::string response = run_wscat(g_url, qstring);  // Run the query
                    // oss
                    //     << " url  ["<<g_url<<"] \n"
                    //     << " query  ["<<qstring<<"] \n"
                    //     << " response  ["<<response<<"] \n"
                    //     << std::endl;
                    oss << response
                        << std::endl;
                }
                //std::string run_query(url, mb_client, qstring);
                //std::string response = run_wscat(url, qstring);  // Run the query

            }
            else if (swords.size() == 3)
            {
                // oss
                //     << "get a value 3 <"<<swords[1]<<"> <"<<swords[2]<<">"
                //     << std::endl;
                int num = 1;
                try {
                    num = std::stoi(swords[2]);
                }
                catch (const std::exception& ex) {
                    oss <<  " values must be numbers\n";
                    return oss.str();
                }
                id = encode_id(swords[1],0);    
// oss
                // id = 0x33nnaaaa
                int rack_num = (id & 0x00ff0000) >> 16;
                //printf("id 0x%x extracted rack_num %d \n", id, rack_num);
                if(rack_num == 20)
                {
                    printf("running on all racks \n");
                }
                // oss
                //     << "id  <0x"<<std::hex << id << std::dec <<"> "
                //     << std::endl;
                std::stringstream ossr;
                ossr << "[";
                
                if(rack_num == 20)
                {
                    for ( auto i = 0 ; i < 20 ; i++)
                    {
                        setRackNum(id,i);
                        std::string qstring  = generateIdQuery("get", 202+i, id, "any", num, values);
                        std::string response = run_wscat(g_url, qstring);  // Run the query
                        ossr  << response;
                        if (i < 19)
                            ossr <<",";


                    }
                    ossr << "]";

                    if( !use_str.empty())
                    {
                        std::vector<std::vector<int>>data2D;
                        resp_to_2dvec(data2D, ossr);
                        var_tables[use_str] = std::move(data2D);
                    }

                    oss << ossr.str();
                }
                else
                {
                std::string qstring  = generateIdQuery("get", 202, id, "any", num, values);
                //std::cout << " qstring ["<<qstring<<"]\n";
                std::string response = run_wscat(g_url, qstring);  // Run the query
                if( !use_str.empty())
                {
                    std::vector<int>data1D;
                    resp_to_1dvec(data1D, response);
                    var_tables[use_str] = std::move(data1D);
                }
                std::cout
                    << " url  ["<<g_url<<"] \n"
                    << " query  ["<<qstring<<"] \n"
                    << " response  ["<<response<<"] \n"
                    << std::endl;
                oss << response
                    << std::endl;
                }

            }
            return oss.str();
        }
        else if ( swords[0] == "getmb")
        {
            uint32_t id = 0;
            std::vector<int>values = {0};
            std::ostringstream oss;
            if (swords.size() == 1)
            {
                oss
                    << "getmb a value <table><name|offset using the modbus connectipn"
                    << std::endl
                    << " examples \n" 
                    << "    getmb sbmu:bits[:rack_num]:1\n" 
                    << "    getmb sbmu:bits[:rack_num]:1  <num of bits> \n" 
                    << "    getmb sbmu:bits:summary_total_undervoltage\n";
            }
            else if (swords.size() == 2)
            {
                // oss
                //     << "get a value 2 <"<<swords[1]
                //     << std::endl;
                id = encode_id(swords[1],0);    
                oss
                    << "{\"id\" :\"0x"<<std::hex << id << std::dec <<"\",";
                int num = 1;
                std::vector<int>data= {};
                auto mbresp = get_modbus(id, num, data);  // Run the query
                oss
                    << " \"response\":"<<mbresp.first<<",";
                oss << " \"data\":[";
                bool dfirst = true;
                for(const auto &item : mbresp.second) {
                    if(dfirst)
                    {
                        dfirst = false;
                    }
                    else 
                    {
                        oss <<", ";
                    }
                    oss << item;
                }
                oss << "]\n";
            }
            if (swords.size() == 3)
            {
                id = encode_id(swords[1],0);    
                oss
                    << "{\"id\" :\"0x"<<std::hex << id << std::dec <<"\",";
                int num = 1;
                try {
                    num = std::stoi(swords[2]);
                }
                catch (const std::exception& ex) {
                    oss <<  " values must be numbers\n";
                    return oss.str();
                }
                std::vector<int>data= {};
                auto mbresp = get_modbus(id, num, data);  // Run the query
                oss
                    << " \"response\":"<<mbresp.first<<",";
                oss << " \"data\":[";
                bool dfirst = true;
                for(const auto &item : mbresp.second) {
                    if(dfirst)
                    {
                        dfirst = false;
                    }
                    else 
                    {
                        oss <<", ";
                    }
                    oss << item;
                }
                oss << "]}\n";

            }
            return oss.str();
        }

        else if ( swords[0] == "setallmb")
        {
            uint32_t id = 0;
            std::vector<int>values = {0};
            std::ostringstream oss;
            if (swords.size() == 1)
            {
                oss
                    << "getmb a value <table><name|offset using the modbus connectipn"
                    << std::endl
                    << " examples \n" 
                    << "    setallmb sbmu:bits[:rack_num]:1  <num> <val>\n" ;
            }
            else if (swords.size() >= 4)
            {
                // oss
                //     << "set a value 2 <"<<swords[1]
                //     << std::endl;
                id = encode_id(swords[1],0);    
                oss
                        << "{\"id\":\"0x"<<std::hex << id << std::dec <<"\",";
                    //<< std::endl;
                std::vector<int>values;
                try {
                    int num  = std::stoi(swords[2]);
                    int val  = std::stoi(swords[3]);
                    for (int i = 0; i < num; i++)
                    {
                        values.push_back(val);
                    }
                }  catch (const std::exception& ex) {
                    oss <<  " values must be numbers\n";
                    return oss.str();
                }               


                int num = values.size();
                auto mbresp = set_modbus(id, num, values);  // Run the query
                if(mbresp.first == -1)
                {
                    mbx_client.connect(mb_connect);
                    mbresp = set_modbus(id, num, values);
                }
                oss
                    << "\"response\":"<<mbresp.first<<",";
                oss << " \"data\": [";
                bool tfirst =  true;
                for(const auto &item : mbresp.second) {
                    if ( tfirst)
                    {
                        tfirst = false;
                    }
                    else
                    {
                        oss << ", ";
                    }
                    oss << item;
                }
                oss << "]}\n";
            }
            return oss.str();
        }
        else if ( swords[0] == "setmb")
        {
            uint32_t id = 0;
            std::vector<int>values = {0};
            std::ostringstream oss;
            if (swords.size() == 1)
            {
                oss
                    << "getmb a value <table><name|offset using the modbus connectipn"
                    << std::endl
                    << " examples \n" 
                    << "    setmb sbmu:bits[:rack_num]:1\n" 
                    << "    setmb sbmu:bits[:rack_num]:1  1 2 3 4 \n" 
                    << "    setmb sbmu:bits:summary_total_undervoltage 1\n";
            }
            else if (swords.size() >= 2)
            {
                // oss
                //     << "set a value 2 <"<<swords[1]
                //     << std::endl;
                id = encode_id(swords[1],0);    
                oss
                        << "{\"id\":\"0x"<<std::hex << id << std::dec <<"\",";
                    //<< std::endl;
                std::vector<int>values;
                try {
 
                    for (int i = 2; i < swords.size(); i++)
                    {
                        values.push_back(std::stoi(swords[i]));
                    }
                }  catch (const std::exception& ex) {
                    oss <<  " values must be numbers\n";
                    return oss.str();
                }               


                int num = values.size();
                auto mbresp = set_modbus(id, num, values);  // Run the query
                if(mbresp.first == -1)
                {
                    mbx_client.connect(mb_connect);
                    mbresp = set_modbus(id, num, values);
                }

                oss
                    << "\"response\":"<<mbresp.first<<",";
                oss << " \"data\": [";
                bool tfirst =  true;
                for(const auto &item : mbresp.second) {
                    if ( tfirst)
                    {
                        tfirst = false;
                    }
                    else
                    {
                        oss << ", ";
                    }
                    oss << item;
                }
                oss << "]}\n";
            }
            // if (swords.size() == 3)
            // {
            //     oss
            //         << "get a value 3 <"<<swords[1]<<"> <"<<swords[2]<<">"
            //         << std::endl;
            //     int num = 1;
            //     try {
            //         num = std::stoi(swords[2]);
            //     }
            //     catch (const std::exception& ex) {
            //         oss <<  " values must be numbers\n";
            //         return oss.str();
            //     }
            //     std::vector<int>data= {};
            //     auto mbresp = get_modbus(id, num, data);  // Run the query
            //     oss
            //         << " response:"<<mbresp.first<<",\n";
            //     oss << " data [";
            //     for(const auto &item : mbresp.second) {
            //         oss << item << " ";
            //     }
            //     oss << "]\n";
            // }
            return oss.str();
        }
        else if ( swords[0] == "tables")
        {
            std::ostringstream oss;

            for (const auto& table_pair : data_tables) {
                int total_size = 0;
                for (auto & item : table_pair.second.items){
                    if(item.name[0] != '_')
                        total_size += item.size;
                }
                oss << table_pair.first 
                    << " number of items :"
                    << total_size
                    << std::endl;
            }
            return oss.str();
        }
        else if ( swords[0] == "test_tables")
        {
            std::ostringstream oss;

            for (const auto& table_pair : test_data_tables) {
                int total_size = 0;
                for (auto & item : table_pair.second.items){
                    if(item.name[0] != '_')
                        total_size += item.size;
                }
                oss << table_pair.first 
                    << " number of items :"
                    << total_size
                    << std::endl;
            }
            return oss.str();
        }
        else if ( swords[0] == "show")
        {
            if (swords.size() > 1)
            {
                if (swords[1] == "tables")
                {
                    std::ostringstream oss;

                    for (const auto& table_pair : data_tables) {
                        oss << table_pair.first << std::endl;
                        for (const auto& table_item : table_pair.second.items) {
                            oss <<"    name: "<< table_item.name 
                                << " offset: "  << table_item.offset 
                                << " size: "  << table_item.size 
                                << std::endl;
                        }
                    }
                    return oss.str();
                }
                if (swords[1] == "test_tables")
                {
                    std::ostringstream oss;

                    for (const auto& table_pair : test_data_tables) {
                        oss << table_pair.first << std::endl;
                        for (const auto& table_item : table_pair.second.items) {
                            oss <<"    name: "<< table_item.name 
                                << " offset: "  << table_item.offset 
                                << " size: "  << table_item.size 
                                << std::endl;
                        }
                    }
                    return oss.str();
                }
                else if (swords[1] == "vars")
                {
                    std::ostringstream oss;
                    showvars(oss);
                    return oss.str();
                }
 
            }
            std::ostringstream oss;
            oss <<"show :-\n";
            oss <<"      tables - show all the data point tables\n";
            return oss.str();
        }
        else if ( swords[0] == "json")
        {
            if (swords.size() > 1)
            {
                if (swords[1] == "methods")
                {                    std::ostringstream oss;
                    bool first = true;
                    oss << "[";
                    // sbms_trans needs to be registered into trans_tables
                    for (const auto& item : meth_dict) {
                        if (!first) 
                        {
                            oss << ",\n";
                        }
                        else
                        {
                            first =  false;
                            oss << "\n";
                        }
                            
                        oss << "   { \"method\": \""<< item.first << "\"," 
                            << "\"desc\": \""<< item.second.desc << "\""
                            << "}";
                    }
                    oss << "\n]\n";
                    return oss.str();

                }
                //std::vector<TransItem> sbms_trans 
                else if (swords[1] == "trans")
                {
                    std::ostringstream oss;
                    bool first = true;
                    oss << "[";
                    // sbms_trans needs to be registered into trans_tables
                    for (const auto& table_item : trans_tables) {
                        if (!first) 
                        {
                            oss << ",\n";
                        }
                        else
                        {
                            first =  false;
                            oss << "\n";
                        }
                            
                        oss << "   { \"table\": \""<< table_item.first << "\"," 
                            << "\"desc\": \""<< table_item.second.desc << "\",\n"
                            << "   \"items\":[";
                        bool tfirst = true;
                        //oss << "\n    [\n";
                        for (const auto& trans : table_item.second.items) {
                            if (!tfirst) 
                            {
                                oss << ",\n";
                            }
                            else
                            {
                                tfirst =  false;
                                oss << "\n";
                            }
                             oss <<  "      " << TransItemtoJsonString(trans);
                        }
                        oss << "\n     ]\n  }";
                    }
                    oss << "\n]\n";
                    return oss.str();
                }
                else if (swords[1] == "tables")
                {
                    const int nameFieldWidth = 48; // Desired width for the name field
                    std::ostringstream oss;
                    oss << "[";
                    bool tfirst = true;
                    for (const auto& table_pair : data_tables) {
                        if (!tfirst) 
                        {
                            oss << ",\n";
                        }
                        else
                        {
                            tfirst =  false;
                            oss << "\n";
                        }
                            
                        oss << "   { \"table\": \""
                        << table_pair.first << "\", \"items\":[";
                        bool first = true;
                        for (const auto& table_item : table_pair.second.items) {
                            if (!first) {
                                oss << ",\n";
                            }
                            else
                            {
                                first =  false;
                                oss << "\n";
                            }

                            int spacesNeeded = nameFieldWidth - table_item.name.length(); // Calculate the number of spaces needed
                            if (spacesNeeded < 0) spacesNeeded = 0; // Ensure no negative values
                            std::string spaces(spacesNeeded, ' '); // Create a string of spaces

                            //oss <<"     {\"name\":" << std::left << std::setw(32)<< "\""<< table_item.name 
                            oss <<"     {\"name\":"<< spaces << "\""<< table_item.name 
                                << "\", \"offset\": "  << table_item.offset 
                                << ", \"size\": "  << table_item.size 
                                << "}" ;//<< std::endl;
                        }
                        oss << "\n     ]\n   }";
                    }
                    tfirst = true;

                    for (const auto& map_pair : data_maps) {
                        if (!tfirst) 
                        {
                            oss << ",\n";
                        }
                        else
                        {
                            tfirst =  false;
                            oss << "\n";
                        }
                            
                        oss << "   { \"map\": \""
                        << map_pair.first << "\", \"items\":[";
                        bool first = true;
                        for (const auto& map_item : map_pair.second.sitems) {
                            if (!first) {
                                oss << ",\n";
                            }
                            else
                            {
                                first =  false;
                                oss << "\n";
                            }

                            int spacesNeeded = nameFieldWidth - map_item.second.destname.length(); // Calculate the number of spaces needed
                            if (spacesNeeded < 0) spacesNeeded = 0; // Ensure no negative values
                            std::string spaces(spacesNeeded, ' '); // Create a string of spaces

                            //oss <<"     {\"name\":" << std::left << std::setw(32)<< "\""<< table_item.name 
                            oss <<"     {\"name\":"<< spaces << "\""<< map_item.second.destname 
                                << "\"";
                            if(map_item.second.method == "map")
                            {
                                oss << ",\"map\": \""  << map_item.second.srcname 
                                << "\" ";
                            }
                            if(map_item.second.method == "mapbit")
                            {
                                oss << ",\"mapbit\": \""  << map_item.second.srcname 
                                << "\" ";
                            }
                            if(map_item.second.method == "calc")
                            {
                                oss << ",\"calc\": \""  << map_item.second.srcname 
                                << "\" ";
                            }
                            if(map_item.second.size > 0)
                            {
                                oss <<", \"size\": "  << map_item.second.size 
                                << "" ;//<< std::endl;
                            }
                        }
                        oss << "\n     ]\n   }";

                    }
                    oss << "]\n";
                    return oss.str();
                }
                else if (swords[1] == "tests")
                {
                    const int nameFieldWidth = 48; // Desired width for the name field
                    std::ostringstream oss;
                    oss << "[";
                    bool tfirst = true;
                    for (const auto& table_pair : test_tables) {
                        if (!tfirst) 
                        {
                            oss << ",\n";
                        }
                        else
                        {
                            tfirst =  false;
                            oss << "\n";
                        }
                            
                        oss << "   { \"test\": \""
                        << table_pair.first << "\", \"items\":[";
                        bool first = true;
                        for (const auto& table_item : table_pair.second.items) {
                            if (!first) {
                                oss << ",\n";
                            }
                            else
                            {
                                first =  false;
                                oss << "\n";
                            }

                            int spacesNeeded = nameFieldWidth - table_item.action.length(); // Calculate the number of spaces needed
                            if (spacesNeeded < 0) spacesNeeded = 0; // Ensure no negative values
                            std::string spaces(spacesNeeded, ' '); // Create a string of spaces

                            //oss <<"     {\"name\":" << std::left << std::setw(32)<< "\""<< table_item.name 
                            oss <<"     {\"action\":"<< spaces << "\""<< table_item.action 
                                << "\", \"desc\": "  << table_item.desc 
                                << ", \"data\": [";  //<< table_item.size 
                            for (size_t i = 0; i < table_item.data.size(); ++i) {
                                if (i > 0) oss << ", ";
                                oss << table_item.data[i];
                            }
                            oss << "]"  //<< table_item.size 
                                << "}" ;//<< std::endl;
                        }
                        oss << "\n     ]\n   }";

                    }
                    oss << "]\n";
                    return oss.str();
                }
            }
            {
                std::ostringstream oss;
                oss <<"json :-\n";
                oss <<"      methods - show all the built in transition methods\n";
                oss <<"      tables - show all the data point tables\n";
                oss <<"      trans  - show all the transfer tables\n";
                oss <<"      tests  - show all the test tables\n";
                return oss.str();
            }
        }
        else if ( swords[0] == "find")
        {
            std::ostringstream oss;
            if (swords.size() > 1)
            {
                if (swords[1] == "name"  && swords.size() > 2) 
                {
                    for (const auto& table_pair : data_tables) {
                        for (const auto& table_item : table_pair.second.items) {
                            if(table_item.name.find(swords[2]) != std::string::npos) {
                                oss << table_pair.first << ":"
                                    << table_item.offset << ":"
                                    << table_item.name << "\n";
                            }
                        }
                    }
                }
                else if (swords[1] == "offset"  && swords.size() > 2) 
                {
                    int offset = str_to_offset(swords[2]);
                    for (const auto& table_pair : data_tables) {
                        for (const auto& table_item : table_pair.second.items) {
                            if(table_item.offset == offset) {
                                oss << table_pair.first << ":"
                                    << table_item.offset << ":"
                                    << table_item.name << "\n";
                            }
                        }
                    }
                }
                else if (swords[1] == "var"  && swords.size() > 2) 
                {
                    for (const auto& table_pair : var_tables) {
                        if(table_pair.first.find(swords[2]) != std::string::npos) {
                            oss << table_pair.first << ":";
                            showvar(oss, table_pair.first);
                            //oss     << "\n";
                        }
                    }
                }

            }
            if (oss.str().empty())
            {
                oss << " find :-\n";
                oss << "   name <name>: find item by name \n";
                oss << "   offset <offset>: find item by offset \n";
            }
            return oss.str();
        }
    }
    if (parts.size() < 2) {
        return "Invalid query format. Use 'table:query' format.\n";
    }

    std::string table_key = parts[0];
    std::string identifier = parts[1];
    std::string item;
    table_key += ":";

    if (parts.size() > 2) {
        table_key += parts[1];
        item = parts[2];
    }
    else
    {
        std::istringstream iss2(identifier);
        parts.clear();
        while (std::getline(iss2, part, ' ')) {
            parts.push_back(part);
        
        }
        table_key += parts[0];
        item = parts[1];
    }
    identifier = item;
    
    if (data_tables.find(table_key) == data_tables.end()) {
        return "Table not found.\n";
    }

    const auto& items = data_tables.at(table_key).items;

    if (identifier.find('*') != std::string::npos) {  // Pattern match query
        std::string pattern = identifier.replace(identifier.find('*'), 1, "");
        std::vector<std::string> results;
        for (const auto& item : items) {
            if (item.name.find(pattern) != std::string::npos) {
                results.push_back(item.name + " at offset " + std::to_string(item.offset) + "\n");
            }
        }
        return !results.empty() ? std::accumulate(results.begin(), results.end(), std::string("")) : "No items found matching the pattern.\n";
    } else if (identifier.find('-') != std::string::npos) {  // Range query
        size_t dash_pos = identifier.find('-');
        int start = std::stoi(identifier.substr(0, dash_pos));
        int end = std::stoi(identifier.substr(dash_pos + 1));
        std::vector<std::string> results;
        for (const auto& item : items) {
            if (item.offset >= start && item.offset <= end) {
                results.push_back(item.name + " at offset " + std::to_string(item.offset)+ "\n");
            }
        }
        return !results.empty() ? std::accumulate(results.begin(), results.end(), std::string("")) : "No items found in the specified range.\n";
    } else if (std::isdigit(identifier[0])) {  // Offset query
        int offset = std::stoi(identifier);
        for (const auto& item : items) {
            if (item.offset == offset) {
                return item.name + " at offset " + std::to_string(item.offset) + "\n";
            }
        }
        return "Item not found at the specified offset.\n";
    } else {  // Name query
        for (const auto& item : items) {
            if (item.name == identifier) {
                return item.name + " at offset " + std::to_string(item.offset) + "\n";
            }
        }
        return "Item not found with the specified name.\n";
    }
}

// Define a function pointer type for handling queries
typedef std::string (*QueryHandler)(const std::string&, std::map<std::string, DataTable>&, int);

void start_server(int port, QueryHandler query_handler, std::map<std::string, DataTable>& data_tables) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for client connections
    if (listen(server_fd, 10) < 0) { // Can handle 10 clients in waiting queue
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server started on port " << port << std::endl;

    // Accept clients and handle them concurrently
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            continue;
        }

        // Create a new thread for each client
        std::thread([new_socket, &data_tables, query_handler]() {
            char buffer[1024];
            while (true) {
                memset(buffer, 0, sizeof(buffer));
                int bytes_received = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) {
                    std::cout << "Client disconnected or error occurred.\n";
                    break;
                }

                std::string query(buffer);
                std::string response = query_handler(query, data_tables, new_socket);
                send(new_socket, response.c_str(), response.size(), 0);
            }

            close(new_socket);
        }).detach(); // Detach thread to handle client independently
    }

    close(server_fd);
}


DataItemManager manager;

// Function to parse the input string into table_key and name
std::pair<std::string, std::string> parseKeyAndName(const std::string& input) {
    size_t lastColonPos = input.rfind(':'); // Find the position of the last colon
    if (lastColonPos == std::string::npos) {
        // Handle the error case where no colon is found
        return {"", ""}; // Return empty pair
    }

    std::string table_key = input.substr(0, lastColonPos);
    std::string name = input.substr(lastColonPos + 1);

    return {table_key, name};
}

DataItem* find_key_name(const std::string& input) {
    auto [table_key, name] = parseKeyAndName(input);
    auto& items = data_tables.at(table_key).items;
    for (auto& item : items) {
        if (item.name == name) {
            return &item; 
        }
    }
    return nullptr;
}


// name_mappings = {
//     'State': 'status',
//     'Max_Allowed_Charge_Power': 'chargeable_power',
//     'Max_Allowed_Discharge_Power':'dischargeable_power',

//     'Max_Charge_Voltage':'chargeable_volt',
//     'Max_Discharge_Voltage':'dischargeable_volt',

//     'Max_Charge_Current':'chargeable_current',
//     'Max_Discharge_Current':'dischargeable_current',

//     'DI1':"di1",
//     'DI2':"di2",
//     'DI3':"di3",
//     'DI4':"di4",
//     'DI5':"di5",
//     'DI6':"di6",
//     'DI7':"di7",
//     'DI8':"di8",

//     'Max_cell_voltage':'max_voltage',
//     'Max_cell_voltage_number':'max_voltage_num',
//     'Min_cell_voltage':'min_voltage',
//     'Min_cell_voltage_number':'min_voltage_num',
//     'Max_cell_temperature': 'max_temp',
//     'Max_cell_temperature_number':'max_temp_num',
//     'Min_cell_temperature': 'min_temp',
//     'Min_cell_temperature_number':'min_temp_num',
//     'Max_cell_SOC':'max_soc',
//     'Max_cell_SOC_number':'max_soc_num',
//     'Min_cell_SOC':'min_soc',
//     'Min_cell_SOC_number':'min_soc_num',
//     'SOC':'soc',
//     'SOH':'soh',
//     'Average_cell_voltage':'avg_voltage',
//     'Average_cell_temperature':'avg_temp',

//     'Accumulated_charge_capacity':'total_charge_cap',
//     'Accumulated_discharge_capacity':'total_discharge_cap'

// }

int run_mapper() {
    // Assuming JSON data is stored in a file called "mapping.json"
    std::ifstream json_file("mapping.json");
    nlohmann::json json_data;
    json_file >> json_data;

    //std::vector<int> mappedItems; // This vector will store destination keys

    // Process each entry in the JSON array
    for (const auto& entry : json_data) {
        MappingEntry mapping;
        mapping.src = entry["src"];
        mapping.dest = entry["dest"];
        mapping.size = entry["size"];
        mapping.method = entry["method"];

        //auto [table_key, name] = parseKeyAndName(mapping.src);

// sbmu:hold:at_single_charge_temp_over",
// we need the table_key sbmu:hold
// we need the name at_single_charge_temp_over
// look in 
//std::string handle_query(const std::string& qustr, const std::map<std::string, DataTable>& data_tables) {

        //int srcKey = manager.registerItem(mapping.src);
        //int destKey = manager.registerItem(mapping.dest);

        DataItem* msrc = find_key_name(mapping.src);
        DataItem* mdest = find_key_name(mapping.dest); 
        if(msrc && mdest)
        {
            msrc->mappedkeys.push_back(mdest->mykey);
        }

        // Assuming you link the destination key with the source key somehow
        //mappedItems.push_back(destKey);

        // Process each mapping as needed
        std::cout << "Mapped " << mapping.src << " to " << mapping.dest << " with method " << mapping.method << std::endl;
    }

    return 0;
}

int test_server(std::map<std::string, DataTable>) {
    int port = 9999;
    start_server(port, handle_query, data_tables);
    return 0;
}

// int process_json_item(std::map<std::string, DataTable>& data_tables, const json &jitem) {
//     std::string table_name = jitem["table"];
//     std::cout << " Table name [" << table_name<<"]\n";
//     int current_offset  = 0;
//     data_tables[table_name] = DataTable(current_offset);
//     data_tables[table_name].base_offset = current_offset;// = DataTable(current_offset);
//     json jtable = item["items"];
//     if (jtable.is_array()){
//         std::cout << " Items Array found " << std::endl;
//     }
//     for (const auto& jitem : jtable) {
//         std::string name = jitem["name"];
//         int offset =  jitem["offset"];
//         int size =  jitem["size"];
//         // we may have a notes string 
//         //{"name":                            "terminal_overvoltage", "offset": 0, "size": 3},
//         DataItem item(name, offset, size);
//         data_tables[table_name].items.push_back(item);
//         data_tables[table_name].calculated_size += size;
//     }
//     return 0;
// }

int meth_to_id(std::string & meth, std::string& desc)
{
    int meth_id = 0;
    if(meth == "3sum")
        meth_id = METH_3SUM;
    else if(meth == "into")
        meth_id = METH_INTO;
    else if(meth == "3lim_max")
        meth_id = METH_3LIMMAX;
    else if(meth == "3lim_min")
        meth_id = METH_3LIMMIN;
    else if(meth == "3limmax")
        meth_id = METH_3LIMMAX;
    else if(meth == "3limmin")
        meth_id = METH_3LIMMIN;
    else if(meth == "max")
        meth_id = METH_MAX;
    else if(meth == "min")
        meth_id = METH_MIN;
    else if(meth == "sum")
        meth_id = METH_SUM;
    else if(meth == "sum2")
        meth_id = METH_SUM2;
    else if(meth == "count")
        meth_id = METH_COUNT;
    else
    {
        std::cout << " Undefined method ["<< meth <<"] operation ["<< desc <<"] will be ignored"<<std::endl;
    }
    return meth_id;


} 

//int meth_str_to_id(const std::string& meth_str, const std::string& desc) {
uint32_t shm_def_to_id(std::string& shmdef)
{
    uint32_t id = 0;
    if (shmdef == "none")
        return id;

    MemId mem_id;
    RegId reg_id; 
    int rack_num ;
    uint16_t offset;   
    bool def_ok = decode_shmdef(mem_id, reg_id, rack_num, offset,shmdef);  
    
    if(def_ok)
    {
        id |= (static_cast<uint32_t>(mem_id) & 0xF) << 28;  // 4 bits for mem_id
        id |= (static_cast<uint32_t>(reg_id) & 0xF) << 24;  // 4 bits for reg_id
        id |= (static_cast<uint32_t>(rack_num) & 0xFF) << 16;   // 8 bits for rack
        id |= (offset & 0xFFFF);       
    }
    else
    {
        std::cout << " Undefined memory ["<< shmdef <<"]  will be ignored ";
    }
    
    return id;
}
//int meth_str_to_id(const std::string& meth_str, const std::string& desc) {
uint32_t shm_def_to_id(std::string& shmdef, const std::string& item, std::string& desc)
{
    return shm_def_to_id(shmdef);
}


int process_ops_item(std::map<std::string, OpsTable>& ops_tables, const json &jitem) {
    if (!jitem.contains("table") || !jitem["table"].is_string()) {
        std::cerr << "Error: Missing or invalid 'table' field" << std::endl;
        return -1;
    }

    std::string table_name = jitem["table"];
    std::cout << "Table name [" << table_name << "]\n";

    if (!jitem.contains("items") || !jitem["items"].is_array()) {
        std::cerr << "Error: Missing or invalid 'items' array" << std::endl;
        return -1;
    }

    json jtable = jitem["items"];
    std::cout << "Items Array found" << std::endl;

    bool isNewTable = ops_tables.find(table_name) == ops_tables.end();
    if (isNewTable) {
        ops_tables[table_name] = OpsTable();
        ops_tables[table_name].name = table_name; // Uses the default constructor
    }

    // Create or access the DataTable for this table_name
    OpsTable& table = ops_tables[table_name];
    if (isNewTable) {
        table.name = table_name;
    }

    for (const auto& jitem : jtable) {
        if (!jitem.contains("src") || !jitem.contains("dest") || !jitem.contains("meth")|| !jitem.contains("lim")|| !jitem.contains("desc")) {
            std::cerr << "Error: Missing required fields in item" << std::endl;
            continue;  // Skip this item or you might want to return an error
        }

        std::string src = jitem["src"];
        std::string dest = jitem["dest"];
        std::string meth = jitem["meth"];
        std::string lim = jitem["lim"];
        std::string desc = jitem["desc"];
        std::string notes = jitem.value("notes", "");  // Optional notes field
        std::cout << " table ["<<table_name<<"] Added opts item [" << desc <<"]\n"; 
        OpsItem item(src, dest, meth, lim, desc);
        item.src_id =  shm_def_to_id(src,   "src",  desc);
        item.dest_id = shm_def_to_id(dest,  "dest", desc);
        item.lim_id =  shm_def_to_id(lim,   "lim",  desc);
        item.meth_id = meth_to_id(meth, desc);

        table.items.push_back(item);
        //table.calculated_size += size;
    }

    return 0;
}

int process_json_item(std::map<std::string, DataTable>& data_tables, const json &jitem);
// int process_json_item(std::map<std::string, DataTable>& data_tables, const json &jitem) {
//     if (!jitem.contains("table") || !jitem["table"].is_string()) {
//         std::cerr << "Error: Missing or invalid 'table' field" << std::endl;
//         return -1;
//     }

//     std::string table_name = jitem["table"];
//     std::cout << "Table name [" << table_name << "]\n";

//     if (!jitem.contains("items") || !jitem["items"].is_array()) {
//         std::cerr << "Error: Missing or invalid 'items' array" << std::endl;
//         return -1;
//     }

//     json jtable = jitem["items"];
//     std::cout << "Items Array found" << std::endl;

//     bool isNewTable = data_tables.find(table_name) == data_tables.end();
//     if (isNewTable) {
//         data_tables[table_name] = DataTable(); // Uses the default constructor
//     }

//     // Create or access the DataTable for this table_name
//     DataTable& table = data_tables[table_name];
//     if (isNewTable) {
//         table.base_offset = 0;  // Set base_offset only if it's a new table
//         table.calculated_size = 0;  // Ensure calculated size starts at 0
//     }

//     for (const auto& jitem : jtable) {
//         if (!jitem.contains("name") || !jitem.contains("offset") || !jitem.contains("size")) {
//             std::cerr << "Error: Missing required fields in item" << std::endl;
//             continue;  // Skip this item or you might want to return an error
//         }

//         std::string name = jitem["name"];
//         int offset = jitem["offset"];
//         int size = jitem["size"];
//         std::string notes = jitem.value("notes", "");  // Optional notes field

//         DataItem item(name, offset, size);
//         table.items.push_back(item);
//         table.calculated_size += size;
//     }

//     return 0;
// }

int test_ops_parse(std::map<std::string, OpsTable>& ops_tables, const std::string &filename,std::ostringstream& oss) {
    std::ifstream file(filename);
    json jfile;

    if (!file.is_open()) {
        std::cout <<"Could not open file: " + filename;
        //throw std::runtime_error("Could not open file: " + filename);
    }

    try {

        file >> jfile;
        std::cout << " json loaded " << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    try {
        // Iterate over each element in the array
        for (const auto& item : jfile) {
        // Check if "table" key exists in the JSON object
            if(item.contains("Title"))
                std::cout << " Data Tables ["<<item["Title"]<<"]\n";
            if(item.contains("Release"))
                std::cout << " Release ["<<item["Release"]<<"]\n";
            if(item.contains("Date"))
                std::cout << " Date ["<<item["Date"]<<"]\n";

            if (item.contains("table") && item["table"].is_string()) {
                process_ops_item(ops_tables, item);
                //process_json_item(data_tables, item);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}

int test_json_parse(std::map<std::string, DataTable>& data_tables, const std::string &filename) {

    std::ifstream file(filename);

    try {
        
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }
        json jfile;
        file >> jfile;
        std::cout << " json loaded " << std::endl;


        if (jfile.is_array()){
            std::cout << " Array found " << std::endl;
        }

        // Iterate over each element in the array
        for (const auto& item : jfile) {
        // Check if "table" key exists in the JSON object
            if(item.contains("Title"))
                std::cout << " Data Tables ["<<item["Title"]<<"]\n";
            if(item.contains("Release"))
                std::cout << " Release ["<<item["Release"]<<"]\n";
            if(item.contains("Date"))
                std::cout << " Date ["<<item["Date"]<<"]\n";

            if (item.contains("table") && item["table"].is_string()) {
                process_json_item(data_tables, item);
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
//        auto data_file = "src/data_definition.txt";
int test_parse(std::map<std::string, DataTable>& data_tables, const std::string & data_file) {
    try {
        // if (argc < 2) {
        //     std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        //     return 1;
        // }
        //auto data_file = "src/data_definition.txt";
        parse_definitions(data_tables, data_file);
        for (const auto& table_pair : data_tables) {
            const auto& table = table_pair.second;
            std::cout << "Table " << table_pair.first << " starts at offset " << table.base_offset << std::endl;
            std::cout << "Total calculated size: " << table.calculated_size << std::endl;
            for (const auto& item : table.items) {
                //std::cout << "  " << item.name << " at offset " << item.offset << " size " << item.size << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    if (verify_offsets(data_tables)) {
        std::cout << "All offsets are sequentially correct." << std::endl;
    } else {
        std::cout << "There are errors in the data offsets." << std::endl;
    }

    // Example queries
    std::cout << handle_query("tables",                  data_tables,0) << std::endl;
    std::cout << handle_query("rtos:input rack_voltage", data_tables,0) << std::endl;
    std::cout << handle_query("rtos:input:rack_voltage", data_tables,0) << std::endl;
    std::cout << handle_query("rtos:input rack*",        data_tables,0) << std::endl;
    std::cout << handle_query("rtos:input 26", data_tables,0) << std::endl;
    std::cout << handle_query("rtos:input:26", data_tables,0) << std::endl;
    std::cout << handle_query("sbmu:bits 200-205",       data_tables,0) << std::endl;
    return 0;
}

// well assume a local data cache is acceses when the src_id is masked 

int run_ops_item(OpsItem& item, int rack_num, int max_rack, std::ostringstream& oss)
{
    uint32_t src_id = item.src_id;
    uint32_t dest_id = item.dest_id;
    uint32_t test_id = src_id | 0xc0000000; // mask src_id to divert to test cache

    if(set_debug)printf( " src_id 0x%x \n", src_id);
    if(set_debug)printf( " test_id 0x%x \n", test_id);

    // testid will use local cache memory

    switch (item.meth_id)
    {
        case METH_BLOCK:
        {
            // send all the values from src to dest lim is the string of the count
            if(item.count == 0)
            {
                // lim str is now two values "480:1000"
                std::istringstream iss(item.lim);
                std::string part;
                if (std::getline(iss, part, ':')) {
                    item.count = std::stoi(part); // First number before the colon
                }
                if (std::getline(iss, part)) {
                    item.rack_inc = std::stoi(part); // Second number after the colon
                }
            }
            if(rack_online(rack_num))
            {
                std::vector<int>test_values;
                setRackNum(src_id, rack_num); 
                //setRackNum(dest_id, rack_num); 
                get_mem_int_val_id(src_id, item.count, test_values);
                set_mem_int_val_id(dest_id+(item.rack_inc*rack_num), item.count, test_values);
            }

        }
        break;
        case METH_3LIMMAX:
        case METH_3LIMMIN:
        {
            if(rack_num == 0)
            {
                //set set max and min alert maybe we have max/min rack_num as well flags  
                set_mem_int_val_id(test_id, 8, {0,0,0,0,0,0, max_rack+1, max_rack+1 });
            }
            std::vector<int>test_results;
            get_mem_int_val_id(test_id, 6, test_results);
            if(rack_online(rack_num))
            {
                std::vector<int>test_values;
                setRackNum(src_id,rack_num); 
                get_mem_int_val_id(src_id, 1, test_values);
                std::vector<int>test_limits;
                get_mem_int_val_id(test_id, 4, test_limits);  // level 0 level 1 level 2 /diff
                if(item.meth_id==METH_3LIMMAX)
                {
                    if (test_values[0]> (test_limits[0]+test_limits[3]))
                        test_results[0] = 1;
                    if (test_values[0]> (test_limits[1]+test_limits[3]))
                        test_results[1] = 1;
                    if (test_values[0]> (test_limits[2]+test_limits[3]))
                        test_results[2] = 1;
                }
                else if(item.meth_id==METH_3LIMMIN)
                {
                    if (test_values[0]< (test_limits[0]-test_limits[3]))
                        test_results[0] = 1;
                    if (test_values[0]< (test_limits[1]-test_limits[3]))
                        test_results[1] = 1;
                    if (test_values[0]< (test_limits[2]-test_limits[3]))
                        test_results[2] = 1;
                }
                set_mem_int_val_id(test_id, 6, test_results);
            }
            if(rack_num == max_rack-1)
            {
                std::vector<int>test_results;
                get_mem_int_val_id(test_id, 6, test_results);
                if (item.meth_id == METH_3LIMMAX )
                {
                     set_mem_int_val_id(item.dest_id,   1, test_results[0]);
                     set_mem_int_val_id(item.dest_id+1, 1, test_results[1]);
                     set_mem_int_val_id(item.dest_id+2, 1, test_results[2]);
                }
                if (item.meth_id == METH_3LIMMIN )
                {
                     set_mem_int_val_id(item.dest_id,   1, test_results[3]);
                     set_mem_int_val_id(item.dest_id+1, 1, test_results[4]);
                     set_mem_int_val_id(item.dest_id+2, 1, test_results[5]);
                }
            }
        }
        break;
        // max min avg
        case METH_MAX: // we just do one for now
        case METH_MIN: // we just do one for now
        case METH_SUM: // we just do one for now
        case METH_SUM2: // we just do one for now
        case METH_COUNT: // we just do one for now
        {
            if(rack_num == 0)
            {
                //set max and min count and sum(32 bit) values
                set_mem_int_val_id(test_id, 5, {0,0xffff,0,0,0});
            }
            if(rack_online(rack_num))
            {
                std::vector<int>test_values;
                setRackNum(src_id,rack_num); 
                get_mem_int_val_id(src_id, 1, test_values);
                std::vector<int>test_results;
                get_mem_int_val_id(test_id, 5, test_results);
                if (test_results[0]< test_values[0])
                {
                    test_results[0] = test_values[0];  // max
                }
                if (test_results[1]> test_values[0])
                {
                    test_results[1] = test_values[0]; // min
                }
                test_results[2] = test_values[2]+1; // count 
                uint32_t sum = (test_results[3]<< 16) + test_results[4];
                sum += test_values[0];
                test_results[3] == sum>>16;
                test_results[4] == sum&0xffff;
                set_mem_int_val_id(test_id, 5, test_results);

            }
            if(rack_num == max_rack-1)
            {
                std::vector<int>test_results;
                get_mem_int_val_id(test_id, 5, test_results);

                if (item.meth_id == METH_MAX )
                    set_mem_int_val_id(item.dest_id, 1, test_results[0]);
                else if (item.meth_id == METH_MIN )
                    set_mem_int_val_id(item.dest_id, 1, test_results[1]);
                else if (item.meth_id == METH_SUM )
                    set_mem_int_val_id(item.dest_id, 1, test_results[4]);
                else if (item.meth_id == METH_SUM2 )
                {
                    set_mem_int_val_id(item.dest_id, 1, test_results[3]);
                    set_mem_int_val_id(item.dest_id+1, 1, test_results[4]);
                }
                else if (item.meth_id == METH_COUNT)
                    set_mem_int_val_id(item.dest_id, 1, test_results[2]);
            }
        }
        break;
        case METH_3SUM: // we just do one for now
        {
            if(rack_num == 0)
            {
                // set all 3  results to all 0
                set_mem_int_val_id(test_id, 3, {0,0,0});
                // set up dummy rack vals 
                // but this is just for one of the three levels
                //  but luckily we can do this
                if(ops_test)
                {
                    std::vector<int>init_vals(max_rack,0);
                    set_rack_values(src_id,   max_rack, init_vals);
                    set_rack_values(src_id+1, max_rack, init_vals);
                    set_rack_values(src_id+2, max_rack, init_vals);
                }
                if(ops_debug)printf(" before set src rack to 3 0x%x\n", src_id);
                setRackNum(src_id,3);
                if(ops_debug)printf(" after set src rack to 3 0x%x\n", src_id);
                // put a value in place
                //set_value_debug = true;
                if(ops_debug)printf(" set rack 3 val to 1\n");
                set_mem_int_val_id(src_id, 3, 1);
                //set_value_debug = false;

                    //set_values(0x11020020, 2, {3});
            }
            if(rack_online(rack_num))
            {
                // todo return if rack is not online            
                std::vector<int>test_results;
                std::vector<int>test_values;
                get_mem_int_val_id(test_id, 3, test_results);
                if(ops_debug)oss << " rack ["<< rack_num << "] test_results so far ";
                if(ops_debug)for (auto val : test_results)oss<<val<<" ";oss<<std::endl;  
                // pick a value for this rack
                setRackNum(src_id,rack_num); 
                get_mem_int_val_id(src_id, 3, test_values);

                if(ops_debug)oss << " rack ["<< rack_num << "] test_values so far ";
                if(ops_debug)for (auto val : test_values)oss<<val<<" ";oss<<std::endl;  
                // now we have the test_values in an array and the test_results in an array
                // if any test value is > 0 then set the test_result to that value 
                int vsize = test_results.size();
                for ( int i = 0 ; i < vsize; i++)
                {
                    if((test_results[i] == 0) &&(test_values[i] > 0))
                    {
                        test_results[i] = test_values[i];
                        set_mem_int_val_id(test_id, 3, test_results);
                    }
                }
            }
            if(rack_num == max_rack-1)
            {
                std::vector<int>test_results;
                get_mem_int_val_id(test_id, 3, test_results);

                if(ops_debug)oss << " rack final test results ";
                if(ops_debug)for (auto val : test_results)oss<<val<<" ";oss<<std::endl;  
                set_mem_int_val_id(item.dest_id, 3, test_results);
                set_value_debug = false;
            }
        
        }            
        break;
        default:
            oss << " Item method []"<< item.meth <<"] not registered\n";
    }
    return 0;
}
//   if(rack>=0)setRackNum(id, rack);
//     set_mem_int_val_id(id, 0, values);
// }


// void test_sim_mem()
// {
//     
void test_ops_run(std::map<std::string, OpsTable>& ops_tables, const std::string& ops_table, const std::string& op_desc, std::ostringstream& oss)
{
    int max_rack = 12;
    init_memory_simu();
    if (ops_tables.find(ops_table) != ops_tables.end())
    {
        oss << " Found ops table [" << ops_table <<"]"<< std::endl;

        for ( auto&item : ops_tables[ops_table].items)
        {
            oss << "Item desc ["<< item.desc <<"]" << std::endl;
            if( item.desc == op_desc) {
                oss << "Running Item desc ["<< item.desc <<"]" << std::endl;
                oss << "src [" << item.src<<"] dest [" << item.dest<<"]  lim [" << item.lim<<"] meth [" << item.meth<<"]\n"; 
                if (item.meth =="3sum")
                {
                    oss << "the plan is to get the src from all the racks and if one is set then the dest is to be set\n";
                    oss << "its a 3sum because after the initial offset we have two more to check\n";
                    for (int i = 0; i< max_rack ; i++)
                    {
                        run_ops_item(item,i, max_rack, oss);
                    }

                }
            }
        }
    }
}

void test_str_to_offset();
void test_decode_name();
void test_match_system();
void test_StringWords();
void test_decode_shmdef();

void test_data_list(ConfigDataList&configData);

//int test_modbus(const std::string& ip, int port );
int test_modbus();//const std::string& ip, int port );
int ops_parse(std::string filename ,std::ostringstream& oss)
{
    
    test_ops_parse(test_ops_tables, filename, oss);
    //test_read_array(jdata, filename, oss);
            // // Iterate through the JSON array
            // for (const auto& item : jdata) {
            //     // Check if 'src' key exists to avoid potential exceptions
            //     show_id(item, "src",oss );
            //     show_id(item, "dest",oss );
            //     show_id(item, "lim",oss );
            //     show_id(item, "meth",oss );
            //     show_id(item, "descr",oss );
            //     oss << std::endl;

            // }
    return 0;
}

int tparse(std::ostringstream& oss)
{
    //std::map<std::string, DataTable> test_data_tables;

    std::string data_file = "json/data_definition.json";

    test_json_parse(test_data_tables, data_file);
    return 1;
}

#include <fractal_test_unit_test.cpp>


int process_map_item(std::map<std::string, DataMap>& data_maps, const json &jitem) {
    if (!jitem.contains("map") || !jitem["map"].is_string()) {
        std::cerr << "Error: Missing or invalid 'table' field" << std::endl;
        return -1;
    }

    std::string map_name = jitem["map"];
    std::cout << __func__ <<" Map name [" << map_name << "]\n\n";

    if (!jitem.contains("items") || !jitem["items"].is_array()) {
        std::cerr << "Error: Missing or invalid 'items' array" << std::endl;
        return -1;
    }

    json jtable = jitem["items"];
    //std::cout << "Items Array found" << std::endl;
   bool isNewMap = data_maps.find(map_name) == data_maps.end();
    if (isNewMap) {
        data_maps[map_name] = DataMap(); // Uses the default constructor
    }

    // Create or access the DataTable for this table_name
    DataMap& dmap = data_maps[map_name];
    if (isNewMap) {
        // table.base_offset = 0;  // Set base_offset only if it's a new table
        // table.calculated_size = 0;  // Ensure calculated size starts at 0
    }

	int item_idx = 0;
     for (const auto& jitem : jtable) {
        if (!jitem.contains("name") ) {
            std::cerr << "Error: Missing name field in item idx [" << item_idx<<" ]"  << std::endl;
            continue;  // Skip this item or you might want to return an error
        }
		item_idx++;
		MapItem item;
        item.destname = jitem["name"];
        auto src = map_name + ":" + item.destname;
		item.destkey = shm_def_to_id(src);
        if((item.destkey &0xffff) == 0xffff)
        {
            std:: cout << ">>> Mapping error [" << src << "] bad key  [0x" << std::hex << item.destkey << std::dec <<"]";
            //exit (0);
        }
        else 
        {
            std:: cout << "Mapping OK  [" << src << "] found [0x" << std::hex << item.destkey << std::dec <<"]";
        }
        item.methodkey = 0xffff;
        item.srckey = 0xffff;
        item.optkey = 0xffff;
        
        if (jitem.contains("map") ) {
        	item.srcname = jitem["map"]; 
            auto src = item.srcname;
            if( src=="todo")
            {
                std:: cout << " map src [" << src << "] is undefined ";
            }
            else
            {
			    item.srckey = shm_def_to_id(src);
        	    item.method = "map";
        	    item.methodkey = 0;
                std:: cout << " map src [" << src << "] [0x" << std::hex << item.srckey << std::dec <<"] ";
            }
		}
        if (jitem.contains("opt") ) {
        	item.optname = jitem["opt"];
		    auto src = item.optname;
			item.optkey = shm_def_to_id(src);
            std:: cout << " option [" << src << "] [0x" << std::hex << item.optkey << std::dec <<"] ";
		}
		//"rtos:input:group_di 0"},
        if (jitem.contains("mapbit") ) {
        	item.srcname = jitem["mapbit"];
            auto src = item.srcname;
			item.srckey = shm_def_to_id(src);
        	item.method = "mapbit";
        	item.methodkey = 1;
            std:: cout << " map bit [" << src << "] [0x" << std::hex << item.srckey << std::dec <<"] ";
		}
		//"calc":"alarms"
        if (jitem.contains("calc") ) {
        	item.srcname = jitem["calc"];
            auto src = item.srcname;
			item.srckey = shm_def_to_id(src);
        	item.method = "calc";
        	item.methodkey = 2;
            std:: cout << " map calc [" << src << "] [0x" << std::hex << item.srckey << std::dec <<"] ";
		}
        if (jitem.contains("size") ) {
        	item.size = jitem["size"];
            std:: cout << " size [" << item.size << "] ";
		}
		else 
			item.size = 0;
        std::cout << "\n" ;


        dmap.sitems[item.destname] = item;
    }

    return 0;
}

int parse_data_maps(std::map<std::string, DataMap>& data_maps, const std::string &filename, const std::string &filedir)
{
	std::string fname = filedir + filename;
	std::ifstream file(fname);
	json jfile;
	if (!file.is_open()) {
		std::cout << "could not open data_table file ["<<fname<<"] no data table available " << std::endl;
		return -1;
	}

    std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    //std::cout << "File contents read: ->" << str << " <-" << std::endl;  // Debug output

    if (!file.is_open()) {
        std::cout << "Could not open data_map file [" << fname << "], not using embedded data instead." << std::endl;
        // try {
        //     jfile = json::parse(jsonData);
        // } catch (const std::exception& e) {
        //     std::cerr << "Exception parsing embedded JSON data: " << e.what() << std::endl;
        return -1;
        // }
    } else {
        try {
            jfile = json::parse(str);
            //file >> jfile;
        } catch (const std::exception& e) {
            std::cerr << "Exception parsing file JSON data: " << e.what() << std::endl;
            return -1;
        }
    }

	try {
		// Iterate over each element in the array
		for (const auto& item : jfile) {
			if (item.contains("map") && item["map"].is_string()) {
				process_map_item(data_maps, item);
			}
		}

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

	return 0;
}

int process_json_item(std::map<std::string, DataTable>& data_tables, const json &jitem) {
    if (!jitem.contains("table") || !jitem["table"].is_string()) {
        std::cerr << "Error: Missing or invalid 'table' field" << std::endl;
        return -1;
    }

    std::string table_name = jitem["table"];
    std::cout << __func__ <<" Table name [" << table_name << "]\n";

    if (!jitem.contains("items") || !jitem["items"].is_array()) {
        std::cerr << "Error: Missing or invalid 'items' array" << std::endl;
        return -1;
    }

    json jtable = jitem["items"];
    //std::cout << "Items Array found" << std::endl;
   bool isNewTable = data_tables.find(table_name) == data_tables.end();
    if (isNewTable) {
        data_tables[table_name] = DataTable(); // Uses the default constructor
    }

    // Create or access the DataTable for this table_name
    DataTable& table = data_tables[table_name];
    if (isNewTable) {
        table.base_offset = 0;  // Set base_offset only if it's a new table
        table.calculated_size = 0;  // Ensure calculated size starts at 0
    }

	int item_idx = 0;
     for (const auto& jitem : jtable) {
        if (!jitem.contains("name") || !jitem.contains("offset") || !jitem.contains("size")) {
            std::cerr << "Error: Missing required fields in item idx [" << item_idx<<" ]"  << std::endl;
            continue;  // Skip this item or you might want to return an error
        }
		item_idx++;
        std::string name = jitem["name"];
        int offset = jitem["offset"];
        int size = jitem["size"];
        std::string notes = jitem.value("notes", "");  // Optional notes field

		if(item_idx < 5)
			std::cout << " Data Item found, table  [" << table_name <<"] item name [" << name << "] offset [" << offset << "]" << std::endl;		
        DataItem item(name, offset, size);
        table.items.push_back(item);
        table.calculated_size += size;
    }

    return 0;
}


int parse_data_tables(std::map<std::string, DataTable>& data_tables, const std::string &filename, const std::string &filedir)
{
    //std::cout << " parsing data tables" << std::endl;

	std::string fname = filedir + filename;
    std::cout << " parsing data tables filename [" << fname <<"]"<< std::endl;
	std::ifstream file(fname);
	if (!file.is_open()) {
		std::cout << "could not open data_table file ["<<fname<<"] no data table available " << std::endl;
		return -1;
	}
	try {
	
		json jfile;
		file >> jfile;
		std::cout << " json loaded " << std::endl;


		if (jfile.is_array()){
			std::cout << " Array found " << std::endl;
		}

		// Iterate over each element in the array
		for (const auto& item : jfile) {
		// Check if "table" key exists in the JSON object
		    if(item.contains("Title"))
                std::cout << " Data Tables ["<<item["Title"]<<"]\n";
            if(item.contains("Release"))
                std::cout << " Release ["<<item["Release"]<<"]\n";
            if(item.contains("Date"))
                std::cout << " Date ["<<item["Date"]<<"]\n";

			if (item.contains("table") && item["table"].is_string()) {
				process_json_item(data_tables, item);
			}
		}

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

	return 0;
}

int process_test_item(std::map<std::string, TestTable>& test_tables, const json &jitem) {
    std::cout << __func__ <<" Test item\n";
    if (!jitem.contains("test") || !jitem["test"].is_string()) {
        std::cerr << "Error: Missing or invalid 'test' field" << std::endl;
        return -1;
    }
    std::cout << __func__ <<" Test item #2\n";

    std::string test_name = jitem["name"];
    std::cout << __func__ <<" Test name [" << test_name << "]\n";

    if (!jitem.contains("items") || !jitem["items"].is_array()) {
        std::cerr << "Error: Missing or invalid 'items' array" << std::endl;
        return -1;
    }
    std::string cat("summary.sbms.inputs");
    // if(jitem.contains("cat"))
    // {
    //     cat = jitem["cat"];
    // }

    json jtable = jitem["items"];
    //std::cout << "Items Array found" << std::endl;
   bool isNewTable = test_tables.find(test_name) == test_tables.end();
    if (isNewTable) {
        test_tables[test_name] = TestTable(); // Uses the default constructor
    }

    // Create or access the DataTable for this table_name
    TestTable& table = test_tables[test_name];
    if (isNewTable) {
        // table.base_offset = 0;  // Set base_offset only if it's a new table
        // table.calculated_size = 0;  // Ensure calculated size starts at 0
    }

	int item_idx = -1;
     for (const auto& jitem : jtable) {
        item_idx++;

        if (!jitem.contains("action") ||  !jitem.contains("desc") || !jitem.contains("data")) {
            if (!jitem.contains("note")) {
                std::cerr << "Error: Missing required fields in item idx [" << item_idx<<" ]"  << std::endl;
            }
            continue;  // Skip this item or you might want to return an error
        }
        std::string action = jitem["action"];
        // std::string dest = jitem["dest"];
        std::string desc = jitem["desc"];
        std::cout << " Test Item desc ["<< desc << "]"<< std::endl;
        std::vector<int> data = jitem["data"].get<std::vector<int>>();
//        std::string notes = jitem.value("notes", "");  // Optional notes field

		// if(item_idx < 5)
		// 	std::cout << " Data Item found, table  [" << table_name <<"] item name [" << name << "] offset [" << offset << "]" << std::endl;		
        TestItem item;
        item.action = action;
        item.cat = cat;
        //item.dest = dest;
        item.desc = desc;
        item.data = data;
        parse_repeat(item.repeat, jitem);

        table.items.push_back(item);
//        table.calculated_size += size;
    }

    return 0;
}


int parse_test_tables(std::map<std::string, TestTable>& data_tables, const std::string &filename, const std::string &filedir)
{

	std::cout <<"\n\n\n"<< __func__<< " parse test tables \n\n" << std::endl;

	std::string fname = filedir + filename;
	std::ifstream file(fname);
	if (!file.is_open()) {
		std::cout << "could not open data_table file ["<<fname<<"] no data table available " << std::endl;
		return -1;
	}
	try {
	
		json jfile;
		file >> jfile;
		std::cout << __func__<< " json loaded " << std::endl;


		if (jfile.is_array()){
			std::cout << __func__ << " Array found " << std::endl;
		}

		// Iterate over each element in the array
		for (const auto& item : jfile) {
		// Check if "table" key exists in the JSON object
		    if(item.contains("Title"))
                std::cout << " Data Tables ["<<item["Title"]<<"]\n";
            if(item.contains("Release"))
                std::cout << " Release ["<<item["Release"]<<"]\n";
            if(item.contains("Date"))
                std::cout << " Date ["<<item["Date"]<<"]\n";

			if (item.contains("test") && item["test"].is_string()) {

				process_test_item(data_tables, item);
			}
		}

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }

	return 0;
}

// Main Function
int main(int argc, char* argv[]) {
    std::ostringstream cmd;
 // Get the current process ID
    pid_t pid = getpid();

    // Construct the command to sleep, kill the current process, and restart
    cmd << "sleep 0.2 && kill " << pid << " && ";
    cmd << argv[0];  // assuming argv[0] contains the path to the executable

    // Append all original arguments
    for (int i = 1; i < argc; ++i) {
        cmd << " " << argv[i];
    }

    // Execute the command in the background
    cmd << " &";
    restart_string = cmd.str();


    std::ostringstream filename;
  
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test_plan>\n";
        return 1;
    }
    
    if (argc > 2) {

        std::string dataMap = argv[2];
        // deprecated
        // dataMap="data/sbmu_mapping.txt";
        // test_data_map(dataMap);

        // auto data_file = "src/data_definition.txt";
        // test_parse(data_tables, data_file) ;
        std::string data_table_file = "data_definition.json";
        std::string config_dir = "config/json/";

        parse_data_tables(data_tables, data_table_file, config_dir);
        std::cout  << " data tables parsed " << std::endl; 
        parse_data_maps(data_maps, data_table_file, config_dir);
        std::cout  << " data maps parsed " << std::endl; 

        std::string test_table_file = "test_definition.json";
        //std::string config_dir = "config/json/";

        parse_test_tables(test_tables, test_table_file, config_dir);
        // //return 0;
        // data_file = "json/data_defintion.json";
        // test_json_parse(data_tables, data_file) ;

        // Function to initialize the method dictionary
        initializeMethodDictionary();

        //register_trans();

        show_data_map();
        ConfigDataList configData;

        //test_data_list(configData);
            // // Generate reports

        test_str_to_offset();
        test_decode_name();
        test_match_system();
        test_StringWords();
        test_decode_shmdef();

        std::cout << "testing modbus ..." << std::endl;
        //test_modbus("192.168.86.209", 5000);
        mbx_client.connect(mb_connect); //("192.168.86.209", 502);

        test_modbus();

        std::cout << "\n\n**************************************"<<std::endl;
        assert_manager.generate_report();

        std::cout << "\n\n**************************************"<<std::endl;
        assert_manager.generate_summary();
        std::cout << "\n\n**************************************"<<std::endl;

        std::string transFile("json/system_trans.json");
        // Function to parse JSON and populate the structures
        parseTransJson(transFile);


        test_server(data_tables);


        // assert_manager.generate_summary();
        // assert_manager.generate_report();

        return 0;
    } 

    std::string testname = argv[1];

    auto test_run = get_test_run(testname);

    std::string url = "ws://192.168.86.209:9003";
    g_url =  url;

    filename << "json/"<< testname<<".json";

    json testPlan;
    load_test_plan(testPlan, testname);//filename.str()) ;
    run_test_plan(testPlan, testname, test_run);

    return 0;  
}
