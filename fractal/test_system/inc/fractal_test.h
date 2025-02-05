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

// for new id_based stuff , its all in smname 
// Enums for mem_id
enum MemId {
    MEM_NONE = 0,
    MEM_SBMU = 1,
    MEM_RACK = 2,
    MEM_RTOS = 3,
    MEM_UNKNOWN = 4
};

// Enums for reg_id
enum RegId {
    REG_NONE = 0,
    REG_BITS = 1,
    REG_INPUT = 2,
    REG_HOLD = 3,
    REG_COIL = 4,
    REG_SM16 = 5,
    REG_SM8 = 6
};


//deprecated
enum class System {
    NONE, SBMU, RACK, RTOS, UNKNOWN
};

enum class RegType {
    NONE, BITS, INPUT, HOLD, COIL, SM16, SM8, UNKNOWN
};

// Convert string to enum for System and RegType
System stringToSystem(const std::string& str) {
    if (str == "sbmu") return System::SBMU;
    if (str == "rtos") return System::RTOS;
    if (str == "rack") return System::RACK;
    return System::UNKNOWN;
}

RegType stringToRegType(const std::string& str) {
    if (str == "bits") return RegType::BITS;
    if (str == "hold") return RegType::HOLD;
    if (str == "input") return RegType::INPUT;
    if (str == "coil") return RegType::COIL;
    if (str == "sm8") return RegType::SM8;
    if (str == "sm16") return RegType::SM16;
    return RegType::UNKNOWN;
}

// struct DataItem {
//     std::string name;
//     int offset;
//     int size;
// };
struct DataItem {
    std::string system_name;
    std::string reg_type_name;
    System system;
    RegType reg_type;
    std::string name;
    int offset;
    int size;
    uint32_t mykey;      // Unique numeric key
    std::vector<uint32_t> mappedkeys;  // Key for mapping to another DataItem

    DataItem(const std::string& name, int off, int sz)
    : name(name), offset(off), size(sz) {
    }

    // Parses type from "system_name:reg_type_name" and updates object properties
    void setType(const std::string& mytype) {
        std::istringstream iss(mytype);
        std::string token;
        std::vector<std::string> tokens;
        
        while (getline(iss, token, ':')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 2) {
            system_name = tokens[0];
            reg_type_name = tokens[1];
            system = stringToSystem(system_name);
            reg_type = stringToRegType(reg_type_name);

            // Optionally handle offset if provided
            if (tokens.size() > 2) {
                offset = std::stoi(tokens[2]);
            }
            computeKey();
        } else {
            std::cerr << "Invalid type format. Expected format 'system:reg_type[:offset]'" << std::endl;
        }
    }

    DataItem(const std::string& sys, const std::string& reg, const std::string& nm, int off, int sz)
    : system_name(sys), reg_type_name(reg), name(nm), offset(off), size(sz) {
        system = stringToSystem(sys);
        reg_type = stringToRegType(reg);
        computeKey();
    }

    void computeKey() {
        // Compute a unique key using system and reg_type enums and offset
        // Adjust multipliers as needed to ensure unique keys
        mykey = static_cast<uint32_t>((int)system<<28)+ static_cast<uint32_t>((int)reg_type << 24) + (offset&0xffff);
    }
};

// we  keep a record of data items
// but only if the items are mapped 
typedef std::unordered_map<uint32_t, std::shared_ptr<DataItem>> DataItemMap;

// used to map the src and destinations when sbmu has to set one or more values via a map
// [
//   { "src": "sbmu:hold:at_single_charge_temp_over", "dest": "rtos:hold:at_single_charge_temp_over", "size": 4, "method": "trans4" },
//   { "src": "sbmu:hold:at_single_charge_temp_under", "dest": "rtos:hold:at_single_charge_temp_under", "size": 4, "method": "trans4" }
// ]


struct MappingEntry {
    std::string src;
    std::string dest;
    int size;
    std::string method;  // thise mapping entries allow us to use a different method if needed
    uint32_t srckey;
    uint32_t destkey;
};

class DataItemManager {
private:
    DataItemMap items;
public:

    void addItem(const std::shared_ptr<DataItem>& item) {
        item->computeKey(); // Ensure key is calculated
        items[item->mykey] = item;
    }

    std::shared_ptr<DataItem>getItem(uint32_t key) {
        auto it = items.find(key);
        if (it != items.end()) {
            return it->second;
        }
        return nullptr; // Return nullptr if not found
    }

    void displayItems() {
        for (auto& pair : items) {
            std::cout << "Key: " << pair.first << ", Name: " << pair.second->name << std::endl;
        }
    }
};

struct OpsItem {
    std::string name;
    std::string src;
    std::string dest;
    std::string meth;
    std::string lim;
    std::string desc;
    uint32_t src_id;
    uint32_t dest_id;
    uint32_t lim_id;
    int  meth_id;

    OpsItem() {};
    OpsItem (
            const std::string& src,
            const std::string& dest,
            const std::string& meth,
            const std::string& lim,
             const std::string& desc)
    : src(src),dest(dest), meth(meth),lim(lim) , desc(desc)   
    {
        // todo turn all the items into id's
    };

    // // Parses type from "system_name:reg_type_name" and updates object properties
    // void setType(const std::string& mytype) {
    //     std::istringstream iss(mytype);
    //     std::string token;
    //     std::vector<std::string> tokens;
        
    //     while (getline(iss, token, ':')) {
    //         tokens.push_back(token);
    //     }

    //     if (tokens.size() >= 2) {
    //         system_name = tokens[0];
    //         reg_type_name = tokens[1];
    //         system = stringToSystem(system_name);
    //         reg_type = stringToRegType(reg_type_name);

    //         // Optionally handle offset if provided
    //         if (tokens.size() > 2) {
    //             offset = std::stoi(tokens[2]);
    //         }
    //         computeKey();
    //     } else {
    //         std::cerr << "Invalid type format. Expected format 'system:reg_type[:offset]'" << std::endl;
    //     }
    // }

    // DataItem(const std::string& sys, const std::string& reg, const std::string& nm, int off, int sz)
    // : system_name(sys), reg_type_name(reg), name(nm), offset(off), size(sz) {
    //     system = stringToSystem(sys);
    //     reg_type = stringToRegType(reg);
    //     computeKey();
    // }

    // void computeKey() {
    //     // Compute a unique key using system and reg_type enums and offset
    //     // Adjust multipliers as needed to ensure unique keys
    //     mykey = static_cast<uint32_t>((int)system<<28)+ static_cast<uint32_t>((int)reg_type << 24) + (offset&0xffff);
    // }
};


struct OpsTable {
    OpsTable(){};
    std::string name;
    //int base_offset;
    //uint32_t basekey;      // base_key derived from system + regtype but we may not use htis since we compose

    std::vector<OpsItem> items;
};

typedef std::map<std::string, OpsTable> OpsTables;



// struct DataItem {
//     std::string name;
//     int offset;
// }

struct DataTable {
    int base_offset;
    //uint32_t basekey;      // base_key derived from system + regtype but we may not use htis since we compose

    std::vector<DataItem> items;
    int calculated_size;
    DataTable(int current_offset) {
        base_offset =  current_offset;
        calculated_size = 0;
        items.clear();
    }
    DataTable() {
        base_offset = 0;
        calculated_size = 0;
        items.clear();
    }
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

struct TransItem;

struct MethStruct {
    std::string name;
    std::string desc;
    // Function signature for methods
    std::function<int(const TransItem&)> func;
};

bool decode_shmdef(MemId &mem_id, RegId &reg_id, int& rack_num, uint16_t &offset, const std::string &shmdef);

class ModbusClient {
public:

    std::string run_mbQuery(uint32_t id, int num , std::vector<int>values) {

    MemId mem_id = (MemId)((id & 0xF0000000) >> 28);
        RegId reg_id = (RegId)((id & 0x0F000000) >> 24);
        int rack_num = (int)((id & 0x0F000000)>>16);

        // uint16_t def_offset;


        // auto [status, data] =  processMbQuery(query); 
        std::ostringstream oss;
        // oss << "[ ";
        // for (size_t i = 0; i < data.size(); ++i) {
        //     oss << data[i];
        //     if (i < data.size() - 1) oss << " ";
        // }
        // oss << " ]";
        return oss.str();
    }

    modbus_t* get_ctx()
    {
        return ctx;
    }

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
        std::regex rtos_regex("rtos_(\\d+)");
        std::smatch match;
        if (std::regex_search(sm_name, match, rack_regex) && match.size() > 1) {
            return std::stoi(match[1]);
        }
        if (std::regex_search(sm_name, match, rtos_regex) && match.size() > 1) {
            return std::stoi(match[1]);
        }
        return -1;
    }


// todo 
// we want to decode into an id rack:hold:offset[:rack_no]
//"mbget sbms:bits:123 4"
 //qstring [{"action":"get", "sec":202, "sm_name":"0x11000001", "reg_type":"any", "offset":"1", "num":2, "data":[0]}]
 //qstring json ["{\"action\":\"get\", \"sec\":202, \"sm_name\":\"0x11000001\", \"reg_type\":\"any\", \"offset\":\"1\", \"num\":2, \"data\":[0]}"]
 std::pair<int, std::vector<int>> processMbQuery(const json& query) {
      //std::cout << "running processMbQuery #1\n";
        std::string action = query.value("action", "");
        std::string reg_type = query.value("reg_type", "");
        int offset = 0;// = query.value("offset", 0);
      //std::cout << "running processMbQuery #1a\n";
        int num = query.value("num", 0);
      //std::cout << "running processMbQuery #1b\n";
        std::vector<int> data;;
        if (query.contains("data") && query["data"].is_array()) {
            data = query["data"].get<std::vector<int>>();
        } else { 
            data = {0};
        }
      //std::cout << "running processMbQuery #3\n";
        MemId mem_id;
        RegId reg_id;
        int rack_num;
        uint16_t def_offset;
        std::string shmdef = query.value("sm_name", "");
        bool use_id = decode_shmdef(mem_id, reg_id, rack_num, def_offset, shmdef);
        if(use_id)
        {
            std::cout << " after decode_shmdef  use_id true ["<<shmdef <<"]"<<std::endl;
        } else {
            std::cout << " after decode_shmdef use_id false ["<<shmdef <<"]"<<std::endl;
        }
        int rack_number = 0;

        if(!use_id)
        {
            std::cout << " use_id false ; decoding rack number from ["<<shmdef <<"]"<<std::endl;
            rack_number = decodeRackNumber(shmdef);
        }
        else
        {
            std::cout << " use_id true ; using  rack num [" << rack_num <<"] from ["<<shmdef <<"]"<<std::endl;
            rack_number = rack_num;
        }
        if (rack_number >= 0) {
            // Select the rack by writing to a specific register before actual query
            uint16_t rack_select[] = { static_cast<uint16_t>(rack_number) };
            modbus_write_registers(ctx, 500, 1, rack_select);  // Assuming register 500 is used to select the rack
        }
        if(!use_id)
        {
            if (reg_type == "input") {
                reg_id = REG_INPUT;
            }
            else if (reg_type == "bits") {
                reg_id = REG_BITS;
            }
            else if (reg_type == "hold") {
                reg_id = REG_HOLD;
            }
            else if (reg_type == "coil") {
                reg_id = REG_COIL;
            }
        }
        else
        {
            offset = def_offset;
        }

        if (action == "get") {
            std::vector<uint16_t> response(num);
            std::vector<uint8_t> response_bits(num);
            int res = -1;
            //if (reg_id ==  == "input") {
            if (reg_id ==  REG_INPUT) {
                res = modbus_read_input_registers(ctx, offset, num, response.data());
            //} else if (reg_type == "hold") {
            } else if (reg_id == REG_HOLD) {
                res = modbus_read_registers(ctx, offset, num, response.data());
            //} else if (reg_type == "bits") {
            } else if (reg_id == REG_BITS) {
                res = modbus_read_bits(ctx, offset, num, response_bits.data());
            }

            if (res == -1) {
                std::cerr << "Failed to read: " << modbus_strerror(errno) << std::endl;
                return {-1, {}};
            } else {
                //if (reg_type == "bits") {
                if (reg_id == REG_BITS) {
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
            //if (reg_type == "hold") {
            if (reg_id == REG_HOLD) {
                res = modbus_write_registers(ctx, offset, num, reinterpret_cast<const uint16_t*>(data.data()));
            //} else if (reg_type == "coil") {
            } else if (reg_id == REG_COIL) {
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
        return processMbQuery(query);
    }
 
    std::string run_mb(const std::string& query) {
        auto [status, data] = processMbQuery(query);
    
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

// transfer items 
// // Structure definition
struct TransItem {
    std::string src_str;
    std::string meth_str;
    std::string dest_str;
    std::string lim_str;    // where we find the limits if there are any , used in the 3lim method 
    std::string name;
    uint32_t src_id;
    uint32_t dest_id;
    uint32_t lim_id;
};

struct TransItemTable {
    std::string name;
    std::string desc;
    std::vector<TransItem> items;    
};

struct QueryTest {
    std::string query;
    std::string desc;
    std::string resp;
};

enum MethId {
    METH_UNKNOWN,
    METH_3SUM,
    METH_SUM,
    METH_SUM2,
    METH_INTO,
    METH_3LIMMAX,
    METH_3LIMMIN,
    METH_MAX,
    METH_MIN,
    METH_COUNT
};

int meth_str_to_id(const std::string& meth_str, const std::string& desc) {
    if (meth_str == "3sum") return (int)METH_3SUM;
    if (meth_str == "into") return (int)METH_INTO;
    if (meth_str == "3lim_max") return (int)METH_3LIMMAX;
    if (meth_str == "3lim_min") return (int)METH_3LIMMIN;
    if (meth_str == "max") return (int)METH_MAX;
    if (meth_str == "min") return (int)METH_MIN;
    std::cout << "Undefined method ["<< meth_str <<"] operation ["<< desc <<"] will be ignored"<<std::endl;
    return METH_UNKNOWN;
};


// Structure definition
struct ModbusTransItem {
    std::string src_str;
    uint32_t src_id;
    std::string meth_str;
    uint32_t meth_id;
    std::string dest_str;
    uint32_t dest_id;
    std::string lim_str;
    uint32_t lim_id;
    std::string name;
};

#endif