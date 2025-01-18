/**
 ******************************************************************************
 * @Copyright (C), 2025, Fractal EMS Inc. 
 * @file: fractal_sequence.cpp
 * @author: Phil Wilshire
 * @Description:
 *     System  Sequence processor
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2025.01.16
 ******************************************************************************
 */
/* 
  system bms ( and rack bms) sequence handling 
*/

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <cassert>
#include <random>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>


#include <nlohmann/json.hpp>

// Namespace shortcuts
namespace fs = std::filesystem;
using json = nlohmann::json;

bool key_lim_debug = false;

// TODO put std::any in the vardefs  .. done
//       associate local with rack   done
//       fire up entry and exit steps  
//       test the agg done     
//       cond in an agg     done
// fix uninit variable 
// fix bug on undecoded oper done
//       create max/min_alarms    done except the transfer to the bits
//       create min_alarm3
//       test cond running a trigger
//       test cond running a sequence
//       test trigger running a sequence
// fix the rnum feature

// hack (g)vars to become sm locations
// hack mb vars to become sm locations

// step vars can be numeric 
// read from file
// insert dynacally

// more interesting steps
        // {"oper": "agg", "src": "rinput:1", "dest": "g_rack_voltage",   "reset":true,   "desc": "Calculate max/min/total/count voltage"},
        // {"oper": "agg", "src": "rinput:1", "dest": "g_rack_voltage",     "desc": "Calculate max/min/total/count voltage"},
        // {"oper": "maxl", "src": "rinput:1", "var": "rhold:2", "dest":"bits:12",    "desc": "Set level 1,2,3 max_alarm"},
        // {"oper": "minl", "src": "rinput:1", "var": "rhold:5", "dest":"bits:15",    "desc": "Set level 1,2,3 min_alarm"},

// sample json strings


std::string json_string = R"({
    "trigger_name": "test_trigger",
    "aggregate": "each",
    "src_ref": "bits:3",
    "oper": "and",
    "conditions": [
        {"src": "rinput:3", "oper": ">", "value": 300},
        {"src": "bits:5", "oper": "=", "value": 1}
    ],
    "run": "compound_sequence"
})";

std::string rack_online = R"(
{
    "trigger_name": "rack_online",
    "xaggregate": "each",
    "src_ref": "bits:3",
    "oper": "and",
    "conditions": [
        {"src": "rinput:3", "oper": ">", "value": 300},
        {"src": "bits:5", "oper": "=", "value": 1}
    ],
    "xrun": "compound_sequence"
}
)";



//#### **Compound Trigger**
std::string json_string2 = R"({
    "src_ref": "bits:3",
    "oper": "and",
    "conditions": [
        {"src": "rinput:3", "oper": ">", "value": 300},
        {"src": "bits:5", "oper": "=", "value": 1}
    ],
    "run": "compound_sequence"
})";

//  this will reset and run cooling sequnece  if all the oper conditipns aer met  
std::string json_string3 = R"(
[
    { "src_ref": "bits:8", "oper": "reset_run", "dest_reg": "cooling_sequence", "value": 0},
    { "oper": "and",
        "conditions": [
            {"src_ref": "rinput", "src_reg": 20, "oper": "gt", "value": 80},
            {"src_ref": "rinput", "src_reg": 25, "oper": "lt", "value": 50}
        ],
        "run_sequence": "cooling_sequence"
    },
    { "oper": "or",
        "conditions": [
            {"src_ref": "bits", "src_reg": 10, "value": 1},
            {"src_ref": "bits", "src_reg": 11, "value": 1}
        ],
        "run_sequence": "emergency_shutdown"}
])";


std::string json_string4 = R"({
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 101, "dest": "rbits:1",            "desc": "Start fan 1"},
        {"oper": "wait_reset", "counter": "lcl:wait1", "time": 100, "desc": "Wait 100 cycles"},
        {"oper": "set", "value": 102, "dest": "rbits:2",            "desc": "Start fan 2"},
        {"oper": "wait_reset", "counter": 0, "time": 100,           "desc": "Wait 100 cycles"},
        {"oper": "set", "src": "bits:40", "dest": "rbits:1",        "desc": "Stop fan 1"},
        {"oper": "set", "value": 0, "dest": "rbits":2,              "desc": " Stop fan 2"}
    ]
}
)";

// add oper while ( with time)
// add prebuilt functions with args ( rack_num lvars valr)

std::string seq_config = R"(
[
    {
    "sequence_name": "test_sequence",
    "step": 0,
    "steps": [
        {"oper": "inc",         "src":"rhold:5", "dest":"rhold:5",   "value": 10,    "desc":"inc a value by 10"},
        {"oper": "set", "src": "bits:4", "dest": "rbits:5",                          "desc":"Send shutdown signal"},
        {"oper": "wait_reset",  "counter": "lvar:count1",      "time": 3,            "desc":"Wait for confirmation"},
        {"oper": "set",        "dest": "rbits:5","value": 0,                         "desc":"Clear shutdown signal"},
        {"oper": "jump_if", "cond":"<", "src":"rhold:5", "value": 10, "jump_to":0,   "desc":"Repeat on condition"}
        ]
    },

    {
    "sequence_name": "emergency_shutdown",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 133, "dest":"rbits:5",                    "desc":"Send shutdown signal"},
        {"oper": "set", "src": "bits:4", "dest": "rbits:5",                "desc":"Send shutdown signal"},
        {"oper": "wait_reset", "counter": "lvar:count1", "time": 300,      "desc":"Wait for confirmation"},
        {"oper": "set", "value": 0, "dest": "rbits:5",                     "desc":"Clear shutdown signal"}
        ]
    },
    {
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set",        "value": 101,          "dest": "rbits:1",                  "desc":"Start fan 1"},
        {"oper": "wait_reset",  "counter": "wait1",   "time": 100,                       "desc":"Wait 100 cycles"},
        {"oper": "set",        "value": 102,          "dest": "rbits:2",                 "desc":"Start fan 2"},
        {"oper": "wait_reset", "counter": "wait1",    "time": 100,                       "desc":"Wait 101 cycles"},
        {"oper": "set", "src": "bits:40",             "dest": "rbits:1",                 "desc":"Stop fan 1"},
        {"oper": "set", "value": 0,                   "dest": "rbits:2",                 "desc":"Stop fan 2"}
        ]
    }
]
)";

// aggregate all on line racks

std::string test_agg = R"(
[
    {
    "sequence_name": "test_agg",
    "step": 0,
    "steps": [
        {"oper": "agg", "src": "rinput:1", "dest": "g_rack_voltage", "xcond":"rack_online",       "desc": "Calculate max/min/total/count voltage"},
        {"oper": "agg", "src": "rinput:2", "dest": "g_rack_current", "xcond":"rack_online",       "desc": "Calculate max/min/total/count current"},
        {"oper": "agg", "src": "rinput:2", "dest": "g_online_current", "cond":"rack_online",      "desc": "Calculate max/min/total/count current"}
        ],
    "exit": [
        {"oper": "set", "src": "g_rack_voltage_min",     "dest": "input:1",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_min_num", "dest": "input:2",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_max",     "dest": "input:3",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_max_num", "dest": "input:4",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_min",     "dest": "input:5",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_min_num", "dest": "input:6",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_max",     "dest": "input:7",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_max_num", "dest": "input:8",    "desc": "transfer to sbms modbus regs"}
        ]
    }
]
)";

std::string test_max = R"(
[
    {
    "sequence_name": "test_max",
    "step": 0,
    "steps": [
        {"oper": "maxl",    "src": "rinput:1", "dest": "g_high_volt_1", "limit":"g_volt_max", "xcond":"rack_online",       "desc": "test for high volt numerical limit"},
        {"oper": "maxl",    "src": "rinput:1", "dest": "g_high_volt_2", "limit":"hold:11",    "xcond":"rack_online",          "desc": "test for high volt config item"},
        {"oper": "minl",    "src": "rinput:1", "dest": "g_low_volt_1",  "limit":2400,          "xcond":"rack_online",          "desc": "test for low volt config item"},
        {"oper": "minl",    "src": "rinput:1", "dest": "g_low_volt_1", "limit":"g_volt_min",  "xcond":"rack_online",          "desc": "test for low volt config item"},
        {"oper": "minl",    "src": "rinput:1", "dest": "g_low_volt_2", "limit":"hold:12",     "xcond":"rack_online",          "desc": "test for low volt config item"}
        ],
    "xexit": [
        {"oper": "set", "src": "g_rack_voltage_min",     "dest": "input:1",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_min_num", "dest": "input:2",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_max",     "dest": "input:3",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_voltage_max_num", "dest": "input:4",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_min",     "dest": "input:5",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_min_num", "dest": "input:6",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_max",     "dest": "input:7",    "desc": "transfer to sbms modbus regs"},
        {"oper": "set", "src": "g_rack_current_max_num", "dest": "input:8",    "desc": "transfer to sbms modbus regs"}
        ]
    }
]
)";


class AssertManager {
public:
    struct AssertInfo {
        std::string category;
        std::string message;
        bool passed;
    };

    std::vector<AssertInfo> assert_log;

    // Log an assertion
    void log_assertion(const std::string& category, const std::string& message, bool passed) {
        assert_log.push_back({category, message, passed});
    }

    // Generate a summary
    void generate_summary() const {
        int total = assert_log.size();
        int passed = 0;
        for (const auto& assert : assert_log) {
            if (assert.passed) passed++;
        }
        std::cout << "\n--- Test Summary ---\n";
        std::cout << "Total: " << total << ", Passed: " << passed << ", Failed: " << (total - passed) << "\n";
    }

    // Generate a detailed report
    void generate_report() const {
        std::cout << "\n--- Detailed Test Report ---\n";
        for (const auto& assert : assert_log) {
            std::cout << "[" << (assert.passed ? "PASS" : "FAIL") << "] "
                      << "Category: " << assert.category
                      << " | Message: " << assert.message << "\n";
        }
    }
};

// Global instance for assertion management
AssertManager assert_manager;

#define myassert(condition, category, crash)                               \
    do {                                                                   \
        bool result = (condition);                                         \
        assert_manager.log_assertion(category, #condition, result);        \
        if (!result) {                                                     \
            std::cerr << "[ASSERT FAIL] Category: " << category            \
                      << " | Condition: " << #condition << std::endl;      \
            if (crash) {                                                   \
                std::cerr << "Exiting due to failed assertion.\n";         \
                std::terminate();                                          \
            }                                                              \
        }                                                                  \
    } while (0)

int run_asseert( bool crash) {
    int v1 = 20, v2 = 30;

   // bool crash = false;

    // Assertions
    myassert(v1 > 24, "object.vars.set_value_with_rack", crash);
    myassert(v2 == 30, "object.vars.check_initial_value", crash);
    myassert(v1 + v2 > 50, "object.vars.sum_check", crash);
    myassert(v1 < v2, "object.vars.compare_values", crash);

    // Generate reports
    assert_manager.generate_summary();
    assert_manager.generate_report();

    return 0;
}

struct Step {
    std::string oper;             // Operation type (e.g., "set", "inc", "jump_if", "wait_reset", etc.)
    std::string src;              // Source reference (e.g., "bits:3")
    std::string dest;             // Destination reference (e.g., "rbits:5")
    std::string var;              // var used for add multiply  divide etc (e.g., "rbits:5")
    std::string cond;             // Condition for operations like "jump_if" (e.g., ">", "==")
    int idx;
    int value = 0;                // Integer value for operations
    int jump_to = -1;             // Target step index for jump operations
    int time = 0;                 // Time value in milliseconds for "wait_reset"
//    int counter_reg = -1;         // Counter register for "wait_reset" or similar operations
    std::string counter;

    std::map<int, std::any> step_map;
    std::map<int, std::any> var_map;


    // Default Constructor
    Step() = default;


    // Constructor for 'wait_reset' operation
    //Step(std::string counter_reg, int time)
    //    : oper("wait_reset"), counter(counter_reg), time(time) {}


    // Constructor for fully-specified step
    Step(const std::string& oper, const std::string& dest, int value = 0, const std::string& cond = "",
         int time = 0, std::string counter = "", int jump_to = 0)
        : oper(oper), dest(dest), value(value), cond(cond), time(time), counter(counter), jump_to(jump_to) {}

};


struct Sequence {
    std::string name;                  // Sequence name
    std::map<int, int> current_steps;       // Per-rack current step
    std::map<int, bool> running_states;     // Per-rack running statenning
    std::map<int, std::map<int, int>> counters; // Counters per rack and counter register

    // sm:<rack><name>:<offset>:<value>
    std::map<int, std::map<std::string, int>> sq_vals;

    std::vector<Step> steps;           // List of steps in the sequence
    std::vector<Step> entry;           // List of entry steps in the sequence
    std::vector<Step> exit;            // List of exit steps in the sequence
};

enum MbRegs {
    MB_NONE = 0,
    MB_BITS,
    MB_HOLD,
    MB_INPUT,
    MB_COIL
};

enum StepKeys {
    KEY_OPER = 0,
    KEY_SRC,
    KEY_DEST,
    KEY_VAR,
    KEY_LIM,
    KEY_COND,
    KEY_COUNTER,
    KEY_RNUM,
    KEY_INC,
    KEY_IDX,
    KEY_ADD,
    KEY_DIV,
    KEY_VALUE,
    KEY_JUMP_TO,
    KEY_TIME,
    KEY_OPER_STR,
    KEY_OPER_ADD,
    KEY_OPER_DIV,
    KEY_OPER_MUL,
    KEY_OPER_SET,
    KEY_OPER_INC,
    KEY_OPER_WAIT,
    KEY_OPER_JUMP_IF,
    KEY_OPER_JUMP_NOT,
    KEY_OPER_AGG,
    KEY_OPER_MAX,
    KEY_OPER_MIN,
    KEY_OPER_NOOP,
    KEY_SRC_STR,
    KEY_DEST_STR,
    KEY_VAR_STR,
    KEY_LIM_STR,
    KEY_COND_STR,
    KEY_COUNTER_STR,
    KEY_RNUM_STR,
    KEY_INC_STR,
    KEY_END
};

enum VarType {
    VAR_MODBUS_NONE,
    VAR_MODBUS_BITS,
    VAR_MODBUS_HOLD,
    VAR_MODBUS_INPUT,
    VAR_MODBUS_COIL,
    MODBUS_MAX,
    VAR_MODBUS_RACK_NONE,
    VAR_MODBUS_RACK_BITS,
    VAR_MODBUS_RACK_HOLD,
    VAR_MODBUS_RACK_INPUT,
    VAR_MODBUS_RACK_COIL,
    MODBUS_RACK_MAX,
    VAR_RACK_MEM,
    VAR_RTOS_MEM,
    VAR_SBMU_MEM,
    VAR_GLOBAL_VAR,
    VAR_LOCAL_VAR,
    VAR_INT_VAR,
    VAR_DOUBLE_VAR,
    VAR_UNKNOWN
};


#ifndef MAX_RACK
#define MAX_RACK 20
#endif


// Map to store directory names, file names, and their JSON data
std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, json>>> json_map;



void load_json_files(const std::string& root_dir) {
    try {
        // Iterate over subdirectories in the root directory
        for (const auto& dir_entry : fs::directory_iterator(root_dir)) {
            if (dir_entry.is_directory()) {
                std::string dir_name = dir_entry.path().filename().string();

                // Iterate over files in the subdirectory
                for (const auto& file_entry : fs::directory_iterator(dir_entry)) {
                    if (file_entry.is_regular_file() && file_entry.path().extension() == ".json") {
                        std::string filename = file_entry.path().filename().string();
                        try {
                            // Read and parse the JSON file
                            std::ifstream file(file_entry.path());
                            json parsed_json = json::parse(file);

                            // Store JSON data in the map
                            json_map[dir_name][filename] = parsed_json;
                            std::cout << "Loaded JSON file: " << filename << " in directory: " << dir_name << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "Failed to parse JSON file: " << filename
                                      << " in directory: " << dir_name
                                      << ". Error: " << e.what() << std::endl;
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error while scanning directories: " << e.what() << std::endl;
    }
}

void print_json_map() {
    for (const auto& [dir_name, files] : json_map) {
        std::cout << "Directory: " << dir_name << std::endl;
        for (const auto& [filename, json_data] : files) {
            std::cout << "  Filename: " << filename << std::endl;
            //std::cout << "  JSON Data: " << json_data.dump(2) << std::endl;
        }
    }
}

int test_file_load() {
    const std::string root_dir = "./json"; // Root directory to scan
    load_json_files(root_dir);
    print_json_map();
    return 0;
}

bool find_json_map(json& result, const std::string& dir_name, const std::string& file_name) {
    // Check if the directory exists
    auto dir_it = json_map.find(dir_name);
    if (dir_it == json_map.end()) {
        std::cerr << "Directory not found: " << dir_name << std::endl;
        return false;
    }

    // Check if the file exists in the directory
    const auto& file_map = dir_it->second;
    auto file_it = file_map.find(file_name);
    if (file_it == file_map.end()) {
        std::cerr << "File not found in directory: " << file_name << " in " << dir_name << std::endl;
        return false;
    }

    // Return the JSON content
    result = file_it->second;
    return true;
}

// // Attempt to find and retrieve the JSON data
//     if (find_json_map(j, "test", "test_rack_online.json")) {
//         std::cout << "Found JSON: " << j.dump(4) << std::endl;
//     } else {
//         std::cout << "Entry not found." << std::endl;
//     }
// {
// // Attempt to find and retrieve the JSON data
//     if (find_json_map(j, "test", "test_rack_online.json")) {
//         std::cout << "Found JSON: " << j.dump(4) << std::endl;
//     } else {
//         std::cout << "Entry not found." << std::endl;
//     }
// }
int rack_max = 12;

struct VarDef {
    VarType type;
    int offset;
    // int int_value;
    // double double_value;
    std::any value;

    std::string name; // Only used for named variables like global_var or local_var
};


// TODO move these to the sequence
// we need rack local and sequence local perhaps
std::map<std::string, int> local_vars;
std::map<std::string, int> global_vars;
std::unordered_map<std::string, Sequence> sequence_map;
std::map<std::string, json>saved_triggers_map;


// dynamic modbus registers
// modbus:<type>:<offset>:<value>
std::map<int, std::map<int, int>> mb_regs;
// modbus:<rack><type>:<offset>:<value>
std::map<int, std::map<int ,std::map<int, int>>> mb_rack_regs;
// dynamic sm registers

// sm:<name>:<offset>:<value>
std::map<std::string, std::map<int, int>> sm_regs;
// sm:<rack><name>:<offset>:<value>
std::map<int, std::map<std::string ,std::map<int, int>>> sm_rack_regs;

// TODO these are all ints for now
// Global and local variable storage
std::unordered_map<std::string, int> global_var_index;
std::vector<std::any> global_var_vec;
// Global and local variable storage
std::unordered_map<std::string, int> local_var_index;

std::vector<std::vector<std::any>> local_var_vec(MAX_RACK+1);  //allow for all including all racks (-1)

//std::unordered_map<std::string, int> local_vars;
uint16_t shm_rack_mem[MAX_RACK][4096];
uint16_t shm_rtos_mem[MAX_RACK][4096];
uint16_t mb_map_vals[MODBUS_MAX][4096];
uint16_t mb_rack_vals[MAX_RACK][MODBUS_MAX][4096];

// Modbus function code labels for display
const std::string modbus_labels[MODBUS_MAX+1] = {"NONE", "BITS", "HOLD", "INPUT", "COIL"};



// two ways of doing this , with a name or a json object
bool evaluate_trigger_name(std::vector<bool>& results, const std::string name, int rack_num = -1);
bool evaluate_trigger(std::vector<bool>& results, const json& trigger, int rack_num = -1);
void start_sequence(const std::string& sequence_name, int rack_num = -1);
void stop_sequence(const std::string& sequence_name, int rack_num = -1);

void print_any_value(const std::any& value);
bool evaluate_condition(const std::string& oper, int src_val, int value);
// Decode the variable reference string into a `VarDef`
int decode_var_type(VarDef &def, const std::string& var_ref);
bool set_sequence_state(std::string name, int state, int step, int rack_num =-1);


std::string var_def_to_str(const VarDef& var_def) {
    std::ostringstream oss;

    // Map VarType to string
    static const std::unordered_map<VarType, std::string> var_type_map = {
        {VAR_MODBUS_NONE, "MODBUS_NONE"},
        {VAR_MODBUS_BITS, "MODBUS_BITS"},
        {VAR_MODBUS_HOLD, "MODBUS_HOLD"},
        {VAR_MODBUS_INPUT, "MODBUS_INPUT"},
        {VAR_MODBUS_COIL, "MODBUS_COIL"},
        {VAR_MODBUS_RACK_NONE, "MODBUS_RACK_NONE"},
        {VAR_MODBUS_RACK_BITS, "MODBUS_RACK_BITS"},
        {VAR_MODBUS_RACK_HOLD, "MODBUS_RACK_HOLD"},
        {VAR_MODBUS_RACK_INPUT, "MODBUS_RACK_INPUT"},
        {VAR_MODBUS_RACK_COIL, "MODBUS_RACK_COIL"},
        {VAR_RACK_MEM, "RACK_MEM"},
        {VAR_RTOS_MEM, "RTOS_MEM"},
        {VAR_SBMU_MEM, "SBMU_MEM"},
        {VAR_GLOBAL_VAR, "GLOBAL_VAR"},
        {VAR_LOCAL_VAR, "LOCAL_VAR"},
        {VAR_INT_VAR, "INT_VAR"},
        {VAR_DOUBLE_VAR, "DOUBLE_VAR"},
        {VAR_UNKNOWN, "UNKNOWN"}
    };

    oss << "VarDef { ";
    oss << "type: " << var_type_map.at(var_def.type) << ", ";
    oss << "offset: " << var_def.offset << ", ";
    oss << "name: " << var_def.name << ", ";

    // Add value depending on type
    if (var_def.value.has_value()) {
        try {
            if (var_def.type == VAR_INT_VAR) {
                oss << "value: " << std::any_cast<int>(var_def.value);
            } else if (var_def.type == VAR_DOUBLE_VAR) {
                oss << "value: " << std::any_cast<double>(var_def.value);
            } else {
                oss << "value: " << "Stored type: " << var_def.value.type().name();
            }
        } catch (const std::bad_any_cast& e) {
            oss << "value: [Bad any_cast: " << e.what() << "]";
        }
    } else {
        oss << "value: [none]";
    }

    oss << " }";
    return oss.str();
}


void set_modbus_vars() {

    for (int fc = 0; fc < MODBUS_MAX; ++fc) {
        std::cout << "Function Code [" << modbus_labels[fc] << "]:" << std::endl;

        for (int reg = 0; reg < 4096; ++reg) {
            mb_map_vals[fc][reg] = 0;
        }
    }
}


void print_modbus_vars() {
    return;
    std::cout << "Modbus Variables:" << std::endl;

    for (int fc = 0; fc < MODBUS_MAX; ++fc) {
        std::cout << "Function Code [" << modbus_labels[fc] << "]:" << std::endl;

        bool has_values = false;
        for (int reg = 0; reg < 4096; ++reg) {
            if (mb_map_vals[fc][reg] != 0) { // Print only non-zero values for brevity
                std::cout << "  Register " << reg << " = " << mb_map_vals[fc][reg] << std::endl;
                has_values = true;
            }
        }

        if (!has_values) {
            std::cout << "  No values set for this function code." << std::endl;
        }
    }

    std::cout << "End of Modbus Variables" << std::endl;

}

// Function to strip quotes from a string
std::string strip_quotes(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

int myrand2(int low, int high) {
    return low + std::rand() % (high - low + 1);
}

int init_rand() {
    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    //int rval = myrand(2000, 4000);
    //std::cout << "Random value: " << rval << std::endl;
    return 0;
}


int myrand(int low, int high) {
    // Create a random device to seed the random number generator
    std::random_device rd;

    // Create a Mersenne Twister random number generator
    std::mt19937 gen(rd());

    // Define a uniform integer distribution within the range [low, high]
    std::uniform_int_distribution<int> dist(low, high);

    return dist(gen);
}


// Helper function to get context index for `rack_num`
inline int get_context_index(int rack_num) {
    if (rack_num < -1 || rack_num > MAX_RACK) {
        throw std::out_of_range("Invalid rack_num: must be between -1 and " + std::to_string(MAX_RACK));
    }
    return rack_num + 1; // -1 maps to 0, 0 maps to 1, etc.
}

// Define a local variable with an optional index
template <typename T>
int def_local_var(const std::string& name, int idx = -1) {
    if (local_var_index.find(name) != local_var_index.end()) {
        throw std::runtime_error("Variable name '" + name + "' already defined.");
    }

    if (idx == -1) {
        idx = local_var_index.size();
    }
    local_var_index[name] = idx;

    // Ensure all contexts have space for this variable
    for (auto& context : local_var_vec) {
        if (idx >= context.size()) {
            context.resize(idx + 1);
        }
        context[idx] = T{}; // Default-initialize the variable
    }
    return idx;
}


// Set a local variable by name
template <typename T>
void set_local_var(const std::string& name, T value, int rack_num = -1) {
    int context_idx = get_context_index(rack_num);
    auto it = local_var_index.find(name);
    if (it == local_var_index.end()) {
        throw std::runtime_error("Variable name '" + name + "' not found.");
    }
    int idx = it->second;

    if (idx >= local_var_vec[context_idx].size()) {
        throw std::out_of_range("Variable index out of range.");
    }

    local_var_vec[context_idx][idx] = value;
}

// Set a local variable by index
template <typename T>
void set_local_var(int idx, T value, int rack_num = -1) {
    int context_idx = get_context_index(rack_num);

    if (idx >= local_var_vec[context_idx].size()) {
        throw std::out_of_range("Variable index out of range.");
    }

    local_var_vec[context_idx][idx] = value;
}


// Get a local variable
template <typename T>
T get_local_var(const std::string& name, int rack_num = -1) {
    int context_idx = get_context_index(rack_num);
    auto it = local_var_index.find(name);
    if (it == local_var_index.end()) {
        throw std::runtime_error("Variable name '" + name + "' not found.");
    }
    int idx = it->second;

    if (idx >= local_var_vec[context_idx].size()) {
        throw std::out_of_range("Variable index out of range.");
    }

    return std::any_cast<T>(local_var_vec[context_idx][idx]);
}

// Get a local variable by index
template <typename T>
T get_local_var(int idx, int rack_num = -1) {
    int context_idx = get_context_index(rack_num);

    if (idx >= local_var_vec[context_idx].size()) {
        throw std::out_of_range("Variable index out of range.");
    }

    return std::any_cast<T>(local_var_vec[context_idx][idx]);
}

int test_local_vars(bool crash) {
    // Define variables
    def_local_var<int>("var1");
    def_local_var<double>("var2", 1);

    // Set values for specific racks
    set_local_var("var1", 42, 0);        // Context for rack 0
    set_local_var("var2", 3.14, 1);      // Context for rack 1

    set_local_var(1, 2.718, -1);         // By index for global context

    // Get and check values with assertions
    int var1_rack0 = get_local_var<int>("var1", 0);
    double var2_rack1 = get_local_var<double>("var2", 1);
    double var2_global = get_local_var<double>(1, -1);

    myassert(var1_rack0 == 42, "Rack 0 var1 should be 42", crash);
    myassert(var2_rack1 == 3.14, "Rack 1 var2 should be 3.14", crash);
    myassert(var2_global == 2.718, "Global var2 should be 2.718", crash);

    // Get values
    std::cout << "Rack 0 var1: " << get_local_var<int>("var1", 0) << std::endl;
    std::cout << "Rack 1 var2: " << get_local_var<double>("var2", 1) << std::endl;
    std::cout << "Global var2: " << get_local_var<double>(1, -1) << std::endl;

    return 0;
}


// Function to print the entire global_var_vec
void print_global_vars() {
    return;
    std::cout << "Global Variables:" << std::endl;
    for (const auto& [name, index] : global_var_index) {
        std::cout << "Name: " << name << " [" << index << "]";
        if (index < global_var_vec.size()) {
            std::cout << " Value: ";
            try {
                print_any_value(global_var_vec[index]);
            } catch (const std::bad_any_cast& e) {
                std::cout << "Error: Invalid type at index " << index;
            }
            std::cout << "; ";
        } else {
            std::cout << " Out of bounds index!";
        }
        std::cout << std::endl;

    }
}

void print_local_vars() {
    return;
    if (local_var_index.empty()) {
        std::cout << "No local variables defined." << std::endl;
        return;
    }

    std::cout << "Local Variables:" << std::endl;
    for (const auto& [name, index] : local_var_index) {
        std::cout << "Name: " << name << " [" << index << "]";
        if (index < local_var_vec.size()) {
            std::cout << " Values: ";
            for (size_t rack_num = 0; rack_num < local_var_vec[index].size(); ++rack_num) {
                std::cout << "Rack " << rack_num << ": ";
                try {
                    print_any_value(local_var_vec[index][rack_num]);
                } catch (const std::bad_any_cast& e) {
                    std::cout << "Error: Invalid type at index " << index;
                }
                std::cout << "; ";
            }
        } else {
            std::cout << " Out of bounds index!";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}




template <typename T>
T get_var_mb(int fc, int offset, int rack_number = -1) {
    //std::cout << "get_var_mb : fc :"<< fc <<" offset :" <<offset <<" rack_number "<< rack_number << std::endl; 
    if (rack_number == -1 || fc < MODBUS_MAX)
        return static_cast<T>(mb_map_vals[fc][offset]);
    fc -= MODBUS_MAX;
    return static_cast<T>(mb_rack_vals[rack_number][fc][offset]);
}

template <typename T> 
T read_rtos_mem(int offset, int rack_number = -1) {
    return static_cast<T>(shm_rtos_mem[rack_number][offset]);
}

template <typename T>
T read_rack_mem(int offset, int rack_number = -1) {
    return static_cast<T>(shm_rack_mem[rack_number][offset]);
}

void set_var_mb(int fc, int offset, int value, int rack_number = -1) {
    //std::cout << "set_var_mb : fc :"<<fc <<" Max fc: "<<MODBUS_MAX <<" offset :" <<offset <<" rack_number "<< rack_number <<" value "<< value << std::endl; 
    if (rack_number == -1 || fc < MODBUS_MAX)
        mb_map_vals[fc][offset] = value;
    else
    {
        fc -= MODBUS_MAX;
        mb_rack_vals[rack_number][fc][offset] = value;
    }
}


template <typename T>
T get_def_var(const std::string& var_ref, int rack_number=-1);

template <typename T>
void set_def_var(const std::string& var_ref, T value, int rack_number = -1);


// Function to print the value of a `std::any`
void print_any_value(const std::any& value) {
    if (value.type() == typeid(int)) {
        std::cout << std::any_cast<int>(value);
    } else if (value.type() == typeid(double)) {
        std::cout << std::any_cast<double>(value);
    } else if (value.type() == typeid(std::string)) {
        std::cout << std::any_cast<std::string>(value);
    } else {
        std::cout << "Unknown type";
    }
}


std::string get_any_type(const std::any& a) {
    if (a.has_value()) {
        return a.type().name(); // Get the name of the type stored in std::any
    } else {
        return "empty";
    }
}


template <typename T>
T get_var(VarDef& var_def, int rack_number = -1) {
    try {
        switch (var_def.type) {
            case VAR_MODBUS_INPUT:
            case VAR_MODBUS_HOLD:
            case VAR_MODBUS_BITS:
            case VAR_MODBUS_COIL:
            case VAR_MODBUS_RACK_INPUT:
            case VAR_MODBUS_RACK_HOLD:
            case VAR_MODBUS_RACK_BITS:
            case VAR_MODBUS_RACK_COIL:
                // Modbus access (requires Modbus function code and offset)
                return static_cast<T>(get_var_mb<T>(var_def.type, var_def.offset, rack_number));
            case VAR_RACK_MEM:
                if (rack_number == -1) {
                    throw std::runtime_error("Rack number is required for rack memory access.");
                }
                return *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]);
            case VAR_RTOS_MEM:
                if (rack_number == -1) {
                    throw std::runtime_error("Rack number is required for RTOS memory access.");
                }
                return *reinterpret_cast<T*>(&shm_rtos_mem[rack_number][var_def.offset]);
            case VAR_GLOBAL_VAR:
                return std::any_cast<T>(global_var_vec[var_def.offset]);
            case VAR_LOCAL_VAR:
                return std::any_cast<T>(local_var_vec[rack_number + 1][var_def.offset]);
            case VAR_INT_VAR:
            case VAR_DOUBLE_VAR:
                if (!var_def.value.has_value()) {
                    std::cerr << "Error: Attempting to access an empty std::any value in VarDef." << std::endl;
                    return static_cast<T>(0); // Default to 0 for empty std::any
                }

                try {
                    return std::any_cast<T>(var_def.value);
                } catch (const std::bad_any_cast& e) {
                    std::cerr << "Error: Bad std::any_cast. Expected type: " 
                            << typeid(T).name() 
                            << ", but std::any contains type: " 
                            << var_def.value.type().name() 
                            << ". Exception: " << e.what() << std::endl;
                    return static_cast<T>(0); // Default to 0 for type mismatch
                }
                // if any type is empty then complaing and return 0
                //return std::any_cast<T>(var_def.value);
                //return static_cast<T>(var_def.value);
            default:
                throw std::runtime_error("Invalid variable type.");
        }
    } catch (const std::bad_any_cast& e) {
        std::cerr << "get_var Failed to cast variable value : " << e.what() << std::endl;
        std::cout << "         var_def type "<< var_def_to_str(var_def) << std::endl;

        return T{}; // Return default value of T
    } catch (const std::exception& e) {
        std::cerr << "Error in get_var: " << e.what() << std::endl;
        return T{}; // Return default value of T
    }
}

//#### Improved `set_var`
template <typename T>
void set_var(VarDef& var_def, T value, int rack_number = -1) {
    try {
        switch (var_def.type) {
            case VAR_MODBUS_INPUT:
            case VAR_MODBUS_HOLD:
            case VAR_MODBUS_BITS:
            case VAR_MODBUS_COIL:
            case VAR_MODBUS_RACK_INPUT:
            case VAR_MODBUS_RACK_HOLD:
            case VAR_MODBUS_RACK_BITS:
            case VAR_MODBUS_RACK_COIL:
                set_var_mb(var_def.type, var_def.offset, static_cast<int>(value), rack_number);
                break;
            case VAR_RACK_MEM:
                if (rack_number == -1) {
                    throw std::runtime_error("Rack number is required for rack memory access.");
                }
                *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]) = value;
                break;
            case VAR_RTOS_MEM:
                if (rack_number == -1) {
                    throw std::runtime_error("Rack number is required for RTOS memory access.");
                }
                *reinterpret_cast<T*>(&shm_rtos_mem[rack_number][var_def.offset]) = value;
                break;
            case VAR_GLOBAL_VAR:
                global_var_vec[var_def.offset] = std::any(value);
                break;
            case VAR_LOCAL_VAR:
                local_var_vec[rack_number + 1][var_def.offset] = std::any(value);
                break;
            case VAR_INT_VAR:
                //var_def.int_value = static_cast<int>(value);
                var_def.value = std::any(value);
                break;
            case VAR_DOUBLE_VAR:
                //var_def.double_value = static_cast<double>(value);
                var_def.value = std::any(value);
                break;
            default:
                throw std::runtime_error("Invalid variable type.");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in set_var: " << e.what() << std::endl;
    }
}

//### Tests for `get_var` and `set_var`
void test_new_var_system() {
    // Define a VarDef for global variable
    VarDef global_var_def{VAR_GLOBAL_VAR, 0, std::any(), "global_var_1"};
    global_var_vec.resize(1000); // Resize the global variable vector for safety
    set_var(global_var_def, 42); // Set the global variable to 42

    // Test global variable
    int global_value = get_var<int>(global_var_def);
    myassert(global_value == 42, "Global variable should be 42", true);

    // Define a VarDef for local variable
    VarDef local_var_def{VAR_LOCAL_VAR, 1, std::any(), "local_var_1"};
    //local_var_vec.resize(3, std::vector<std::any>(10)); // Ensure enough room
    set_var(local_var_def, 84, 0); // Set the local variable for rack 0 to 84

    // Test local variable
    int local_value = get_var<int>(local_var_def, 0);
    myassert(local_value == 84, "Local variable for rack 0 should be 84", true);

    // Test error handling
    VarDef invalid_var_def{VAR_INT_VAR, -1,  std::any(), "invalid_var"};
    int invalid_value = get_var<int>(invalid_var_def);
    myassert(invalid_value == 0, "Invalid variable should return 0", true);
}

// template <typename T>
// T old_get_var(const VarDef& var_def, int rack_number = -1) {
//     switch (var_def.type) {
//         case VAR_MODBUS_INPUT:
//         case VAR_MODBUS_HOLD:
//         case VAR_MODBUS_BITS:
//         case VAR_MODBUS_COIL:
//         case VAR_MODBUS_RACK_INPUT:
//         case VAR_MODBUS_RACK_HOLD:
//         case VAR_MODBUS_RACK_BITS:
//         case VAR_MODBUS_RACK_COIL:
//             // Modbus access (requires Modbus function code and offset)
//             return static_cast<T>(get_var_mb<T>(var_def.type, var_def.offset, rack_number));
//         case VAR_RACK_MEM:
//             if (rack_number == -1) {
//                 throw std::runtime_error("Rack number is required for rack memory access.");
//             }
//             return read_rack_mem<T>(var_def.offset, rack_number);
//             //return *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]);
//             break;

//         case VAR_RTOS_MEM:
//             if (rack_number == -1) {
//                 throw std::runtime_error("Rack number is required for RTOS memory access.");
//             }
//             // Example: Implement RTOS shared memory access here
//             return read_rtos_mem<T>(var_def.offset, rack_number);
//             break;
//         case VAR_GLOBAL_VAR:
//             try {
//                 // Use std::any_cast to retrieve the value and cast it to the desired type
//                 return std::any_cast<T>(global_var_vec[var_def.offset]);
//             } catch (const std::bad_any_cast& e) {
//                 // Handle the case where the stored type doesn't match the requested type
//                 std::cerr << "Error: Failed to cast global_var_vec at index "
//                           << var_def.offset << " to the requested type. " << e.what() << std::endl;  
//                 return static_cast<T>(0);
//             }
//         case VAR_LOCAL_VAR:
//             // if (local_vars.find(var_def.name) == local_vars.end()) {
//             //     throw std::runtime_error("Local variable " + var_def.name + " not found.");
//             // }
//             return std::any_cast<T>(local_var_vec[rack_number+1][var_def.offset]);
//         case VAR_INT_VAR:
//             return std::any_cast<T>(var_def.int_value);

//         case VAR_DOUBLE_VAR:
//             return std::any_cast<T>(var_def.double_value);
//         default:
//             throw std::runtime_error("Invalid variable type.");
//     }
// }

// template <typename T>
// void old_set_var(VarDef& var_def, T value, int rack_number = -1) {
//     switch (var_def.type) {
//         case VAR_MODBUS_INPUT:
//         case VAR_MODBUS_HOLD:
//         case VAR_MODBUS_BITS:
//         case VAR_MODBUS_COIL:
//         case VAR_MODBUS_RACK_INPUT:
//         case VAR_MODBUS_RACK_HOLD:
//         case VAR_MODBUS_RACK_BITS:
//         case VAR_MODBUS_RACK_COIL:
//             set_var_mb(var_def.type, var_def.offset, static_cast<int>(value), rack_number);
//             break;
//         case VAR_RACK_MEM:
//             if (rack_number == -1) {
//                 throw std::runtime_error("Rack number is required for rack memory access.");
//             }
//             *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]) = value;
//             break;
//         case VAR_RTOS_MEM:
//             if (rack_number == -1) {
//                 throw std::runtime_error("Rack number is required for RTOS memory access.");
//             }
//             *reinterpret_cast<T*>(&shm_rtos_mem[rack_number][var_def.offset]) = value;
//             // Example: Implement RTOS shared memory write here
//             // write_rtos_mem<T>(rack_number, var_def.offset, value);
//             break;
//         case VAR_GLOBAL_VAR:
//             global_var_vec[var_def.offset] = value;
//             break;
//         case VAR_LOCAL_VAR:
//             //std::cout << " var offset [" << var_def.offset << "] rack_number [" << rack_number <<"]" << std::endl;
//             //std::cout << " var vec size [" << local_var_vec.size()  << "] rack size [" << local_var_vec[var_def.offset].size()  <<"]" << std::endl;
//             local_var_vec[rack_number+1][var_def.offset] = value; // TODO use get_context_index
//             break;
//         case VAR_INT_VAR:
//             var_def.int_value = (int)value;
//             var_def.value = (int)value;
//             break;
//         case VAR_DOUBLE_VAR:
//             var_def.double_value = (double)value;
//             var_def.value = (int)value;
//             break;
//         default:
//             throw std::runtime_error("Invalid variable type.");
//     }
// }


// funcion to decode a json element into a var_def
int decode_step_var(Step& step, const json& step_json, const std::string& name, int src_id , int dest_id)
{
    if(src_id == KEY_LIM_STR)
    {
        if(key_lim_debug)std::cout <<" decoding KEY_LIM" << std::endl;
    }

    if (step_json.contains(name))
    {
        VarDef var_def;
        if(src_id == KEY_LIM_STR)
        {
            if(key_lim_debug)
            {
                std::cout <<" decoding KEY_LIM name found" << std::endl;
                std::cout << "JSON type: [" << step_json[name].type_name() << "] for key '" << name << "'." << std::endl;
                if(step_json[name].is_string())
                {
                    std::cout << "JSON string: [" << step_json[name].get<std::string>() << "] for key '" << name << "'." << std::endl;
                }
            }
        }

        if(step_json[name].is_string())
        {
            step.step_map[src_id]      = step_json[name].get<std::string>();
            decode_var_type(var_def, step_json[name].get<std::string>()); 
        } else if (step_json[name].is_number()) {
            // Handle integer case
            var_def.type = VAR_INT_VAR;
            var_def.value = std::any(step_json[name].get<int>());
        } else if (step_json[name].is_number_integer()) {
            // Handle integer case
            var_def.type = VAR_INT_VAR;
            var_def.value = std::any(step_json[name].get<int>());
        } else if (step_json[name].is_number_float()) {
            // Handle double/float case
            var_def.type = VAR_DOUBLE_VAR;
            var_def.value = std::any(step_json[name].get<double>());
        } else {
            std::cout << "Unsupported JSON type: " << step_json[name].type_name() << " for key '" << name << "'." << std::endl;
            //std::cerr << "Error: Unsupported type for key '" << name << "'." << std::endl;
            return -1;
        }
        // Store the decoded variable definition in the step map
        step.var_map[dest_id] = var_def;
    }
    else
    {
        if(src_id == KEY_LIM_STR)
        {
            if(key_lim_debug)
                std::cout << " looking for limit name  ["<<name<<"] json >>"<<step_json.dump()<< "<<"<< std::endl;
        }
    }
    return 0;
}

void test_decode_step_var() {
    // Test setup
    Step step;
    json step_json = {
        {"src", "rinput:5"},
        {"value_int", 42},
        {"value_double", 3.14}
    };
    // Decode string variable
    int result = decode_step_var(step, step_json, "src", KEY_SRC_STR, KEY_SRC);
    //myassert(result == 0, "Decoding string variable should succeed", false);
    //return;
    VarDef src_var = std::any_cast<VarDef>(step.var_map[KEY_SRC]);
    myassert(src_var.type == VAR_MODBUS_RACK_INPUT, "Source variable type should be VAR_MODBUS_RACK_INPUT", false);
    myassert(src_var.offset == 5, "Source variable offset should be 5", false);
    //return;
    // Decode integer variable
    result = decode_step_var(step, step_json, "value_int", KEY_VALUE, KEY_VALUE);
    //myassert(result == 0, "Decoding integer variable should succeed", false);
    VarDef int_var = std::any_cast<VarDef>(step.var_map[KEY_VALUE]);
    myassert(int_var.type == VAR_INT_VAR, "Integer variable type should be VAR_INT_VAR", false);
    VarDef int_var2 = std::any_cast<VarDef>(step.var_map[KEY_VALUE]);
    myassert(std::any_cast<int>(int_var2.value) == 42, "Integer variable value should be 42", false);

    // Decode double variable
    result = decode_step_var(step, step_json, "value_double", KEY_VALUE, KEY_VALUE);
    //myassert(result == 0, "Decoding double variable should succeed", false);
    VarDef double_var = std::any_cast<VarDef>(step.var_map[KEY_VALUE]);
    //myassert(double_var.type == VAR_DOUBLE_VAR, "Double variable type should be VAR_DOUBLE_VAR", false);
    // TODOsort this out..
    //myassert(std::any_cast<double>(double_var.value) == 3.14, "Double variable value should be 3.14", false);

    std::cout << "All tests passed for decode_step_var!" << std::endl;
}


// Function to decode a Step JSON object into a map of <int, std::any>
int decode_step_map(Step& step, const json& step_json) {
    //std::map<int, std::any> step_map;

    // Map JSON fields to their respective integer keys
    if (step_json.contains("oper")) {
                                                    step.step_map[KEY_OPER_STR] = step_json["oper"].get<std::string>();
        // Add a special mapping for the operation key as well
        if (step_json["oper"] == "set")             step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_SET);
        else if (step_json["oper"] == "wait_reset") step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_WAIT);
        else if (step_json["oper"] == "jump_if")    step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_JUMP_IF);
        else if (step_json["oper"] == "jump_not")   step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_JUMP_NOT);
        else if (step_json["oper"] == "add")        step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_ADD);
        else if (step_json["oper"] == "add")        step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_ADD);
        else if (step_json["oper"] == "inc")        step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_INC);
        else if (step_json["oper"] == "agg")        step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_AGG);
        else if (step_json["oper"] == "maxl")       step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_MAX);
        else if (step_json["oper"] == "minl")       step.step_map[KEY_OPER] = static_cast<int>(KEY_OPER_MIN);
        else step.step_map[KEY_OPER]                                        = static_cast<int>(KEY_OPER_NOOP);
        // Add other operations as needed
    }
    
    decode_step_var(step, step_json, "src",     KEY_SRC_STR,     KEY_SRC);
    decode_step_var(step, step_json, "dest",    KEY_DEST_STR,    KEY_DEST);
    decode_step_var(step, step_json, "var",     KEY_VAR_STR,     KEY_VAR);
    decode_step_var(step, step_json, "limit",   KEY_LIM_STR,     KEY_LIM);
    decode_step_var(step, step_json, "counter", KEY_COUNTER_STR, KEY_COUNTER);
    decode_step_var(step, step_json, "rnum",    KEY_RNUM_STR,    KEY_RNUM);
    decode_step_var(step, step_json, "inc",     KEY_INC_STR,     KEY_INC);

    if (step_json.contains("cond"))                 step.step_map[KEY_COND_STR]     = step_json["cond"].get<std::string>();
    if (step_json.contains("idx"))                  step.step_map[KEY_IDX]          = step_json["idx"].get<int>();
    if (step_json.contains("value"))                step.step_map[KEY_VALUE]        = step_json["value"].get<int>();
    if (step_json.contains("jump_to"))              step.step_map[KEY_JUMP_TO]      = step_json["jump_to"].get<int>();
    if (step_json.contains("time"))                 step.step_map[KEY_TIME]         = step_json["time"].get<int>();

    return 0;
}


bool test_var_map(const Step& step, int key, int rack_number = -1) 
{
    if (step.var_map.find(key) == step.var_map.end()) {
        return false;
        std::cout<< "Key not found in step.var_map: " << std::to_string(key) << std::endl;
    }
    return true;
}
bool test_step_map(const Step& step, int key, int rack_number = -1) 
{
    if (step.step_map.find(key) == step.step_map.end()) {
        return false;
        std::cout<< "Key not found in step.step_map: " << std::to_string(key) << std::endl;
    }
    return true;
}


template <typename T>
T get_map_var(Step& step, int key, int rack_number = -1) {
    auto it = step.var_map.find(key);
    if (it == step.var_map.end()) {
        std::cerr << "Key not found in step.var_map: " << key << std::endl;
        return T{};
    }

    try {
        // Work with a reference to the VarDef in var_map
        VarDef& def = std::any_cast<VarDef&>(it->second);
        if(key == KEY_LIM)
        {
            if(key_lim_debug)
                std::cout << " getting KEY LIM var type " << get_any_type(def.value) << std::endl;

        }
        return get_var<T>(def, rack_number);
    } catch (const std::bad_any_cast& e) {
        throw std::runtime_error("Failed to cast variable for key: " + std::to_string(key) + " - " + std::string(e.what()));
    }
}


template <typename T>
void set_map_var(Step& step, int key, T value, int rack_number = -1) {
    auto it = step.var_map.find(key);
    if (it == step.var_map.end()) {
        std::cerr << "Key not found in step.var_map: " << key << std::endl;
        return;
    }

    try {
        // Work with a reference to the VarDef in var_map
        VarDef& def = std::any_cast<VarDef&>(it->second);
        set_var<T>(def, value, rack_number);
    } catch (const std::bad_any_cast& e) {
        throw std::runtime_error("Failed to cast variable for key: " + std::to_string(key) + " - " + std::string(e.what()));
    }
}


// more interesting steps
        // {"oper": "agg", "src": "rinput:1", "dest": "g_rack_voltage",     "desc": "Calculate max/min/total/count voltage"},
        // {"oper": "3sum_max", "src": "rinput:1", "var": "rhold:2", "dest":"bits:12",    "desc": "Set level 1,2,3 max_alarm"},
        // {"oper": "3sum_min", "src": "rinput:1", "var": "rhold:5", "dest":"bits:15",    "desc": "Set level 1,2,3 min_alarm"},
// run steps based on keys better than decoding strings all the time
void run_step_key(Sequence &sequence , Step& step, const std::string& name, int &current_step, int &rack_num)
{
    //std::cout << " getting oper_str"<< std::endl;
    std::string oper_str = std::any_cast<std::string>(step.step_map[KEY_OPER_STR]);

    //std::cout << " getting oper_key"<< std::endl;
    int oper_key = std::any_cast<int>(step.step_map[KEY_OPER]);

    //std::cout << "Decoded Step Operation: [" << oper_str<< "] key [" << oper_key <<"]"  << std::endl;
    switch (oper_key) {

        case KEY_OPER_SET:
            {
                // add rnum and inc
                                //step.var_map[dest_id] = var_def;

                //decode_var
                int var_val = step.value;
                int inc = 0;
                // do we have a src 
                int var_rnum = -1;
                int var_inc = -1;

                if(test_var_map(step, KEY_SRC))
                {
                    var_val = get_map_var<int>(step, KEY_SRC, rack_num); 
//                    std::cout << "set got var map value :" << var_val <<std::endl;
                }
                else if(test_step_map(step, KEY_SRC_STR))
                {
                    //std::cout << "set got var str value :" << var_val <<std::endl;
                    std::string dest_root = std::any_cast<std::string>(step.step_map[KEY_SRC_STR]);
                    var_val = get_def_var<int>(dest_root, rack_num); 
 
                }
                else
                {
                    std::cout << "used default value :" << var_val <<std::endl;
                }
                if(test_var_map(step, KEY_RNUM))
                {
                    var_rnum = get_map_var<int>(step, KEY_RNUM, rack_num); 
                    std::cout << "set got rnum :" << var_rnum <<std::endl;
                }
                if(test_var_map(step, KEY_INC))
                {
                    var_inc = get_map_var<int>(step, KEY_INC, rack_num); 
                    std::cout << "set got unc :" << var_inc <<std::endl;
                }

                if(test_var_map(step, KEY_DEST))
                {
  //                  std::cout << "found dest var map: " << var_val <<std::endl;
                    set_map_var<int>(step, KEY_DEST, var_val, rack_num); 

                }
                // this will create KEY_DEST
                else if(test_step_map(step, KEY_DEST_STR))
                {
                    //std::cout << "found dest var str :" << var_val <<std::endl;
                    std::string dest_root = std::any_cast<std::string>(step.step_map[KEY_DEST_STR]);
                    set_def_var<int>(dest_root, var_val, rack_num); 


                    //set_ref_var<int>(step, KEY_DEST, var_val, rack_num);
                }
                else
                {
                    std::cout << "set no dest :"<<std::endl;
                }
                if (var_inc > 0)
                {
                    std::cout << "inc found try to inc rnum"<<std::endl;

                    if(test_step_map(step, KEY_RNUM_STR))
                    {
                        set_map_var<int>(step, KEY_RNUM, var_rnum+var_inc, rack_num); 
                    }
                    else
                    {
                        std::cout << " no rnum in map "<<std::endl;

                    }
                }



            } 
            //set_val(step.dest, step.value, rack_num);
            current_step++;
            break;

        case KEY_OPER_INC: 
            //std::cout << "Performing 'inc' operation." << std::endl;
            {
                //std::cout << "'inc' src [" << step.src<<"] val ["<< curr_val << "] step.value [" << step.value<<"]"<<  std::endl;

                int step_val = step.value;
                int var_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                //std::cout << "'after inc' src [" << step.src<<"] val ["<< curr_val << "] var_val #1 ["<< var_val<< "] step.value [" << step.value<<"]"<<  std::endl;
                set_map_var<int>(step, KEY_DEST, var_val + step_val, rack_num); 
                //std::cout << "'after inc' src [" << step.src<<"] val ["<< curr_val << "] var_val #2 ["<< var_val<< "] step.value [" << step.value<<"]"<<  std::endl;
                current_step++;
            }
            break;

        case KEY_OPER_ADD: // KEY_OPER + 1 == "set"
            //std::cout << "Performing 'add' operation." << std::endl;
            {
                int curr_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                int add_val = get_map_var<int>(step, KEY_VAR, rack_num);
                set_map_var<int>(step, KEY_DEST, curr_val + add_val, rack_num);
                current_step++;
            }
            break;

        case KEY_OPER_MUL:
            //std::cout << "Performing 'mul' operation." << std::endl;
            {
                int curr_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                int add_val = get_map_var<int>(step, KEY_VAR, rack_num);
                set_map_var<int>(step, KEY_DEST, curr_val * add_val, rack_num);
                current_step++;
            }
            break;

        case KEY_OPER_DIV:
            //std::cout << "Performing 'div' operation." << std::endl;
            {
                int curr_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                int add_val = get_map_var<int>(step, KEY_VAR, rack_num);
                if(add_val>0)
                set_map_var<int>(step, KEY_DEST, curr_val/add_val, rack_num);
                current_step++;
            }
            // Further logic for "set"
            break;

        case KEY_OPER_WAIT:
            std::cout << "Performing 'wait' operation." << std::endl;
            {
                if(sequence.sq_vals[rack_num].find(step.counter) == sequence.sq_vals[rack_num].end())
                {
                    sequence.sq_vals[rack_num][step.counter] = 0;
                }
                int curr_val = sequence.sq_vals[rack_num][step.counter];
                curr_val++;
                sequence.sq_vals[rack_num][step.counter] = curr_val;
                
                if (curr_val >= step.time) {
                    curr_val = 0; // Reset the counter
                    ++current_step; // Move to the next step
                } else {
                    std::cout << "Waiting on step " << current_step << " for rack " << rack_num
                         << ", counter: " << curr_val << "/" << step.time << std::endl;
                }
            }
            break;


        case KEY_OPER_JUMP_IF:
            //std::cout << "Performing 'jump' operation." << std::endl;
            {
                int src_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                if(1)std::cout << " >>>>> test  eval jump_if src_val ["
                 << step.src << "] val ["
                 << src_val <<"] cond ["
                 << step.cond << "] targ_val ["
                 << step.value <<"] jump_to [" << step.jump_to << "]"<< std::endl;   
                if (evaluate_condition(step.cond, src_val, step.value)) {
                    current_step = step.jump_to;
                    std::cout << " >>>>> execute eval jump_if src_val ["<< step.src << "] val ["<< src_val <<"] jump_to [" << step.jump_to << "]"<< std::endl;   
                    sequence.sq_vals[rack_num]["current_step"] = current_step;
                }
                else
                {
                    current_step++;
                }
            }

            break;

        case KEY_OPER_JUMP_NOT:
            //std::cout << "Performing 'jump not' operation." << std::endl;
            {
                int src_val = get_map_var<int>(step, KEY_SRC, rack_num); 
                if(0)std::cout << " >>>>> test  eval jump_if src_val ["
                 << step.src << "] val ["
                 << src_val <<"] cond ["
                 << step.cond << "] targ_val ["
                 << step.value <<"] jump_to [" << step.jump_to << "]"<< std::endl;   
                if (!evaluate_condition(step.cond, src_val, step.value)) {
                    current_step = step.jump_to;
                    std::cout << " >>>>> execute eval jump_if src_val ["<< step.src << "] val ["<< src_val <<"] jump_to [" << step.jump_to << "]"<< std::endl;   
                    sequence.sq_vals[rack_num]["current_step"] = current_step;
                }
                else
                {
                    current_step++;
                }
            }
            break;

        case KEY_OPER_AGG:
            {
                int num_racks = get_def_var<int>("g_num_racks");
                std::cout << "Performing 'agg' operation." << " num_racks " << num_racks << std::endl;
                std::string cond;

                std::string dest_root = std::any_cast<std::string>(step.step_map[KEY_DEST_STR]);

                if(step.step_map.find(KEY_COND_STR) != step.step_map.end())
                { 
                    cond = std::any_cast<std::string>(step.step_map[KEY_COND_STR]);
                    std::cout << "Performing 'agg' operation cond " << cond << std::endl;
                }

                // this needs to be done once and the defs stored 
                std::string dest_max = dest_root + "_max";
                std::string dest_max_num = dest_root + "_max_num";
                std::string dest_min = dest_root + "_min";
                std::string dest_min_num = dest_root + "_min_num";
                std::string dest_tot = dest_root + "_total";
                std::string dest_num = dest_root + "_num";
                int curr_val = 0;
                //int curr_max_val = 0;

                set_def_var<int>(dest_root, 0, rack_num); 
                set_def_var<int>(dest_max, curr_val, rack_num); 
                set_def_var<int>(dest_min, 0xffff, rack_num); 
                set_def_var<int>(dest_max_num, -1, rack_num); 
                set_def_var<int>(dest_min_num, -1, rack_num); 
                set_def_var<int>(dest_tot, curr_val, rack_num); 
                set_def_var<int>(dest_num, curr_val, rack_num); 
                int curr_min_val = get_def_var<int>(dest_min); 
                int curr_max_val = 0;//get_def_var<int>(dest_max); 
                int curr_tot_val = 0;//get_def_var<int>(dest_tot); 
                int curr_num_val = 0;// get_def_var<int>(dest_num); 
                int curr_min_num = 0;
                int curr_max_num = 0;
                bool online = true;
                std::vector<bool>results;

                for (int i = 0 ; i<num_racks; i++)
                {
                    if ( i == 0)
                    {
                        curr_val = 0;
                    }
                    curr_val = get_map_var<int>(step, KEY_SRC, i); 

                    if(!cond.empty())
                    {
                        online = evaluate_trigger_name(results, cond, i);
                    }
                    if(!online)
                    {
                        std::cout<<" skkpping  rack ["<< i<< "] it is offline"<<std::endl;
                    }
                    if(online)
                    {
                        if(curr_val > curr_max_val) {
                            curr_max_val = curr_val;
                            curr_max_num = i;
                        }

                        if(curr_val < curr_min_val) {

                            curr_min_val = curr_val;
                            curr_min_num = i;
                        } 
                        curr_tot_val += curr_val;
                        curr_num_val ++;
                    }
                    if (i == num_racks-1)
                    {
                        std::cout << " on last rack "<< i <<std::endl;
                    }
                }
                
                set_def_var<int>(dest_max,     curr_max_val,curr_max_num);
                set_def_var<int>(dest_max_num, curr_max_num, curr_max_num);
                set_def_var<int>(dest_min,     curr_min_val, curr_min_num);
                set_def_var<int>(dest_min_num, curr_min_num, curr_min_num);
                set_def_var<int>(dest_tot,     curr_tot_val,-1); 
                set_def_var<int>(dest_num,     curr_num_val, -1); 


            }
            break;

        case KEY_OPER_MAX:
            {
                int num_racks = get_def_var<int>("g_num_racks");
                std::cout << "Performing 'max' operation." << " num_racks " << num_racks << std::endl;
                std::string cond;

                std::string dest_root = std::any_cast<std::string>(step.step_map[KEY_DEST_STR]);

                if(step.step_map.find(KEY_COND_STR) != step.step_map.end())
                { 
                    cond = std::any_cast<std::string>(step.step_map[KEY_COND_STR]);
                    std::cout << "Performing 'max' operation cond " << cond << std::endl;
                }

                // this needs to be done once and the defs stored 
                std::string dest_max = dest_root + "_max";
                std::string dest_max_num = dest_root + "_max_num";
                std::string dest_lim = dest_root + "_lim";
                std::string dest_limits_found = dest_root + "_found";
                // std::string dest_tot = dest_root + "_total";
                std::string dest_num = dest_root + "_num";
                int curr_val;
                int lim_val;
                // set_def_var<int>(dest_root, 0, rack_num); 
                // set_def_var<int>(dest_max, curr_val, rack_num); 
                // set_def_var<int>(dest_min, 0xffff, rack_num); 
                // set_def_var<int>(dest_max_num, -1, rack_num); 
                // set_def_var<int>(dest_min_num, -1, rack_num); 
                // set_def_var<int>(dest_tot, curr_val, rack_num); 
                // set_def_var<int>(dest_num, curr_val, rack_num); 
                // int curr_min_val = get_def_var<int>(dest_min); 
                // int curr_max_val = get_def_var<int>(dest_max); 
                // int curr_tot_val = get_def_var<int>(dest_tot); 
                int curr_num_val = 0;// get_def_var<int>(dest_num); 
                int curr_lim_num = 0;
                int curr_lim_val = 0;
                int limits_found = 0;
                bool online = true;
                bool max_alarm = false;
                std::vector<bool>results;

                for (int i = 0 ; i<num_racks; i++)
                {
                    if ( i == 0)
                    {
                        curr_val = 0;
                    }
                    curr_val = get_map_var<int>(step, KEY_SRC, i); 
                    // we may have different limits for each rack
                    //std::cout<<" getting KEY_LIM"<<std::endl;
                    lim_val = get_map_var<int>(step, KEY_LIM, i); 
                    //std::cout<<" got KEY_LIM"<<std::endl;


                    if(!cond.empty())
                    {
                        online = evaluate_trigger_name(results, cond, i);
                    }
                    if(!online)
                    {
                        std::cout<<" skkpping  rack ["<< i<< "] it is offline"<<std::endl;
                    }
                    if(online)
                    {
                        if(curr_val > lim_val) {
                            if(curr_val > curr_lim_val) {
                                curr_lim_val = curr_val;
                                curr_lim_num = i;
                                limits_found++;
                            }
                            max_alarm = true;
                        }
                        curr_num_val ++;
                    }
                    if (i == num_racks-1)
                    {
                        std::cout << " on last rack "<< i <<std::endl;
                    }
                }
                
                set_def_var<int>(dest_max,     curr_lim_val,curr_lim_num);
                set_def_var<int>(dest_max_num, curr_lim_num, curr_lim_num);
                // set_def_var<int>(dest_min,     curr_min_val, curr_min_num);
                // set_def_var<int>(dest_min_num, curr_min_num, curr_min_num);
                // set_def_var<int>(dest_tot,     curr_tot_val,-1); 
                set_def_var<int>(dest_num,     curr_num_val, -1); 
                set_def_var<int>(dest_lim,     lim_val, -1); 
                set_def_var<int>(dest_limits_found,     limits_found, -1); 

            }
            break;
        case KEY_OPER_MIN:
            {
                int num_racks = get_def_var<int>("g_num_racks");
                std::cout << "Performing 'min' operation." << " num_racks " << num_racks << std::endl;
                std::string cond;

                std::string dest_root = std::any_cast<std::string>(step.step_map[KEY_DEST_STR]);

                if(step.step_map.find(KEY_COND_STR) != step.step_map.end())
                { 
                    cond = std::any_cast<std::string>(step.step_map[KEY_COND_STR]);
                    std::cout << "Performing 'max' operation cond " << cond << std::endl;
                }

                // this needs to be done once and the defs stored 
                std::string dest_min = dest_root + "_min";
                std::string dest_min_num = dest_root + "_min_num";
                std::string dest_lim = dest_root + "_lim";
                std::string dest_limits_found = dest_root + "_found";
                // std::string dest_tot = dest_root + "_total";
                std::string dest_num = dest_root + "_num";
                int curr_val;
                int lim_val;
                int curr_num_val = 0;// get_def_var<int>(dest_num); 
                int curr_lim_num = 0;
                int curr_lim_val = 0;
                int limits_found = 0;
                bool online = true;
                bool max_alarm = false;
                std::vector<bool>results;

                for (int i = 0 ; i<num_racks; i++)
                {
                    if ( i == 0)
                    {
                        curr_val = 0;
                    }
                    curr_val = get_map_var<int>(step, KEY_SRC, i); 
                    // we may have different limits for each rack
                    if(key_lim_debug)std::cout<<" getting KEY_LIM"<<std::endl;
                    lim_val = get_map_var<int>(step, KEY_LIM, i); 
                    if(key_lim_debug)std::cout<<" got KEY_LIM"<<std::endl;


                    if(!cond.empty())
                    {
                        online = evaluate_trigger_name(results, cond, i);
                    }
                    if(!online)
                    {
                        std::cout<<" skkpping  rack ["<< i<< "] it is offline"<<std::endl;
                    }
                    if(online)
                    {
                        if(curr_val > lim_val) {
                            if(curr_val < curr_lim_val) {
                                curr_lim_val = curr_val;
                                curr_lim_num = i;
                                limits_found++;
                            }
                            max_alarm = true;
                        }
                        curr_num_val ++;
                    }
                    if (i == num_racks-1)
                    {
                        std::cout << " on last rack "<< i <<std::endl;
                    }
                }
                
                set_def_var<int>(dest_min,     curr_lim_val,curr_lim_num);
                set_def_var<int>(dest_min_num, curr_lim_num, curr_lim_num);
                set_def_var<int>(dest_num,     curr_num_val, -1); 
                set_def_var<int>(dest_lim,     lim_val, -1); 
                set_def_var<int>(dest_limits_found,     limits_found, -1); 

            }
            break;

        case KEY_OPER_NOOP:
        default:
            std::cout << "Unknown operation.[" << oper_str <<"]"<< std::endl;
            break;
    }
}

// returns 1 if this is a new var
int decode_var_type(VarDef &def, const std::string& var_ref) {
    int ret = 0;
    if (var_ref.find("rinput:") == 0) {
        def.type = VAR_MODBUS_RACK_INPUT;
        def.offset = std::stoi(var_ref.substr(7));
    } else if (var_ref.find("input:") == 0) {
        def.type = VAR_MODBUS_INPUT;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("rhold:") == 0) {
        def.type = VAR_MODBUS_RACK_HOLD;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("hold:") == 0) {
        def.type = VAR_MODBUS_HOLD;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rbits:") == 0) {
        def.type = VAR_MODBUS_RACK_BITS;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("bits:") == 0) {
        def.type = VAR_MODBUS_BITS;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rcoil:") == 0) {
        def.type = VAR_MODBUS_RACK_COIL;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("coil:") == 0) {
        def.type = VAR_MODBUS_COIL;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rack:") == 0) {
        def.type = VAR_RACK_MEM;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("sbmu:") == 0) {
        def.type = VAR_SBMU_MEM;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rtos:") == 0) {
        def.type = VAR_RTOS_MEM;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("g_") == 0) {
        // Global variable
        def.type = VAR_GLOBAL_VAR;
        def.name = var_ref;
        if (global_var_index.find(var_ref) == global_var_index.end()) {
            global_var_index[var_ref] = global_var_index.size();
            global_var_vec.resize(global_var_index.size()); // Ensure the vector can hold the new index
            ret = 1;
        }
        def.offset = global_var_index[var_ref];
    } else {
        //std::cout<<" setting up a local var ["<<var_ref<<"]"<< std::endl;
        // Local variable
        // definie a local variable
        def.type = VAR_LOCAL_VAR;
        def.name = var_ref;
        if (local_var_index.find(var_ref) == local_var_index.end()) {
            auto idx  = local_var_index.size();
            //std::cout<<" setting up a local var index ["<<var_ref<<"] idx ["<< idx<<"]"<< std::endl;
            local_var_index[var_ref] = idx;
            //std::cout<<" checking local var vec size  [" <<local_var_vec.size()<<"]"<< std::endl;
            ///local_var_vec.resize(local_var_index.size()); // Ensure the vector can hold the new index
            // Ensure all contexts have space for this variable
            for (auto& context : local_var_vec) {
                if (idx >= context.size()) {
                    context.resize(idx + 1);
                }
                context[idx] = 0; // Default-initialize the variable
            }
            //std::cout<<" rechecking local var vec size  [" <<local_var_vec.size()<<"]"<< std::endl;
            ret = 1;

        }
        def.offset = local_var_index[var_ref];
    }
    return ret;
}


// possibly todo
// std::unordered_map<int, std::function<void()>> operations = {
//     {KEY_OPER_SET, []() { std::cout << "Performing 'set' operation." << std::endl; }},
//     {KEY_OPER_ADD, []() { std::cout << "Performing 'add' operation." << std::endl; }},
//     {KEY_OPER_DIV, []() { std::cout << "Performing 'div' operation." << std::endl; }},
//     {KEY_OPER_WAIT, []() { std::cout << "Performing 'wait' operation." << std::endl; }},
//     {KEY_OPER_JUMP, []() { std::cout << "Performing 'jump' operation." << std::endl; }},
// };

// if (operations.find(oper_key) != operations.end()) {
//     operations[oper_key]();
// } else {
//     std::cout << "Unknown operation [" << oper_str << "]" << std::endl;
// }


// if a reg_ref starts with an "r" then this becomes a rack type of access
// parse a reg ref into name
std::string parse_reg_name(const std::string& ref) {
    size_t colon_pos = ref.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid name reference format: " + ref);
    }

    // Extract the part after the colon
    return ref.substr(colon_pos + 1);
}

// parse a reg ref into name
int parse_reg_num(const std::string& ref) {
    size_t colon_pos = ref.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid reference format: " + ref);
    }

    // Extract the part after the colon and convert it to an integer
    std::string reg_str = ref.substr(colon_pos + 1);
    try {
        return std::stoi(reg_str);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Failed to parse register number from reference: " + ref);
    }
}


std::string parse_name(const std::string& ref) {
    size_t colon_pos = ref.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid name reference format: " + ref);
    }
    // Extract the part after the colon
    return ref.substr(colon_pos + 1);
}

int get_trigger_val(const json& trigger_value, int rack_num = -1) {
    // Check if the value is a number
    if (trigger_value.is_number()) {
        return trigger_value.get<int>();
    }
    // Check if the value is a string
    if (trigger_value.is_string()) {
        auto tvstr = trigger_value.get<std::string>();
        //std::cout << "trigger string = ["<<tvstr<<"]"<< std::endl;
        int val = get_def_var<int>(tvstr, rack_num);
        //std::cout << "trigger string = ["<<tvstr<<"] val ["<< val <<"] rack ["<< rack_num <<"]"<<  std::endl;
        return val;
    }
    return 0;
    //throw std::invalid_argument("Invalid value type in trigger: " + trigger_value.dump());
}

// seq_config step.dest =
int decode_step_from_json(Step &step, json stepj)
{
    step.oper    = stepj.value("oper", "");;
    step. src    = stepj.value("src", "");
    step.dest    = stepj.value("dest", "");
    step.value   = stepj.value("value", 0);
    step.counter = stepj.value("counter", "");
    step.time    = stepj.value("time", 0);
    step.jump_to = stepj.value("jump_to", 0);
    step.cond    = stepj.value("cond", "");
    step.var     = stepj.value("var", "");
    decode_step_map(step, stepj);

    return 0;
}


int setup_sequence(json& seq) 
{
    if (!seq.contains("sequence_name") || !seq.contains("steps")) {
        //throw std::runtime_error("Invalid sequence configuration: Missing required fields.");
        std::cout <<"Invalid sequence configuration: Missing required fields."<<std::endl;
        return -1;
    }
    Sequence sequence;
    std::string sequence_name = seq["sequence_name"];
    //saved_triggers_map[sequence_name] = config;
    sequence.name = sequence_name;
    bool running = seq.value("running", false); // Default to false if not specified
    std::vector<Step> steps;

    // Process each step
    if(seq.contains("steps"))
    {
        steps.clear();
        for (const auto& stepj : seq["steps"]) {
            Step step;
            decode_step_from_json(step, stepj);

            steps.emplace_back(step);
        }
        sequence.steps = steps;
    }
    // Process each entry
    if(seq.contains("entry"))
    {
        steps.clear();
        for (const auto& stepj : seq["entry"]) {
            Step step;
            decode_step_from_json(step, stepj);

            steps.emplace_back(step);
        }
        sequence.entry = steps;
    }
    // Process each entry
    if(seq.contains("exit"))
    {
        steps.clear();
        for (const auto& stepj : seq["exit"]) {
            Step step;
            decode_step_from_json(step, stepj);

            steps.emplace_back(step);
        }
        sequence.exit = steps;
    }
    // Add sequence to the sequence map
    sequence_map[sequence_name] = sequence;
    return 0;
}


void setup_sequences_from_json(const std::string& json_config) 
{
    //std::cout << "Setting up json object from " << json_config << std::endl;

    json config = json::parse(json_config);
    //std::cout << "OK Setup json object from " << json_config << std::endl;

    if(config.is_array())
    {
        std::cout << "parsing array " << std:: endl;
// Iterate through each sequence in the configuration
        for (const auto& seq : config) {
            if (!seq.contains("sequence_name") || !seq.contains("steps")) {
                throw std::runtime_error("Invalid sequence configuration: Missing required fields.");
            }
            Sequence sequence;
            std::string sequence_name = seq["sequence_name"];
            //saved_triggers_map[sequence_name] = config;
            sequence.name = sequence_name;
            bool running = seq.value("running", false); // Default to false if not specified
            std::vector<Step> steps;

            // Process each step
            if(seq.contains("steps"))
            {
                steps.clear();
                for (const auto& stepj : seq["steps"]) {
                    Step step;
                    decode_step_from_json(step, stepj);

                    steps.emplace_back(step);
                }
                sequence.steps = steps;
            }
            // Process each entry
            if(seq.contains("entry"))
            {
                steps.clear();
                for (const auto& stepj : seq["entry"]) {
                    Step step;
                    decode_step_from_json(step, stepj);

                    steps.emplace_back(step);
                }
                sequence.entry = steps;
            }
            // Process each entry
            if(seq.contains("exit"))
            {
                steps.clear();
                for (const auto& stepj : seq["exit"]) {
                    Step step;
                    decode_step_from_json(step, stepj);

                    steps.emplace_back(step);
                }
                sequence.exit = steps;
            }
            // Add sequence to the sequence map
            sequence_map[sequence_name] = sequence;
        }
    }
}

void setup_sequence()
{

    Sequence relay_sequence;
    relay_sequence.name = "relay_sequence",
    relay_sequence.steps.push_back(Step("set", "bits:4", 1));
    relay_sequence.steps.push_back(Step("wait_reset", "", 0, "", 200, 0)); // Counter 0, wait 200ms
    relay_sequence.steps.push_back(Step("set", "bits:6", 1));
    relay_sequence.steps.push_back(Step("wait_reset", "", 0, "", 200, 0));

    // Add to the sequence map
    sequence_map["relay_sequence"] = relay_sequence;
}

void start_sequence(const std::string& sequence_name, int rack_num) 
{
    // Check if the sequence exists in the sequence map
    if (sequence_map.find(sequence_name) == sequence_map.end()) {
        std::cerr << "Error: Sequence '" << sequence_name << "' not found." << std::endl;
        return;
    }
    std::cout << "Running Sequence " << sequence_name << " rack " << rack_num << std::endl;

    Sequence& sequence = sequence_map[sequence_name];
    if (sequence.current_steps.find(rack_num) == sequence.current_steps.end())
        sequence.current_steps[rack_num]=0;
    if (sequence.running_states.find(rack_num) == sequence.running_states.end())
        sequence.running_states[rack_num]=false;
    // Retrieve the sequence
    int current_step = sequence.current_steps[rack_num];


    // Initialize the sequence
    current_step = 0;
    sequence.running_states[rack_num] = true; // Mark sequence as completed for this rack

    sequence.current_steps[rack_num] = 0;
    // set state to 1;
    sequence.sq_vals[rack_num]["state"] = 1;

    std::cout << ">>>Starting sequence: " << sequence_name
              << (rack_num >= 0 ? " for rack " + std::to_string(rack_num) : "") << std::endl;
    current_step = 0;
    //int rack_num = 0;
    for (auto& step : sequence.entry)
    {
        std::cout << "running entry step current step " << current_step <<  std::endl; 
        run_step_key(sequence, step, sequence.name, current_step, rack_num);
    }
    sequence.current_steps[rack_num] = 0;

    // Save the updated sequence back into the sequence map
    //sequence_map[sequence_name] = sequence;
}


void stop_sequence(const std::string& sequence_name, int rack_num) 
{
    // Check if the sequence exists in the sequence map
    if (sequence_map.find(sequence_name) == sequence_map.end()) {
        std::cerr << "Error: Sequence '" << sequence_name << "' not found." << std::endl;
        return;
    }

    Sequence& sequence = sequence_map[sequence_name];


    // Initialize the sequence
    int current_step = 0;
    // sequence.running_states[rack_num] = true; // Mark sequence as completed for this rack

    // sequence.current_steps[rack_num] = 0;
    // // set state to 1;
    // sequence.sq_vals[rack_num]["state"] = 0;

    std::cout << "Ending  sequence: " << sequence_name
              << (rack_num >= 0 ? " for rack " + std::to_string(rack_num) : "") << std::endl;
    //current_step = 0;
    //int rack_num = 0;
    for (auto& step : sequence.exit)
    {
        std::cout << "running exit step current step " << current_step <<  std::endl; 
        run_step_key(sequence, step, sequence.name, current_step, rack_num);
    }
    sequence.current_steps[rack_num] = 0;

    // Save the updated sequence back into the sequence map
    //sequence_map[sequence_name] = sequence;
}


bool evaluate_condition(const std::string& oper, int src_val, int value) {
    if (oper == "=") return src_val == value;
    else if (oper == ">") return src_val > value;
    else if (oper == ">=") return src_val >= value;
    else if (oper == "<") return src_val < value;
    else if (oper == "<=") return src_val <= value;
    else if (oper == "and") return src_val & value;
    else if (oper == "or") return src_val | value;
    return false;
}



bool get_sequence(Sequence &sequence, std::string name)
{
    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return false;
    }
    sequence = sequence_map[name];
    return true;
}


bool set_sequence_state(std::string name, int state, int step, int rack_num)
{
    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return false;
    }
    std::cout << " set state  sequence [" << name <<"]" << std::endl; 
    Sequence& sequence = sequence_map[name];
    sequence.sq_vals[rack_num]["state"] = state;
    sequence.sq_vals[rack_num]["current_step"] = step;    
    sequence.sq_vals[rack_num]["running"] = state;
 
    return true;
}

bool show_sequence_state(std::string name, std::string desc, int cycle, int rack_num)
{
    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return false;
    }
    std::cout << desc << " show sequence state [" << name <<"]"; 
    Sequence& sequence = sequence_map[name];
    int state = sequence.sq_vals[rack_num]["state"];
    int step = sequence.sq_vals[rack_num]["current_step"];    
    auto running = sequence.sq_vals[rack_num]["running"];
    std::cout 
                << " cycle [" << cycle <<"]" 
                << " state [" << state <<"]" 
                <<  " step [" << step <<"]"
                <<  " running [" << running <<"]"
                << std::endl; 
 
    return true;
}

// given a seqence object run (on a rack) until all the steps are complete 
// do this by rack num
//void setup_sequences_from_json(const std::string& json_config) {

int run_agg_sequence(std::string name)
{
    std::cout << " run_agg_seq running  " << name <<  std::endl; 

    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return -1;
    }
    Sequence& sequence = sequence_map[name];
    int idx = 0;

    std::cout << " run_agg_sequence running steps " << std::endl; 
    for (auto & step : sequence.steps)
    {
        std::cout << " running step  [" << idx <<"]" << std::endl; 
        int rack_num = -1;
        run_step_key(sequence, step, name, idx, rack_num);
        idx++;
    }
    idx  = 0;
    std::cout << " run_agg running exit " << std::endl; 
    for (auto & step : sequence.exit)
    {
        //std::cout << " running exit  [" << idx <<"]" << std::endl; 
        int rack_num = -1;
        run_step_key(sequence, step, name, idx, rack_num);
        idx++;
    }
    std::cout << " run_agg done " << std::endl; 
    

    // if (current_step >= sequence.steps.size()) {
    //     state = 0;
    //     sequence.sq_vals[rack_num]["running"] = 0;
    //     std::cout << "Sequence [" << name << "] completed for rack " << rack_num << "." << std::endl;
    //     sequence.sq_vals[rack_num]["state"] = 0; // idle
    //     sequence.sq_vals[rack_num]["current_step"] = 0;
    //     return;
    // }

    // //std::cout << "Sequence [" << name << "] running step " << current_step << " for rack " << rack_num << "." << std::endl;
    // Step& step = sequence.steps[current_step];
    // run_step_key(sequence, step, name, current_step, rack_num);
    // //run_step(sequence, step, name, current_step, rack_num);


    // // Save and increment the step for this rack
    // //sequence.current_steps[rack_num] = 
    // //current_step++;
    // sequence.sq_vals[rack_num]["current_step"] = current_step;
    return idx;
}


int execute_sequence_step(std::string name, int rack_num)
{
    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return -1;
    }
    Sequence& sequence = sequence_map[name];
    if(sequence.sq_vals.find(rack_num) == sequence.sq_vals.end())
    {
        sequence.sq_vals[rack_num]["state"] = 0;
        sequence.sq_vals[rack_num]["current_step"] = 0;    
        std::cout << " looks like we had to set up sequence [" << name <<"] for rack ["<<rack_num <<"]" << std::endl; 
    }
    //sequence.current_steps[rack_num];
    int state = sequence.sq_vals[rack_num]["state"];
    int current_step = sequence.sq_vals[rack_num]["current_step"];
    bool running  = sequence.running_states[rack_num]; // Mark sequence as completed for this rack

    // Skip sequences not running for this rack
    if (state == 0) {
        std::cout << "Sequence [" << name << "] state 0  for " << rack_num << "." << std::endl;
        return -1;
    }
    // if(running)
    //     std::cout << " running sequence [" << name <<"] rack [" << rack_num <<"] state ["<< state <<"] current step ["<< current_step << "]" << std::endl; 
    // else
    //     std::cout << " not running sequence [" << name <<"] rack return;[" << rack_num <<"] state ["<< state <<"] current step ["<< current_step << "]" << std::endl; 

    if (current_step >= sequence.steps.size()) {
        state = 0;
        sequence.sq_vals[rack_num]["running"] = 0;
        std::cout << "Sequence [" << name << "] completed for rack " << rack_num << "." << std::endl;
        sequence.sq_vals[rack_num]["state"] = 0; // idle
        sequence.sq_vals[rack_num]["current_step"] = 0;
        return -1;
    }

    //std::cout << "Sequence [" << name << "] running step " << current_step << " for rack " << rack_num << "." << std::endl;
    Step& step = sequence.steps[current_step];
    run_step_key(sequence, step, name, current_step, rack_num);
    //run_step(sequence, step, name, current_step, rack_num);


    // Save and increment the step for this rack
    //sequence.current_steps[rack_num] = 
    //current_step++;
    sequence.sq_vals[rack_num]["current_step"] = current_step;
    return 0;
}
 

void execute_sequence_item(std::vector<std::string>& seq_list, std::string name, int rack_num)
{
    if(sequence_map.find(name) == sequence_map.end())
    {
        std::cout << " cannot find sequence [" << name <<"]" << std::endl; 
        return;
    }
    Sequence& sequence = sequence_map[name];
    if(sequence.sq_vals.find(rack_num) == sequence.sq_vals.end())
    {
        sequence.sq_vals[rack_num]["state"] = 0;
        sequence.sq_vals[rack_num]["current_step"] = 0;    
        std::cout << " looks like we had to set up sequence [" << name <<"] for rack ["<<rack_num <<"]" << std::endl; 
    }
    //sequence.current_steps[rack_num];
    int state = sequence.sq_vals[rack_num]["state"];
    int current_step = sequence.sq_vals[rack_num]["current_step"];
    bool running  = sequence.running_states[rack_num]; // Mark sequence as completed for this rack

    // Skip sequences not running for this rack
    if (state == 0) {
        //std::cout << "Sequence [" << name << "] state 0  for " << rack_num << "." << std::endl;
        return;
    }
    // if(running)
    //     std::cout << " running sequence [" << name <<"] rack [" << rack_num <<"] state ["<< state <<"] current step ["<< current_step << "]" << std::endl; 
    // else
    //     std::cout << " not running sequence [" << name <<"] rack return;[" << rack_num <<"] state ["<< state <<"] current step ["<< current_step << "]" << std::endl; 

    if (current_step >= sequence.steps.size()) {
        state = 0;
        sequence.sq_vals[rack_num]["running"] = 0;
        std::cout << "Sequence [" << name << "] completed for rack " << rack_num << "." << std::endl;
        sequence.sq_vals[rack_num]["state"] = 0; // idle
        sequence.sq_vals[rack_num]["current_step"] = 0;
        return;
    }

    //std::cout << "Sequence [" << name << "] running step " << current_step << " for rack " << rack_num << "." << std::endl;
    Step& step = sequence.steps[current_step];
    run_step_key(sequence, step, name, current_step, rack_num);
    //run_step(sequence, step, name, current_step, rack_num);


    // Save and increment the step for this rack
    //sequence.current_steps[rack_num] = 
    //current_step++;
    sequence.sq_vals[rack_num]["current_step"] = current_step;
}


void run_all_sequences(std::vector<std::string>& seq_list, int rack_num)
{
    for (auto seq: seq_list)
    {
        //std::cout << "processing sequence [" << seq << "]" << std::endl ;
        execute_sequence_item(seq_list, seq, rack_num);
    }
}

// aggregate triggers work for all racks
//  prepare a list of results
// std::string json_string = R"({
//     "aggregate": "each",
//     "src_ref": "bits:3",
//     "oper": "and",
//     "conditions": [
//         {"src": "rinput:3", "oper": ">", "value": 300},
//         {"src": "bits:5", "oper": "=", "value": 1}
//     ],
//     "run": "compound_sequence"
// })";

bool evaluate_trigger_name(std::vector<bool>& results, const std::string name, int rack_num)
{
    if (saved_triggers_map.find(name) == saved_triggers_map.end())
    {
        return false;
    }
    return evaluate_trigger(results, saved_triggers_map[name], rack_num);
}


bool evaluate_trigger(std::vector<bool>& results, const json& trigger, int rack_num) {

    // Evaluate for all racks if no specific rack is given
    if (trigger.contains("aggregate") && (rack_num < 0)) {
        bool final_result = false;
        results.clear();
        std::string aggregate = trigger.value("aggregate", "any"); // Default to "any"
        std::vector<bool> rack_results(rack_max, false);

        for (int rack_num = 0; rack_num < rack_max; rack_num++) {
            rack_results[rack_num] = evaluate_trigger(results, trigger, rack_num);
            results.push_back(rack_results[rack_num]);
            std::cout << "Evaluated trigger for rack " << rack_num << ": " << (rack_results[rack_num] ? "true" : "false") << std::endl;
            if (aggregate == "each" && rack_results[rack_num]) {
                if (trigger.contains("run_sequence")) {
                    std::string seq = strip_quotes(trigger["run_sequence"]);
                    std::cout << " trigger running sequence [" << seq << "] for rack "<< rack_num << std::endl;
                    start_sequence(seq, rack_num); // 
                    final_result = false;
                }
                if (trigger.contains("run_trigger")) {
                    std::string seq = strip_quotes(trigger["run_trigger"]);
                    std::cout << "trigger run_trigger  [" << seq << "] for rack "<< rack_num << std::endl;
                    final_result = evaluate_trigger(results, seq, rack_num); // 
                    //final_result = false;
                }
                if (trigger.contains("run")) {
                    std::string seq = strip_quotes(trigger["run"]);
                    std::cout << "trigger run   [" << seq << "] for rack "<< rack_num << std::endl;
                    //final_result = evaluate_trigger(results, seq, rack_num); // 
                    //final_result = false;
                }
            }                
        }

        // Apply trigger aggregation looks for any or all 
        if (aggregate == "any") {
            final_result = std::any_of(rack_results.begin(), rack_results.end(), [](bool val) { return val; });
        } else if (aggregate == "all") {
            final_result = std::all_of(rack_results.begin(), rack_results.end(), [](bool val) { return val; });
        } else if (aggregate == "sum") {
            final_result = std::accumulate(rack_results.begin(), rack_results.end(), 0) > 0;
        } else if (aggregate == "none") {
            final_result = std::none_of(rack_results.begin(), rack_results.end(), [](bool val) { return val; });
        } else if (aggregate == "avg") {
            int true_count = std::count(rack_results.begin(), rack_results.end(), true);
            final_result = (true_count > 0) && (true_count >= (rack_max / 2)); // Example average logic
        }

        return final_result;
    }

    // Evaluate conditions if present
    if (trigger.contains("conditions")) {
        std::string oper = trigger.value("oper", "and");
        //std::cout << " condition oper " << oper << std::endl;
        bool result = (oper == "and");

        for (const auto& condition : trigger["conditions"]) {
            bool eval = evaluate_trigger(results, condition, rack_num);
            if (oper == "and") result &= eval;
            if (oper == "or") result |= eval;
        }
        return result;
    }

    // Evaluate base condition
    int src_val = get_trigger_val(strip_quotes(trigger["src"]), rack_num);
    //std::cout << "evaluate_trigger src_val [" << src_val << "] for rack "<< rack_num << std::endl;

    int value = get_trigger_val(trigger["value"], rack_num);
    //std::cout << "evaluate_trigger value [" << value << "] for rack "<< rack_num << std::endl;

    std::string oper = trigger["oper"];
    //std::cout << " condition src [" << trigger["src"] << "] oper [" << oper << "] src_val "<< src_val<<" value "<< value <<std::endl;

    if (oper == "=") return src_val == value; 
    else if (oper == ">" && src_val > value)
    {
        //std::cout << " returning true"<< std::endl;
        return true;
    }
    else if (oper == "<") return src_val < value;
    else if (oper == "reset_run") {
        // if (src_val) {
        //     set_val(trigger["src"], 0, rack_num);
        //     return true;
        // }
        return false;
    }

    return false;
}



// //#### **`process_triggers`: Process Triggers with Rack and System Logic**
// //Handles both rack-specific and system-wide destinations.
// void process_triggers(const json& triggers) {
//     for (const auto& trigger : triggers) {
//         if (trigger["src"].find("rinput") == 0 || trigger["src"].find("rack:sm16") == 0) {
//             // Evaluate for each rack
//             for (int rack_num = 0; rack_num < rack_max; ++rack_num) {
//                 if (evaluate_trigger(trigger, rack_num)) {
//                     std::string sequence = trigger["run"];
//                     start_sequence(sequence, rack_num); // Pass rack_num for rack-specific sequences
//                 }
//             }
//         } else {
//             // Evaluate as system-wide trigger
//             if (evaluate_trigger(trigger)) {
//                 std::string sequence = trigger["run"];
//                 start_sequence(sequence); // System-wide sequence
//             }
//         }
//     }
// }

// //#### **Aggregation Logic for System Variables**
// //Add support for system-wide aggregation modes (`sum`, `avg`, `any`, `all`, `none`).

// int aggregate_results(const std::string& mode, const std::vector<bool>& results) {
//     if (mode == "any") return std::any_of(results.begin(), results.end(), [](bool val) { return val; });
//     if (mode == "all") return std::all_of(results.begin(), results.end(), [](bool val) { return val; });
//     if (mode == "none") return !std::any_of(results.begin(), results.end(), [](bool val) { return val; });
//     if (mode == "sum") return std::count(results.begin(), results.end(), true);
//     if (mode == "avg") return std::count(results.begin(), results.end(), true) / static_cast<int>(results.size());
//     //throw std::invalid_argument("Invalid aggregation mode: " + mode);
//     return 0;
// }



// json j1 ={"src": "rinput:5","oper": ">","value": 400,"run": "rack_specific_sequence"};

// json  j2 =  
// {
//     "src": "rinput:10","oper": ">","value": 500,"dest": "bits:15",
//     "dest_mode": "any", // Aggregates results from all racks
//     "run": "system_sequence"
// };

//#### **Compound Trigger**
// the word aggregate implies that we shild run this for all the racks

// sample trigger
// this is called the test_trigger
// run the state machine called "compund_sequence" for each of these triggers 
// see _triggers


void log_sequence_progress(const std::string& sequence_name, int step) {
    std::cout << "Sequence: " << sequence_name << " | Current Step: " << step << std::endl;
}

void log_operation(const json& command) {
    std::cout << "Executing Operation: " << command.dump() << std::endl;
}

void state_machine_loop() {
    while (true) {
        // process_triggers(triggers, sequences, bits, inputs);
        // execute_sequences(sequences, counters, bits, registers);
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}


// void print_sequence(const Sequence& seq) {
//     std::cout << "Sequence Name: " << seq.name << "\n";
// //    std::cout << "Starting Step: " << seq.step << "\n";
//     for (const auto& step : seq.steps) {
//         std::cout << "  Step: " << step.oper
//                   << ", Src: " << (step.src.empty() ? "None" : step.src)
//                   << ", Dest: " << (step.dest.empty() ? "None" : step.dest)
//                   << ", Value: " << step.value
//                   << ", Counter: " << step.counter
//                   << ", Time: " << step.time
//                   << "\n";
//     }
// }


//void set_var
template <typename T>
void set_def_var(const std::string& var_ref, T value, int rack_number)
{
    VarDef def;
    // Decode the variable reference string into a `VarDef`
    //std::cout << " decode var type [" << var_ref <<"] "<< std::endl;
    decode_var_type(def, var_ref);
    //std::cout << " setting var [" << var_ref <<"] to ["<< value << "]" << std::endl;

    set_var<T>(def, value, rack_number);
}

template <typename T>
T get_def_var(const std::string& var_ref, int rack_number)
{
    VarDef def;
    // Decode the variable reference string into a `VarDef`
    int ret = decode_var_type(def, var_ref);
    //std::cout << " def var "<<var_ref << " decoded ret " << ret << std::endl;
    if (ret > 0)
    {
        set_var<T>(def, 0, rack_number);
    }
    return get_var<T>(def, rack_number);
}





// Function to read values from racks and keep them with rack numbers
std::vector<std::pair<int, int>> read_vec_values(int vec_max) {
    std::vector<std::pair<int, int>> vec_values;

    for (int vec_num = 0; vec_num < vec_max; ++vec_num) {
        // Example: Simulate reading a value for each rack
        int value = rand() % 100; // Replace with actual rack value reading logic
        vec_values.emplace_back(vec_num, value);
    }

    return vec_values;
}

// Function to sort the vector and return the sorted rack numbers
// used for finding groups of vars
std::vector<int> sort_and_get_vec_numbers(std::vector<std::pair<int, int>>& vec_values, bool ascending = true) {
    // Sort the vector based on the value (second element of the pair)
    if (ascending) {
        std::sort(vec_values.begin(), vec_values.end(), 
                  [](const auto& a, const auto& b) { return a.second < b.second; });
    } else {
        std::sort(vec_values.begin(), vec_values.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
    }

    // Extract sorted rack numbers
    std::vector<int> sorted_vec_numbers;
    for (const auto& pair : vec_values) {
        sorted_vec_numbers.push_back(pair.first);
    }

    return sorted_vec_numbers;
}


int test_vec_sort() {
    int vec_max = 20;

    // Read rack values
    std::vector<std::pair<int, int>> vec_values = read_vec_values(vec_max);

    // Print original values
    std::cout << "Original vec values:" << std::endl;
    for (const auto& [vec, value] : vec_values) {
        std::cout << "Vec " << vec << ": " << value << std::endl;
    }

    // Sort and get rack numbers in ascending order
    std::vector<int> sorted_vec_numbers = sort_and_get_vec_numbers(vec_values, true);

    // Print sorted vec values
    std::cout << "\nSorted vec values (ascending):" << std::endl;
    for (const auto& [vec, value] : vec_values) {
        std::cout << "Vec " << vec << ": " << value << std::endl;
    }

    // Print sorted vec numbers
    std::cout << "\nSorted vec numbers:" << std::endl;
    for (int vec : sorted_vec_numbers) {
        std::cout << "Vec " << vec << std::endl;
    }

    return 0;
}


int main(int argc , char* argv[])
{

    std::cout << " test starting" <<std::endl;
    bool crash = false;
    json j;
    std::string trigger_name;
    set_modbus_vars();


    myassert(local_var_vec.size() == MAX_RACK+1,"local var vec size ", true);
    {
        set_def_var<int>("local_var", 0, -1);
        int val = get_def_var<int>("local_var", -1);
        //std::cout << " set_def_var 0 get_def_var ["<<val<<"]" <<std::endl;
        myassert(val == 0,"var.local var test", true);
    }

    test_local_vars(crash);

    j = json::parse(rack_online);
    trigger_name = j.value("trigger_name", "");
    if(!trigger_name.empty())
    {
        saved_triggers_map[trigger_name] = j;
    }


    j = json::parse(json_string);
    trigger_name = j.value("trigger_name", "");
    if(!trigger_name.empty())
    {
        saved_triggers_map[trigger_name] = j;
    }
    std::cout << " parse ok" <<std::endl;

    {
        set_def_var<int>("bits:5", 0);
        int val = get_def_var<int>("bits:5");
        //std::cout << " set_def_var 0 get_def_var ["<<val<<"]" <<std::endl;
        myassert(val == 0,"var.modbus.test_bits_5_0", crash);
    }
    {    
        set_def_var<int>("bits:5", 1);
        int val = get_def_var<int>("bits:5");
        myassert(val == 1,"var.modbus.test_bits_5_1", crash);
    }
    {
        set_def_var<float>("myfloat", 1234);
        float myval = get_def_var<float>("myfloat");
        //std::cout << " set_float 1234 get_def_var ["<<myval<<"]" <<std::endl;
        myassert(myval == 1234,"var.global.test_myfloat_1234", crash);
    }
    //return 0;

    set_def_var<int>("input:3", 245);
    set_def_var<int>("bits:5", 0);
    //std::cout << " set_def_var ok" <<std::endl;
    int v1 = get_def_var<int>("input:3");
    int v2 = get_def_var<int>("bits:5");
    myassert(v1 == 245,"var.modbus.test_input_3_245", crash);
    myassert(v2 == 0,"var.modbus.test_bits_5_0", crash);


    set_def_var<int>("rinput:3", 290, 2);
    set_def_var<int>("rinput:3", 990, 6);
    v1 = get_def_var<int>("rinput:3", 2);
    v2 = get_def_var<int>("rinput:3", 6);
    myassert(v1 == 290,"var.modbus.rack_2_input_3_290", crash);
    myassert(v2 == 990,"var.modbus.rack_6_input_3_990", crash);

    // write_rack_mb(MB_INPUT, 3, 290, 2); // Fetch rack-specific input
    // write_rack_mb(MB_INPUT, 3, 990, 6); // Fetch rack-specific input
    //std::cout << " write_rack_mb ok" <<std::endl;

    //int myval2 = get_def_var<int>("rinput:3", 2);
    //int myval6 = get_def_var<int>("rinput:3", 6);
    //std::cout << " get 290 myval2 ["<<myval2<<"]" <<std::endl;
    //std::cout << " get 990 myval6 ["<<myval6<<"]" <<std::endl;
    //return 0;
    std::vector<bool>results;
    auto v = evaluate_trigger_name(results, "test_trigger", -1);
    myassert(v == false,"eval trigger expect false", crash);
    for(int idx = 0; idx < results.size(); idx++ )
    {
        {
            std::ostringstream oss;
            oss << "eval trigger #1 idx <" << idx << "> expect false";

            std::string res = oss.str();
            myassert(results[idx] == false, res, crash);
        }
    }


    std::cout << " test 1 value is " << (v?"true":"false") <<std::endl;
 
    set_def_var<int>("rinput:3", 310, 2);
    set_def_var<int>("bits:5", 1);
    v1 = get_def_var<int>("rinput:3", 2);
    std::cout << " rinput:2 rack 2 value now " << v1 <<std::endl;
    v2 = get_def_var<int>("bits:5");
    std::cout << " bits:5 value now " << v2 <<std::endl;

    v = evaluate_trigger_name(results, "test_trigger", -1);
    myassert(results.size() == 12," expect results to be 12", crash);

    for(int idx = 0; idx < results.size(); idx++ )
    {
        if(idx == 2 || idx == 6)
        {
            std::ostringstream oss;
            oss << "eval trigger #2 idx <" << idx << "> expect true";

            std::string res = oss.str();
            myassert(results[idx] == true,res, crash);
        }
        else
        {
            std::ostringstream oss;
            oss << "eval trigger #2 idx <" << idx << "> expect false";

            std::string res = oss.str();
            myassert(results[idx] == false, res, crash);
        }
    }
    std::cout << " test 2 value is " << (v?"true":"false") <<std::endl;

    //return 0;


    setup_sequences_from_json(seq_config);
    std::vector<std::string>seq_list;
    //start_sequence("test_sequence", -1);


    seq_list.push_back("test_sequence");
    run_all_sequences(seq_list, -1);
    start_sequence("test_sequence", -1);

    // number of cycles
    for (int i = 0 ; i < 20; i++)
    {  
        std::cout << "running cycle [" << i << "]" << std::endl;
        // for all racks
        for (int j = 0 ; j < 3; j++)
        {
            if(i == 1)
                start_sequence("test_sequence", j);

            run_all_sequences(seq_list, j);
        }
    }

    // execute_sequence_item
    //std::cout << " running sequence [" << name <<"] rack [" << rack_num <<"] state ["<< state <<"] current step ["<< current_step << "]" << std::endl; 

    set_def_var<int>("g_volt_max", 2700, -1);
    set_def_var<int>("g_volt_min", 2500, -1);

    setup_sequences_from_json(test_agg);
    setup_sequences_from_json(test_max);
    //Sequence seq;
    int rval;


    //if(get_sequence(seq , "test_agg"))
    {
        for (int rnum = 0; rnum < 12 ; rnum++)
        {
            rval = myrand(2000, 3500);
            set_def_var("rinput:1", rval, rnum);
            rval = myrand(200, 350);
            set_def_var("rinput:2", rval, rnum);
        }
    }

    // special run for agg sequence 
    set_def_var("g_num_racks", 12, -1);
    


    int res;
    res = run_agg_sequence("test_agg");
    res = run_agg_sequence("test_max");

    //std::cout << " sequence should be set up for each rack"<< std::endl;

    // this will be done from the online trigger
    // seq_list.clear();
    // for (int rnum = 0; rnum < 12 ; rnum++)
    // {
    //     seq.sq_vals[rnum]["state"] = 1;
    //     int state = seq.sq_vals[rnum]["state"];
    //     std::cout << " start set  sequence state " << state << " for rack "<< rnum << std::endl;

    //     execute_sequence_item(seq_list, "test_agg", rnum);
    //     state = seq.sq_vals[rnum]["state"];
    //     std::cout << " end got  sequence state " << state << " for rack "<< rnum << std::endl;

    // }

    // stop_sequence("test_agg", -1);


    print_global_vars();
    print_local_vars();
    print_modbus_vars();

    results.clear();
    for(int i = 0; i < 12; i++) 
    {
        bool online = evaluate_trigger_name(results, "rack_online", i);

        if(online)
        {
            std::cout << " rack [" << i << "] is online "<<std::endl;  
        }
        else
        {
            std::cout << " rack [" << i << "] is offline "<<std::endl;  
        }
    }

    test_new_var_system();
    test_decode_step_var();

    std::cout << "\n\n**************************************"<<std::endl;
    test_file_load();

    
    // look for the entry test/test_rack_online.json in  
// Attempt to find and retrieve the JSON data
    json tj;
    bool json_found = find_json_map(tj, "tests", "test_rack_online.json");
    myassert(json_found == true, " test json file found ", false);
    if (json_found)
    {
        std::cout << "Found JSON: " << tj.dump(4) << std::endl;

        std::cout << "\n\n******************************" << std::endl;
        int res = setup_sequence(tj);
        myassert(res == 0, " test rack sequence decoded", false);
        start_sequence("test_rack_online", -1);
        //set_sequence_state set_sequence_state(std::string name, int state, int step, int rack_num)

        set_sequence_state("test_rack_online", 1, 0, -1);
        show_sequence_state("test_rack_online", " step ", 0, -1);
        for ( int j = 0 ; j < 10 ;j++)
        {
            res = execute_sequence_step("test_rack_online", -1);
            show_sequence_state("test_rack_online", " step ", j,  -1);
        } 
    }
    
    // now we need to decode that as a sequence
    

    std::cout << "\n\n**************************************"<<std::endl;

    assert_manager.generate_report();

    std::cout << "\n\n**************************************"<<std::endl;
    assert_manager.generate_summary();
    std::cout << "\n\n**************************************"<<std::endl;



    return 0;
}