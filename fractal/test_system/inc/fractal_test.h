/**
 ******************************************************************************
 * @Copyright (C), 2025, Fractal EMS Inc. 
 * @file: fractal_test.h
 * @author: Phil Wilshire
 * @Description:
 *     Fractal BMS Test System
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2025.01.23
 ******************************************************************************
 */
#ifndef __FRACTAL_TEST_H
#define __FRACTAL_TEST_H

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <algorithm>
#include <modbus/modbus.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct DataItem {
    std::string name;
    int offset;
    int size;
};
// struct DataItem {
//     std::string name;
//     int offset;
// }
struct DataTable {
    int base_offset;
    std::vector<DataItem> items;
    int calculated_size;
};

typedef std::map<std::string, DataTable> DataTables;

// Structs
struct InputVector {
    int offset;
    std::vector<uint16_t> data;
    double timestamp;
};

struct MatchObject {
    std::vector<uint16_t> vector;
    std::vector<uint16_t> mask;    // Mask for each element
    std::vector<uint16_t> tolerance;  // Tolerance for each element
    std::vector<uint16_t> weight;     // Weight for each element
    // std::vector<std::string> mask_defs;    // Mask  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
    // std::vector<std::string> tolerance_defs;    // tolerance  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
    // std::vector<std::string> weight_defs;    // weight  defs  "offset:value:count"  all strings , if we have no count then match all the remaining items
  
    std::map<int, std::vector<int>> matches;
    std::string name;
};


// map of variables
struct ConfigItem {
    std::string name;
    std::string system;
    std::string type;
    int offset;
    int size;
// ConfigItem item{name, currentSystem, currentType, currentOffset + offset, size};
    // Constructor for convenience
    ConfigItem(const std::string& n, const std::string& s, const std::string& t, int o, int sz)
         : name(n), system(s), type(t), offset(o), size(sz) {}
    ConfigItem(){};
};


// Hash Function for Vectors
struct VectorHash {
    std::size_t operator()(const std::vector<uint16_t>& vec) const {
        std::size_t hash = 0;
        for (const auto& val : vec) {
            hash ^= std::hash<uint16_t>()(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};



class ModbusClient {
private:
    modbus_t *ctx = nullptr;
    bool connected = false;

    void initializeModbusConnection(const std::string& ip, int port) {
        std::cout << " Modbus client opening " << std::endl;
        ctx = modbus_new_tcp(ip.c_str(), port);
        if (ctx == nullptr) {
            connected = false;
            throw std::runtime_error("Failed to create the Modbus context.");
        }

        if (modbus_connect(ctx) == -1) {
            modbus_free(ctx);
            connected = false;
            throw std::runtime_error("Failed to connect to the Modbus server.");
        }
        else
        {
            connected = true;
        }
    }


    void closeModbusConnection() {
        std::cout << " Modbus client  closing " << std::endl;
        if (ctx) {
            modbus_close(ctx);
            modbus_free(ctx);
            ctx = nullptr;
            connected = false;
        }
    }


    int decodeRackNumber(const std::string& sm_name) {
        std::regex rack_regex("rack_(\\d+)");
        std::smatch match;
        if (std::regex_search(sm_name, match, rack_regex) && match.size() > 1) {
            return std::stoi(match[1]);
        }
        return -1;
    }

 std::pair<int, std::vector<int>> processQuery(const json& query) {
        std::string action = query.value("action", "");
        std::string reg_type = query.value("reg_type", "");
        int offset = query.value("offset", 0);
        int num = query.value("num", 0);
        std::vector<int> data = query.value("data", std::vector<int>());

        int rack_number = decodeRackNumber(query.value("sm_name", ""));
        if (rack_number >= 0) {
            // Select the rack by writing to a specific register before actual query
            uint16_t rack_select[] = { static_cast<uint16_t>(rack_number) };
            modbus_write_registers(ctx, 500, 1, rack_select);  // Assuming register 500 is used to select the rack
        }

        if (action == "get") {
            std::vector<uint16_t> response(num);
            std::vector<uint8_t> response_bits(num);
            int res = -1;
            if (reg_type == "input") {
                res = modbus_read_input_registers(ctx, offset, num, response.data());
            } else if (reg_type == "hold") {
                res = modbus_read_registers(ctx, offset, num, response.data());
            } else if (reg_type == "bits") {
                res = modbus_read_bits(ctx, offset, num, response_bits.data());
            }

            if (res == -1) {
                std::cerr << "Failed to read: " << modbus_strerror(errno) << std::endl;
                return {-1, {}};
            } else {
                if (reg_type == "bits") {
                    std::vector<int> int_response(response_bits.begin(), response_bits.end());
                    return {0, int_response};
                }
                else
                {
                    std::vector<int> int_response(response.begin(), response.end());
                    return {0, int_response};
                }
            }
        } else if (action == "set") {
            int res = -1;
            if (reg_type == "hold") {
                res = modbus_write_registers(ctx, offset, num, reinterpret_cast<const uint16_t*>(data.data()));
            } else if (reg_type == "coil") {
                res = modbus_write_bit(ctx, offset, data[0]);  // Assuming single bit write for coils
            }

            if (res == -1) {
                std::cerr << "Failed to write: " << modbus_strerror(errno) << std::endl;
                return {-1, {}};
            } else {
                return {0, {}};
            }
        }
        return {-1, {}};
    }

public:
    ModbusClient() {}

    ModbusClient(const std::string& ip, int port) {
        initializeModbusConnection(ip, port);
    }

    ~ModbusClient() {
        closeModbusConnection();
    }
    bool is_connected() {
        return connected;
    }

    bool connect(const std::string& mb_connect)
    {
        // decode ip port from 192.168.86.209:5000
        size_t colonPos = mb_connect.find(':');
        if (colonPos == std::string::npos) {
            return false;
        }

        auto ip = mb_connect.substr(0, colonPos);
        auto port = std::stoi(mb_connect.substr(colonPos + 1));
        initializeModbusConnection(ip,port);
        return true;
        
    }

    std::pair<int, std::vector<int>> addQuery(const json& query) {
        return processQuery(query);
    }
 
    std::string run_mb(const std::string& query) {
        auto [status, data] = processQuery(query);
    
        // Convert the status and data into a JSON string
        json jsonResponse;
        jsonResponse["status"] = status;
        jsonResponse["data"] = data;

        return jsonResponse.dump();  // Convert JSON object to string
    }

};

// Global ofstream for logging
std::ofstream log_file;

// Macro to open log file
#define log_open(path) do { log_file.open(path, std::ios::out | std::ios::app); if (!log_file.is_open()) std::cerr << "Failed to open log file: " << path << std::endl; } while (0)

// Macro to log messages
#define log_msg if (!log_file) std::cerr << "Log file is not open!" << std::endl; else log_file

// Macro to close log file
#define log_close() if (log_file.is_open()) log_file.close()

// Macro to delete log file
#define log_delete(path) do { \
    if (std::filesystem::exists(path)) { \
        std::filesystem::remove(path); \
    } \
} while (0)


#endif