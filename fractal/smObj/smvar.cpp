
/**
 ******************************************************************************
 * @Copyright (C), 2024, Fractal EMS Inc. 
 * @file: smvar.cpp
 * @author: Phil Wilshire
 * @Description:
 *     Shared Memory Objets
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2024.12.30
 ******************************************************************************
 */

// todo 
// offset  -- used get_str_offset
// 
// ws link partially done
// test rack alarms
// fix rack_max done
// sbObj, give it a map and a vector, recursive parsing complete. but needs testing 

#include "Common.h"
#include <fstream>
#include <string>
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm> // For std::replace
#include <memory>
#include <vector>
#include <mutex>
#include <ctime>
#include <cstdint>
#include <cstring> // for memcpy


#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <modbus/modbus.h>
//#include "cmsis_gcc.h"
//#include <arm_acle.h>  
#ifdef __aarch64__
#include <arm_acle.h>  
#endif

#include <nlohmann/json.hpp>

#include "FractalShm.h"
#include "shm_mem.h"
#include "rack_mem.h"
#include "fractal_inc.h"
#include "build_info.h"
#include "smvar.h"


using json = nlohmann::json;

extern int rtos_size;// = RTOS_MEM_SIZE;
extern int rack_size;// = RACK_MEM_SIZE;
extern int sbmu_size;// = SBMU_MEM_SIZE;
extern int rack_max;// = RACK_MAX;
extern void* shm_rtos;// = nullptr; // orig rtos_data
extern void* shm_rack;// = nullptr;  // new rack data area mtype = 1
extern void* shm_sbmu;// = nullptr;  // new sbmu data area mtype = 2

int lcl_rack_max;// = RACK_MAX;

// Define the operation function signature
//typedef int (*OperationFunc)(std::map<std::string, smObj>&, std::vector<smObj>&, std::vector<smObj>&);


// function map to allow functipns to be acesses via strings 
std::map<std::string, int(*)(std::map<std::string, smObj>&, std::vector<smObj>&, std::vector<smObj>&)> function_map;
std::map<std::string, int(*)(opDef&, std::vector<smObj*>*, std::vector<smObj*>*)> func_map;
std::map<std::string, opDef*> opDef_map;  // Map to store operation definitions
std::map<std::string, smVar*> smVar_map;  // Map to store smVar definitions

int num_opdefs = 0;

// this are  virtual machines to perform specific tasks
// operation definitions
#define NUM_OPDEFS 1024
opDef opdefs[NUM_OPDEFS];

// these are the variables used to control the machines.
int num_smvars = 0;
#define NUM_SMVARS 1024
smVar smvars[NUM_SMVARS];

// Function to parse smVar from JSON
void parse_smVar(smVar* smvar, json j );


// Function to register a name for operation functions
void register_func(const std::string& function_name, OpDefFunc func)
{
    func_map[function_name] = func;
    std::cout << "Function '" << function_name << "' registered successfully!" << std::endl;
}

// this is run by the ops code to get values from modbus or dirtect sm interfaces
int lcl_get_mem_int_val_rack(const char* rack_name, int rack_num, const char* reg_type, int offset)
{
    int value = 0xffff;
    //printf(" [%s] rack_name [%s]\n", __func__, rack_name);
    std::vector<int>values;
    std::string mem_type="";
    std::string rack_name_str(rack_name);
    std::string reg_type_str(reg_type);
    get_mem_int_val_rack(rack_name_str, rack_num, mem_type, reg_type_str, offset, 0, values);
    std::cout   << " rack name [" << rack_name_str 
                << "]  reg_type [" << reg_type_str 
                << "] offset ["<< (int)offset
                << "] "; 

    if(values.size() > 0)
    {
        value = values[0];
        std::cout<< " value [" << (int)value<< "]";
    }
    std::cout<< std::endl;

    return value;
}

// Recursive function to parse nested smObj using nlohmann JSON
int parse_smobj(const json& obj_json, smObj* smobj) {
    // may be just key and value 
    // Parse simple fields
    int is_obj = 0;
    // if (obj_json.value().is_boolean()) {
    //     smobj->bool_val = obj_json.value().get<bool>();
    //     smobj->is_bool = true;
    // } else if (obj_json.value().is_number_integer()) {
    //     smobj->int_val = obj_json.value().get<int>();
    //     smobj->is_integer = true;
    // } else if (obj_json.value().is_number_float()) {
    //     smobj->float_val = obj_json.value().get<float>();
    //     smobj->is_float = true;
    // } else if (obj_json.value().is_string()) {
    //     smobj->string_val = obj_json.value().get<std::string>();
    //     smobj->is_string = true;
    // }

    if (obj_json.contains("name")) {
        smobj->name = obj_json["name"].get<std::string>();
        is_obj = 1;
    }
    if (obj_json.contains("desc")) {
        smobj->desc = obj_json["desc"].get<std::string>();
        is_obj = 1;
    }

    if (obj_json.contains("value")) {
        is_obj = 1;

        if (obj_json["value"].is_boolean()) {
            smobj->bool_val = obj_json["value"].get<bool>();
            smobj->is_bool = true;
        } else if (obj_json["value"].is_number_integer()) {
            smobj->int_val = obj_json["value"].get<int>();
            smobj->is_integer = true;
        } else if (obj_json["value"].is_number_float()) {
            smobj->float_val = obj_json["value"].get<float>();
            smobj->is_float = true;
        } else if (obj_json["value"].is_string()) {
            smobj->string_val = obj_json["value"].get<std::string>();
            smobj->is_string = true;
        }
    }

    // Parse obj_map recursively
    if (obj_json.contains("obj_map") && obj_json["obj_map"].is_object()) {
        is_obj = 1;

        const json& obj_map_json = obj_json["obj_map"];
        for (auto& item : obj_map_json.items()) {
            smObj* sub_obj = new smObj();
            parse_smobj(item.value(), sub_obj);  // Parse recursively
            smobj->obj_map[item.key()] = sub_obj;  // Add the sub-object to obj_map
        }
    }

    // Parse obj_vec (if any)
    if (obj_json.contains("obj_vec") && obj_json["obj_vec"].is_array()) {
        is_obj = 1;

        for (const auto& item : obj_json["obj_vec"]) {
            smObj* sub_obj = new smObj();
            parse_smobj(item, sub_obj);  // Parse each object in the array
            smobj->obj_vec.push_back(sub_obj);  // Add the sub-object to obj_vec
        }
    }
    return is_obj;
    // if (is_obj == 0) {
    //     if (obj_json.is_boolean()) {
    //         smobj->bool_val = obj_json.value().get<bool>();
    //         smobj->is_bool = true;
    //     } else if (obj_json.is_number_integer()) {
    //         smobj->int_val = obj_json.value().get<int>();
    //         smobj->is_integer = true;
    //     } else if (obj_json.is_number_float()) {
    //         smobj->float_val = obj_json.value().get<float>();
    //         smobj->is_float = true;
    //     } else if (obj_json.is_string()) {
    //         smobj->string_val = obj_json.value().get<std::string>();
    //         smobj->is_string = true;
    //     }
    // }
}

void parse_array(std::vector<std::shared_ptr<smObj>>& sm_objects, const json& j) {
    for (const auto& item : j) {
        if (item.is_object()) {
            // Create an smObj for this object
            auto obj = std::make_shared<smObj>("object");
            for (const auto& field : item.items()) {
                const std::string& key = field.key();
                const auto& value = field.value();

                if (value.is_object()) {
                    // If the value is an object, create a child smObj and recurse
                    auto child_obj = std::make_shared<smObj>(key);
                    parse_array(child_obj->s_obj_vec, value);
                    obj->s_obj_map[key] = child_obj;
                } else if (value.is_array()) {
                    // If the value is an array, recurse directly
                    auto child_obj = std::make_shared<smObj>(key);
                    parse_array(child_obj->s_obj_vec, value);
                    obj->s_obj_map[key] = child_obj;
                } else {
                    // Otherwise, store the value as a string
                    obj->s_obj_map[key] = std::make_shared<smObj>(key, value.dump());
                }
            }
            sm_objects.push_back(obj);
        } else if (item.is_array()) {
            // If the item itself is an array, recurse
            parse_array(sm_objects, item);
        } else {
            // Create an smObj for the primitive value
            sm_objects.push_back(std::make_shared<smObj>("value", item.dump()));
        }
    }
}

void parse_inputs(std::vector<std::shared_ptr<smObj>>& sm_objects, const json& j) {
    if (j.contains("inputs") && j["inputs"].is_array()) {
        parse_array(sm_objects, j["inputs"]);
    } else {
        std::cerr << "Error: 'inputs' is missing or not an array!" << std::endl;
    }
}

void parse_opdef_array(json j, const char* item_name) {
    // Check if "inputs" exists and is an array
    if (j.contains(item_name) && j[item_name].is_array()) {
        for (const auto& input : j[item_name]) {
            std::cout << "Input Object:" << std::endl;
            
            // Iterate through all keys and values in the current object
            for (const auto& item : input.items()) {
                std::string key = item.key();
                auto value = item.value();

                std::cout << "  Key: " << key << ", Value: ";

                // Dynamically determine the type of the value
                if (value.is_boolean()) {
                    std::cout << (value.get<bool>() ? "true" : "false");
                } else if (value.is_number_integer()) {
                    std::cout << value.get<int>();
                } else if (value.is_number_float()) {
                    std::cout << value.get<double>();
                } else if (value.is_string()) {
                    std::cout << value.get<std::string>();
                } else {
                    std::cout << "Unknown or Unsupported Type";
                }

                std::cout << std::endl;
            }
        }
    } else {
        std::cerr << "Error: 'inputs' is missing or not an array!" << std::endl;
    }
}

// create a named operation definition entity 
void parse_opdef(opDef *opdef, json j)
{
    //json j = json::parse(json_input);

    std::map<std::string, smObj*> sys_map;
    std::vector<smObj*> inputs;
    std::vector<smObj*> outputs;

    // Create the shared_ptr vectors for inputs and outputs
    //auto inputs = std::make_shared<std::vector<std::shared_ptr<smObj>>>();
    //auto outputs = std::make_shared<std::vector<std::shared_ptr<smObj>>>();

    // Parse the "sys" part
    // we want a map of named smObj items 
    // Parse the "sys" part from the JSON and convert it into a map of named smObj items
    // Check if "sys" exists and is an object
    if (j.contains("sys") && j["sys"].is_object()) {
        //std::cout << "sys is an object" << std::endl;
        for (auto& sys : j["sys"].items()) {
            // For each system configuration, create a smObj and add it to the sys_map
            std::string sys_name = sys.key();
            std::cout << "   sys_name  " << sys_name << std::endl;
            std::cout << "           sys  " << sys << std::endl;

            smObj* smobj = new smObj(sys_name, "Description of " + sys_name, nullptr, 0, 0, true);  // Default values for smObj
            //auto smobj = std::make_shared<smObj>(sys_name, "Description of " + sys_name, nullptr, 0, 0, true);
            smobj->init = false;
            int is_obj = parse_smobj(sys, smobj);
            if(is_obj == 0)
            {
                if (sys.value().is_boolean()) {
                    smobj->bool_val = sys.value().get<bool>();
                    smobj->is_bool = true;
                } else if (sys.value().is_number_integer()) {
                    smobj->int_val = sys.value().get<int>();
                    smobj->is_integer = true;
                } else if (sys.value().is_number_float()) {
                    smobj->float_val = sys.value().get<float>();
                    smobj->is_float = true;
                } else if (sys.value().is_string()) {
                    smobj->string_val = sys.value().get<std::string>();
                    smobj->is_string = true;
                }
            }
            sys_map[sys_name] = smobj;
        }
    }

    // Parse inputs only if we have inputs defined
    for (auto& input : j["inputs"]) {
        std::string input_name = input["name"];
        smObj *smobj = new smObj(input_name, "Description of " + input_name);
        //auto smobj = std::make_shared<smObj>(sys_name, "Description of " + sys_name, nullptr, 0, 0, true);

        smobj->init = false;
        //parse_smobj(input, smobj);
        int is_obj = parse_smobj(input, smobj);
        if(is_obj == 0)
        {
            // if (input.value().is_boolean()) {
            //     smobj->bool_val = input.value().get<bool>();
            //     smobj->is_bool = true;
            // } else if (input.value().is_number_integer()) {
            //     smobj->int_val = input.value().get<int>();
            //     smobj->is_integer = true;
            // } else if (input.value().is_number_float()) {
            //     smobj->float_val = input.value().get<float>();
            //     smobj->is_float = true;
            // } else if (input.value().is_string()) {
            //     smobj->string_val = input.value().get<std::string>();
            //     smobj->is_string = true;
            //}
        }
        
        //std::cout << " added input smObj " << input_name << std::endl;
        inputs.push_back(smobj);
    }

    // Parse outputs (if any)
    for (auto& output : j["outputs"]) {
        std::string output_name = output["name"];
        smObj * smobj =  new smObj(output_name, "Description of " + output_name);
        smobj->init = false;
        int is_obj = parse_smobj(output, smobj);
        if(is_obj == 0)
        {
            // if (output.value().is_boolean()) {
            //     smobj->bool_val = output.value().get<bool>();
            //     smobj->is_bool = true;
            // } else if (output.value().is_number_integer()) {
            //     smobj->int_val = output.value().get<int>();
            //     smobj->is_integer = true;
            // } else if (output.value().is_number_float()) {
            //     smobj->float_val = output.value().get<float>();
            //     smobj->is_float = true;
            // } else if (output.value().is_string()) {
            //     smobj->string_val = output.value().get<std::string>();
            //     smobj->is_string = true;
            // }
        }

        parse_smobj(output, smobj);

        //std::cout << " added output smObj " << output_name << std::endl;
        outputs.push_back(smobj);
    }

    std::string func_name = "undefined";
    if (j.contains("func") && j["func"].is_string()) {

        // Retrieve the function name and execute the operation
        func_name = j["func"];
        opdef->func_name = func_name;
    }
    // Retrieve the function name and execute the operation
    std::string oper_name = j["name"];
    //opDef opdef;
    opdef->name = oper_name;
    opdef->sys_map = sys_map;
    opdef->inputs = inputs;
    opdef->outputs = outputs;

    // Register the operation in the map
    opDef_map[oper_name] = opdef;

    std::cout << "Operation '" << oper_name << "' registered with function '" << func_name << "'" << std::endl;
    std::cout << "Operation name  [" << opdef->name  << "]"<< std::endl;

}

// Helper function to split a string by a delimiter
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;
    
    while (std::getline(ss, part, delimiter)) {
        parts.push_back(part);
    }
    
    return parts;
}


// Set the int_val for a nested smObj using a dot notation path
void set_smobj_int_val(smObj& smobj, const std::string& path, int value) {
    std::vector<std::string> parts = split_string(path, '.');
    smObj* current_obj = &smobj;

    // Navigate through the path, creating objects if necessary
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const std::string& part = parts[i];

        // Check if the object exists, create it if it doesn't
        if (current_obj->obj_map.find(part) == current_obj->obj_map.end()) {
            current_obj->obj_map[part] = new smObj(part, "Description of " + part);
        }
        
        // Move to the next nested object
        current_obj = current_obj->obj_map[part];
    }

    // Set the int_val for the final part
    const std::string& last_part = parts.back();
    current_obj->int_val = value;
    current_obj->is_integer = true;

    std::cout << "Set " << path << " to " << value << std::endl;
}

// Get the int_val for a nested smObj using a dot notation path
int get_smobj_int_val(smObj& smobj, const std::string& path, int defval) {
    std::vector<std::string> parts = split_string(path, '.');
    smObj* current_obj = &smobj;

    // Navigate through the path
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];

        // Check if the object exists
        if (current_obj->obj_map.find(part) == current_obj->obj_map.end()) {
            std::cout << "Object not found for path: " << path << std::endl;
            return defval;  // Return default value if not found
        }

        // Move to the next nested object
        current_obj = current_obj->obj_map[part];
    }

    // Return the int_val if it exists
    if (current_obj->is_integer) {
        return current_obj->int_val;
    } else {
        std::cout << "No integer value found for path: " << path << std::endl;
        return defval;  // Return default value if the int_val is not set
    }
}

int test_smobj() {
    smObj root("root", "Description of root");

    // Set some values using the set_smobj_int_val function
    set_smobj_int_val(root, "obj1.obj2.intval", 234);
    set_smobj_int_val(root, "obj1.obj2.sub_obj.intval", 456);

    // Get the values using the get_smobj_int_val function
    int val = get_smobj_int_val(root, "obj1.obj2.intval", -1);
    std::cout << "Retrieved value: " << val << std::endl;

    val = get_smobj_int_val(root, "obj1.obj2.sub_obj.intval", -1);
    std::cout << "Retrieved value: " << val << std::endl;

    // Test non-existing path, should return the default value (-1)
    val = get_smobj_int_val(root, "obj1.obj3.intval", -1);
    std::cout << "Retrieved value (default): " << val << std::endl;

    return 0;
}


int parse_opdef_str(std::string& opdef_str)
{
    opDef* opdef = &opdefs[num_opdefs++];
    json j = json::parse(opdef_str);
    parse_opdef(opdef, j);
    return 0;
}

int parse_smvar_str(std::string&json_input)
{
    smVar* smvar = &smvars[num_smvars++];
    json j = json::parse(json_input);
    parse_smVar(smvar, j); // Parse the JSON into smVar
    return 0;
}

opDef* find_opdef(const char* name)
{
     // Register the operation in the map
    if(opDef_map.find(name) != opDef_map.end())
    {
        return opDef_map[name];
    }
    return nullptr;
}

// this strucure defines a named use of a function 
bool get_opdef_map_int(int& val, opDef& opdef, const char* name)
{
    if (opdef.sys_map.find(name) != opdef.sys_map.end())
    {
        auto item = opdef.sys_map[name];
        if (item->is_integer)
        {
            val = item->int_val;
            return true;
        }
    }
    return false;
}

// looks for a named int value  in a named opdef
bool get_opdef_map_int_name(int& val, const char* def_name, const char* name)
{
    opDef* opdef =  find_opdef(def_name);
    if (opdef)
    {
        return get_opdef_map_int(val, *opdef, name);
    }   
    return false;
}



// this means passing the context and the inputs / outputs to the functipn
int run_opdef_func(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    //std::cout << " running opdef "<< opdef.name << " function " << opdef.func_name << std::endl;
    if (func_map.find(opdef.func_name) != func_map.end()) {
        std::cout << " running opdef "<< opdef.name << " function " << opdef.func_name << std::endl;
        func_map[opdef.func_name](opdef, inputs, outputs);
    }
    return 0;
}


opDef* run_opdef_name(const char* name, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    if(opDef_map.find(name) != opDef_map.end())
    {
        run_opdef_func(*opDef_map[name], inputs, outputs);
        return (opDef_map[name]);
    }
    return nullptr;
}



smVar *find_smvar(const char* name)
{
    if(smVar_map.find(name) != smVar_map.end())
    {
        return smVar_map[name];
    }
    return nullptr;
}

// add _xx to the name to define the rack , use leading zeros if < 10
smVar *find_rack_smvar(const char* name, int rack_num)
{
    std::string var_name(name);
    var_name += "_"; // Properly append rack_num as a string
    if(rack_num < 10)
    {
        var_name += "0";
    }        
    // Append the rack number to the variable name
    var_name += std::to_string(rack_num); // Properly append rack_num as a string
    if(smVar_map.find(var_name) != smVar_map.end())
    {
        return smVar_map[var_name];
    }
    return nullptr;
}


// Helper function to parse alarms (max, min, or stall) dynamically
int parse_smvar_alarm( Alarm* alarm, json alarm_json) {
    int i = 0;

    if (alarm_json.is_array()) {
        //std::cout << " json we found an array " << std::endl;
        // Get the number of items in the array
        size_t array_size = alarm_json.size();
        //std::cout << "Number of items in the array: " << array_size << std::endl;
        // Access each item in the array
        for (auto& item : alarm_json) {
 
            // If the item is an object, you can further process it
            if (item.is_object()) {
                // Example: process the object's key-value pairs
                for (auto& key_value : item.items()) {
                    //std::cout << "Key: " << key_value.key() << ", Value: " << key_value.value() << std::endl;
                    if(key_value.key() == "hist")
                    {
                        //std::cout << "Processing hist: " << key_value.value() << std::endl;
                        alarm->hist =  key_value.value();
                   }
                    else
                    {
                        if (i == 0) {
                            //std::cout << " alarm.l1 Key: " << key_value.key() << ", Value: " << key_value.value() << std::endl;

                            strncpy(alarm->l1_name, key_value.key().c_str(), sizeof(alarm->l1_name) - 1);
                            alarm->l1_level = key_value.value();
                        } else if (i == 1) {
                            //std::cout << " alarm.l2 Key: " << key_value.key() << ", Value: " << key_value.value() << std::endl;

                            strncpy(alarm->l2_name, key_value.key().c_str(), sizeof(alarm->l2_name) - 1);
                            alarm->l2_level = key_value.value();
                        } else if (i == 2) {
                            //std::cout << " alarm.l3 Key: " << key_value.key() << ", Value: " << key_value.value() << std::endl;
                            strncpy(alarm->l3_name, key_value.key().c_str(), sizeof(alarm->l3_name) - 1);
                            alarm->l3_level = key_value.value();
                        }
                        else 
                        {
                            std::cout << " alarm ignored  Key: " << key_value.key() << ", Value: " << key_value.value() << std::endl;
                        }
                        i++;
                    }

                }
            }
        }
    }
    //std::cout << "Finished processing alarms" << std::endl;
    return (i > 3) ? 3:i;
}


// Function to parse smVar from JSON
void parse_smVar(smVar* smvar, json j ) {
    // Set name
    smvar->name[15] = '\0';
    strncpy(smvar->name, j["name"].get<std::string>().c_str(), sizeof(smvar->name) - 1);
    
    //std::cout << " we got a name [" << smvar->name << "]"<< std::endl;

    // Set source (optional, default "sbmu")
    if (j.find("source") != j.end()) {
        strncpy(smvar->source, j["source"].get<std::string>().c_str(), sizeof(smvar->source) - 1);
    }
    //std::cout << " we got a source " << std::endl;

    // Set reg_type
    if (j.find("reg_type") != j.end()) {
        strncpy(smvar->reg_type, j["reg_type"].get<std::string>().c_str(), sizeof(smvar->reg_type) - 1);
    }
    //std::cout << " we got a reg_type " << std::endl;

    // Set sm_name
    if (j.find("sm_name") != j.end()) {
        strncpy(smvar->sm_name, j["sm_name"].get<std::string>().c_str(), sizeof(smvar->sm_name) - 1);
        //std::cout << " we got a sm_name [" << smvar->sm_name << "]"<< std::endl;

    }
    //std::cout << " we got a sm_name " << std::endl;

    // Set offset (can be a number or string)
    if (j.find("offset") != j.end()) {
        if (j["offset"].is_string()) {
            std::string offset_str = j["offset"];
            smvar->offset = get_str_offset(offset_str);
        } else {
            smvar->offset = j["offset"].get<int>();  // Direct integer assignment
        }
    }
    // TODO check for 0xffffffff offsets  todu put a try catch around the offset eval

    //std::cout << " we got an offset " << smvar.offset << std::endl;
    // Set latched (optional, default is false)
    if (j.find("latched") != j.end()) {
        smvar->latched = j["latched"].get<bool>();
    }


    // Parse max_alarms (up to 3 levels)
    if (j.find("max_alarms") != j.end()) {
        //std::cout << " we got max_alarms " << std::endl;
        smvar->num_max_alarms = parse_smvar_alarm(&smvar->max_alarm, j["max_alarms"]);  // Parse max alarms
    }

    // Parse min_alarms (up to 3 levels)
    if (j.find("min_alarms") != j.end()) {
        //std::cout << " we got min_alarms " << std::endl;
        smvar->num_min_alarms = parse_smvar_alarm(&smvar->min_alarm, j["min_alarms"]);  // Parse min alarms
    }

    // Parse stall_alarms (up to 3 levels)
    if (j.find("stall_alarms") != j.end()) {
        //std::cout << " we got stall_alarms " << std::endl;
        smvar->num_stall_alarms = parse_smvar_alarm(&smvar->stall_alarm, j["stall_alarms"]);  // Parse stall alarms
        // 3;  // Assuming 1 level for now (update as needed)
    }
    smVar_map[smvar->name] = smvar;

}


// This function will parse a single JSON object into an smVar object
bool parse_var_item(smVar* obj, json item) {
    try {
        // Initialize fields with default values
        std::memset(obj, 0, sizeof(smVar));

        // Set name, source, reg_type, and rack_name
        std::strncpy(obj->name,      item["name"].get<std::string>().c_str(),     sizeof(obj->name) - 1);
        std::strncpy(obj->source,    item["source"].get<std::string>().c_str(),   sizeof(obj->source) - 1);
        std::strncpy(obj->reg_type,  item["reg_type"].get<std::string>().c_str(), sizeof(obj->reg_type) - 1);
        std::strncpy(obj->sm_name,   item["sm_name"].get<std::string>().c_str(),  sizeof(obj->sm_name) - 1);

        // Set offset as integer, handle accordingly for hex
        if (item.find("offset") != item.end()) {
            if (item["offset"].is_string()) {
                std::string offset_str = item["offset"];
                obj->offset = get_str_offset(offset_str);
            } else {
                obj->offset = item["offset"].get<int>();  // Direct integer assignment
            }
        }

        //obj->offset = std::stoi(item["offset"].get<std::string>(), nullptr, 16);

        if (item.find("latched") != item.end()) {
            obj->latched = item["latched"].get<bool>();
        }
        // TODO make this a vector of Actions
        // Parse max_alarms if present
        if (item.find("max_alarms") != item.end()) {
            obj->num_max_alarms = 1; // just one for now
            const auto& max_alarms = item["max_alarms"];
            //obj.num_max_alarms = max_alarms.size();
            for (int j = 0; j < obj->num_max_alarms; ++j) {
                const auto& alarm = max_alarms[j];
                obj->max_alarm.l1_level = alarm["level1"];
                obj->max_alarm.l2_level = alarm["level2"];
                obj->max_alarm.l3_level = alarm["level3"];
                obj->max_alarm.hist = alarm["hist"];
            }
        }

        // Parse min_alarms if present
        if (item.find("min_alarms") != item.end()) {
            obj->num_min_alarms = 1; // just one for now
            const auto& min_alarms = item["min_alarms"];
            //obj.num_min_alarms = min_alarms.size();
            for (int j = 0; j < obj->num_min_alarms; ++j) {
                const auto& alarm = min_alarms[j];
                obj->min_alarm.l1_level = alarm["level1"];
                obj->min_alarm.l2_level = alarm["level2"];
                obj->min_alarm.l3_level = alarm["level3"];
                obj->min_alarm.hist = alarm["hist"];
            }
        }
        // TODO make all 3 levels optional 
        if (item.find("stall_alarms") != item.end()) {
            obj->num_stall_alarms = 1; // just one for now
            const auto& stall_alarms = item["stall_alarms"];
            //obj.num_stall_alarms = stall_alarms.size();
            for (int j = 0; j < obj->num_stall_alarms; ++j) {
                const auto& alarm = stall_alarms[j];
                obj->stall_alarm.l1_level = alarm["level1"];
                obj->stall_alarm.l2_level = alarm["level2"];
                obj->stall_alarm.l3_level = alarm["level3"];
                obj->stall_alarm.hist = alarm["hist"];
            }
        }

        // do this like sys_map 
        if (item.contains("obj_map") && item["obj_map"].is_object()) {
            //std::cout << "sys is an object" << std::endl;
            for (auto& sys : item["obj_map"].items()) {
                // For each system configuration, create a smObj and add it to the sys_map
                std::string sys_name = sys.key();
                //std::cout << "   item key " << sys.key() << std::endl;

                smObj smobj;
                //sys_name, "Description of " + sys_name, nullptr, 0, 0, true);  // Default values for smObj
                smobj.init = false;
                //std::cout << "   smObj created  " << std::endl;

                // Check the type of the value and set the appropriate field in smObj
                if (sys.value().is_boolean()) {
                    smobj.is_bool = true;
                    smobj.bool_val = sys.value();  // Store boolean values in the 'flag' field
                }
                else if (sys.value().is_number_integer()) {
                    smobj.is_integer = true;
                    smobj.int_val = sys.value();  // Store boolean values in the 'flag' field
                }
                else if (sys.value().is_number_float()) {
                    smobj.is_float = true;
                    smobj.float_val = sys.value().get<float>();  // Store boolean values in the 'flag' field
                    //smobj.type = static_cast<int>(sys.value().get<float>());  // Store float values in the 'type' field (or as needed)
                }
                else if (sys.value().is_string()) {
                    smobj.string_val = sys.value();  // Store string values in the 'desc' field
                }
                else {
    //                smobj.ptr = &sys.value();  // For other types, store the pointer to the value
                }
                //obj->obj_map[sys_name] = smobj;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing agg item: " << e.what() << std::endl;
        return false;
    }
}

// Function to parse the JSON file and populate the global agg array
void parse_json_file_to_vars(const std::string& filename) {
    // Open and parse the JSON file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Parse JSON array
    nlohmann::json json_data;
    file >> json_data;

    // Iterate over each JSON object and call parse_agg_item
    for (size_t i = 0; i < json_data.size(); ++i) {
        if (num_smvars >= NUM_SMVARS) {
            std::cerr << "Reached maximum number of Agg objects: " << NUM_SMVARS << std::endl;
            break;  // Stop if we've reached the maximum number of Agg objects
        }

        const auto& item = json_data[i];

        // Use parse_agg_item to parse each individual item
        smVar* obj = &smvars[num_smvars++];
        if (parse_var_item(obj, item)) {

            // Optionally, print the created Agg object for verification
            std::cout << "Created var object for: " << obj->name << std::endl;
            std::cout << "Offset: " << std::hex << obj->offset << std::dec << ", Max Alarms: " << obj->num_max_alarms << std::endl;
        }
    }

    file.close();
}

// bool clear_alarm(Agg &agg)
// {
//     agg.max_alarm.bits.state = 0;
//     agg.min_alarm.bits.state = 0;
//     agg.stall_alarm.bits.state = 0;
//     return 0;
// }


bool clear_alarm_var(smVar *var)
{
    var->max_alarm.bits.state = 0;
    var->min_alarm.bits.state = 0;
    var->stall_alarm.bits.state = 0;
    return 0;
}

// watch this for leaks
void add_alarm_to_outputs(std::vector<smObj*>*outputs, smVar* input, const char* desc, const char* level_name )
{
    std::ostringstream full_desc;
    smObj *smobj = new smObj;
    smobj->idx = input->idx;
    full_desc << input->name << "  " << desc << level_name;
    smobj->desc = full_desc.str();

    outputs->emplace_back(smobj);
}

uint16_t handle_max_alarm_var(std::vector<smObj*>*outputs, smVar* var, smVar* rack_var, uint16_t level, uint16_t alarm_state, const char* level_name)
{
    if (rack_var->value < (level - var->max_alarm.hist)) // if value < level - hist, unset the alarm
    {
        if (alarm_state == 1)
        {
            if(!var->latched)
                alarm_state = 0;
        }
    }
    else if (rack_var->value > level) // if value > level, set the alarm
    {
        if (alarm_state == 0)
        {
            alarm_state = 1;
        }
    }
    if(alarm_state == 1)
    {
        add_alarm_to_outputs(outputs, rack_var, "_max_alarm_", level_name);
    }
    return alarm_state;
}

uint16_t handle_min_alarm_var(std::vector<smObj*>*outputs, smVar* var,  smVar* rack_var, uint16_t level, uint16_t alarm_state, const char* level_name)
{
    if (rack_var->value > (level - var->min_alarm.hist)) // if value > level - hist, unset the alarm
    {
        if (alarm_state == 1)
        {
            if(!var->latched)
                alarm_state = 0;
        }
    }
    else if (rack_var->value < level) // if value > level, set the alarm
    {
        if (alarm_state == 0)
        {
            alarm_state = 1;
        }
    }
    if(alarm_state == 1)
    {
        add_alarm_to_outputs(outputs, rack_var,  "_min_alarm_",level_name);
    }

    return alarm_state;
}

uint16_t handle_stall_alarm_var(std::vector<smObj*>*outputs, smVar* var,  smVar* rack_var, uint16_t level, uint16_t alarm_state, const char* level_name)
{
    // detect change
    if (rack_var->value != rack_var->last_value)
    {
        rack_var->last_value = rack_var->value;
        rack_var->last_time = ref_time_dbl();
        if (alarm_state == 1)
        {
            if(!var->latched)
                alarm_state = 0;
        }
        return alarm_state;
    }
    // no change for a whlle (level in mS)
    if((rack_var->time - rack_var->last_time) >(float)level/1000.0)
    {
        if (alarm_state == 0)
        {
            alarm_state = 1;
        }
    }
    if(alarm_state == 1)
    {
        add_alarm_to_outputs(outputs, rack_var, "_stall_", level_name);
    }

    return alarm_state;

}

// the rtos_ rack_ sbmu shared memories are all found by default
// testing is done via the _qt app which has been extended to 
//  define smVars, define and run cmd_ops with arguments
// to make it simple when we run the agg_op and the group_alarm_op we'll use the list of smVars by default
// running simulation mode,  we can set up values for each rack run  the agg and alarm ops and see the results.
// new actions "smvar" set / get /show / modifes a named smvar
// "op"  get/set/run defines a named cmd_op with arguments  

// An smVar is a data point , we keep the raw object data base intact and cannaccess it using modbus or sm methods whicj understsnd rack , rtos an sbmu memory areas and rack_numbers
// the agg_op takes smVars
// agregates are  collected  from all the racks that are online and in service
// we collect max, min and stall alarm values
// the group_alarm_op takes the current value and checks against the alarm states is any
// such a data point may well have alarms that are normally of 3 levels
// these alarms will have trigger levels and hysteresis
// if an alarm is set a bit is set somewhere, we also collect a list of alarms in the output vector 
// designed to be run from the system bms.


void setup_smVar(smVar*smvar)
{
    
    // void* sm_rtos = get_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
    // void* sm_rack = get_rack_mem(rack_num, shm_rack, rack_max, rack_size);
    if(strncmp(smvar->sm_name, "rtos" , strlen("rtos"))==0)
    {
        smvar->sm_base = shm_rtos;
    }
    if(strncmp(smvar->sm_name, "rack" , strlen("rack"))==0)
    {
        smvar->sm_base = shm_rack;
    }
    if(strncmp(smvar->sm_name, "sbmu" , strlen("sbmu"))==0)
    {
        smvar->sm_base = shm_sbmu;
    }
    return;
}

    // Initialize random number generator with a random seed
#include <random>
int get_random()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
        // // Define the range for the random number
    static std::uniform_int_distribution<> dist(0, 1000);

    // // Generate a random number between 3000 and 4000
    int random_number = dist(gen);
    return random_number;
}

// lets get closer to a more capable, flexible system
// we'll define the aggregate as an op , one of a list of ops each op will have a name  and a dict 
// we dont need shm defs since we have abstracted those away
// todo this first time it runs it will "compile" stuff to resolve varaibles
//  (bcm_data->Evbcm_Sample_Data.group_curr + 16000);

int agg_op(std::map<std::string,smObj*>&sys, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    // todo get raxk_max from sys
    // for each rackn
    int my_rack_max=12;
    if( get_opdef_map_int_name(my_rack_max, "globals", "lcl_rack_max") )
    {
        std::cout << " my_rack_max  [" << (int)my_rack_max<< "]" ;
    }


    std::cout << " agg_op running on " << my_rack_max << " racks" << std::endl;
    for (auto item: *inputs)
    {   
        smVar* input = find_smvar(item->name.c_str());
        if(!input)
            continue;
        
        //smVar* smvarf = find_smvar(smvar->name);
        //std::cout << " at start smvars ["<< j <<"]  name [" << smvar->name << "] sm_name [" << smvar->sm_name << "]" << std::endl;  
        //printf(" found smvarf %p\n", smvarf);
        input->count = 0;
        input->total = 0;
    }


    for (int rack_num = 0; rack_num < my_rack_max; rack_num++)
    {
        void* sm_rtos = get_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
        void* sm_rack = get_rack_mem(rack_num, shm_rack, rack_max, rack_size);

        SbmuRack* my_rack = (SbmuRack*) ((uint8_t*)sm_rack + RACK_SM_MEM_SIZE);

        if (my_rack->rack_online)
        {
            std::cout << " rack " << rack_num << " is online" << std::endl;
            use_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
            //for each item
            for (auto item: *inputs)
            {   
                smVar* input = find_smvar(item->name.c_str());
                if(!input)
                {
                    std::cout << "     input [" << item->name << "] not found, skipping it "  << std::endl;
                    continue;
                }
                smVar* output;
                // agg works on just the input variables
                output = input;
                int value = lcl_get_mem_int_val_rack(input->sm_name, rack_num, input->reg_type, input->offset);
                // add the value to the total

                output->total += value;

                if(output->count == 0)
                {
                    output->min = value;
                    output->max = value;
                }
                else
                {
                    if (value < output->min) 
                    {
                        output->min = value;
                        output->min_rack = rack_num;
                    }
                    if (value > output->max)
                    {
                        output->max = value;
                        output->max_rack = rack_num;
                    }
                }
                output->count++;
            }
            release_rack_mem();
        }
        else
        {
            std::cout << " rack " << rack_num << " is offline" << std::endl;
        }
    }
    std::cout << " all racks finshed" << std::endl;
    return 0;
}


int agg_op_def(opDef& opdef,std::vector<smObj*>*inputs, std::vector<smObj*>*outputs )
{
    int ret = agg_op(opdef.sys_map, &opdef.inputs, &opdef.outputs);
    std::cout << "Agg_op finished " << std::endl;
    return ret;
}

// for each rack
// this can be run on the collected aggregate on on each rack with different limits
// this one scans the inputs and makes sure the values do not exceed the max/min/stall alarm limits
// for now the limits are saved in the input vars
// the outputs map is a map of all the valid alarms under an expanded name
int group_alarm_op(std::map<std::string,smObj*>&sys, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{

    // Iterate through the vector and delete each dynamically allocated smObj
    for (auto* obj : *outputs) {
        delete obj;  // Delete the dynamically allocated smObj
    }
    outputs->clear();   

    for (auto item: *inputs)
    {   
        smVar* input = find_smvar(item->name.c_str());
        smVar* rack_input = input;
        if(!input)
            continue;
        input->value = input->total;
        std::cout << " run_alarms value for [" << item->name << "]" 
                    << " input name [" << input->name << "]"
                    << " idx [" << (int)  item->idx << "]"
                    << " value [" << (int)  input->value << "]"
                    << " num_max [" << (int)  input->num_max_alarms << "]"
                    << " num_min [" << (int)  input->num_min_alarms << "]"
                    << " num_stall [" << (int)  input->num_stall_alarms << "]"<<std::endl;

        // Handle alarms
        if(input->num_max_alarms > 0) // just one for now
        {
            // TODO check if there are any alarms defined 
            input->max_alarm.bits.flags.l1_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l1_level, input->max_alarm.bits.flags.l1_state, "Max Level 1");
            input->max_alarm.bits.flags.l2_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l2_level, input->max_alarm.bits.flags.l2_state, "Max Level 2");
            input->max_alarm.bits.flags.l3_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l3_level, input->max_alarm.bits.flags.l3_state, "Max Level 3");
            std::cout << " alarm states for [" << item->name << "] max [" << std::hex<<(int) input->max_alarm.bits.state<< std::dec << "]" << std::endl;
        }
        if(input->num_min_alarms > 0) // just one for now
        {
            input->min_alarm.bits.flags.l1_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l1_level, input->min_alarm.bits.flags.l1_state, "Min Level 1");
            input->min_alarm.bits.flags.l2_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l2_level, input->min_alarm.bits.flags.l2_state, "Min Level 2");
            input->min_alarm.bits.flags.l3_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l3_level, input->min_alarm.bits.flags.l3_state, "Min Level 3");
            std::cout << " alarm states for [" << item->name << "] min [" << std::hex<<(int) input->min_alarm.bits.state<< std::dec << "]" << std::endl;
        }
        if(input->num_stall_alarms > 0) // just one for now
        {
            input->stall_alarm.bits.flags.l1_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l1_level, input->stall_alarm.bits.flags.l1_state, "Min Level 1");
            input->stall_alarm.bits.flags.l2_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l2_level, input->stall_alarm.bits.flags.l2_state, "Min Level 1");
            input->stall_alarm.bits.flags.l3_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l3_level, input->stall_alarm.bits.flags.l3_state, "Min Level 1");
            std::cout << " alarm states for [" << item->name << "] stall [" << std::hex<<(int) input->stall_alarm.bits.state<< std::dec << "]" << std::endl;
        }
    }
    return 0;
}


// the alarm op saves its result in the outputs
int group_alarm_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    int ret = group_alarm_op(opdef.sys_map, &opdef.inputs, &opdef.outputs);

    printf("\n\nresults for [%s] \n\n", opdef.name.c_str());
    //    std::cout << "Results:" << std::endl;
    for (auto ob : opdef.outputs)
    {
        std::cout << ob->desc << std::endl;
    }
    std::cout << "==============" << std::endl;
    opDef* opdef2 = find_opdef("run_group_alarms");
    printf("\nusing  def \nresults for [%s] \n\n", opdef2->name.c_str());
    //    std::cout << "Results:" << std::endl;
    for (auto ob : opdef2->outputs)
    {
        std::cout << ob->desc << std::endl;
    }
    std::cout << "==============" << std::endl;


    return ret;
}

// for each rack
// this can be run on the collected aggregate on on each rack with different limits
// this one scans the inputs and makes sure the values do not exceed the max/min/stall alarm limits
// for now the limits are saved in the input vars
// the outputs map is a map of all the valid alarms under an expanded name
// the inputs in this case are all the rack limit variables we have to create one  
// but we have to get the value from eack rack using rack_num
int rack_alarm_op(std::map<std::string,smObj*>&sys, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{

    outputs->clear();
    for (int rack_num = 0 ; rack_num < lcl_rack_max; rack_num++)
    {
        void* sm_rtos = get_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
        void* sm_rack = get_rack_mem(rack_num, shm_rack, rack_max, rack_size);

        SbmuRack* my_rack = (SbmuRack*) ((uint8_t*)sm_rack + RACK_SM_MEM_SIZE);
        if (!my_rack->rack_online)
        {
            std::cout << " rack " << rack_num << " is offline" << std::endl;
            continue;
        }

        std::cout << " rack " << rack_num << " is online" << std::endl;
        use_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);

        for (auto item: *inputs)
        {   
            smVar* input = find_smvar(item->name.c_str());
            if(!input)
            {
                std::cout << "     input [" << item->name << "] not found, skipping it "  << std::endl;
                continue;
            }
            smVar* output;
            //find the individual rack var and get the value using rack_num
            smVar* rack_input = find_rack_smvar(item->name.c_str(), rack_num);
            if(!rack_input)
            {
                std::cout << "     rack_input [" << item->name << "] not found, skipping it "  << std::endl;
                continue;
            }

            int value = lcl_get_mem_int_val_rack(input->sm_name, rack_num, input->reg_type, input->offset);
            rack_input->value = value;//find_smvar(item.name.c_str());

            std::cout << " run_rack_alarms value for [" << item->name << "]" 
                    << " input name [" << input->name << "]"
                    << " idx [" << (int)  item->idx << "]"
                    << " value [" << (int)  rack_input->value << "]"
                    << " num_max [" << (int)  input->num_max_alarms << "]"
                    << " num_min [" << (int)  input->num_min_alarms << "]"
                    << " num_stall [" << (int)  input->num_stall_alarms << "]"
                    <<std::endl;

            // Handle alarms
            if(input->num_max_alarms > 0) // just one for now
            {
                // TODO check if there are any alarms defined 
                input->max_alarm.bits.flags.l1_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l1_level, input->max_alarm.bits.flags.l1_state, "Max Level 1");
                input->max_alarm.bits.flags.l2_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l2_level, input->max_alarm.bits.flags.l2_state, "Max Level 2");
                input->max_alarm.bits.flags.l3_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l3_level, input->max_alarm.bits.flags.l3_state, "Max Level 3");
                std::cout << " alarm states for [" << item->name << "] max [" << std::hex<<(int)  rack_input->max_alarm.bits.state<< std::dec << "]" << std::endl;
            }
            if(input->num_min_alarms > 0) // just one for now
            {
                input->min_alarm.bits.flags.l1_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l1_level, input->min_alarm.bits.flags.l1_state, "Min Level 1");
                input->min_alarm.bits.flags.l2_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l2_level, input->min_alarm.bits.flags.l2_state, "Min Level 2");
                input->min_alarm.bits.flags.l3_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l3_level, input->min_alarm.bits.flags.l3_state, "Min Level 3");
                std::cout << " alarm states for [" << item->name << "] min [" << std::hex<<(int)  rack_input->min_alarm.bits.state<< std::dec << "]" << std::endl;
            }
            if(input->num_stall_alarms > 0) // just one for now
            {
                input->stall_alarm.bits.flags.l1_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l1_level, input->stall_alarm.bits.flags.l1_state, "Min Level 1");
                input->stall_alarm.bits.flags.l2_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l2_level, input->stall_alarm.bits.flags.l2_state, "Min Level 1");
                input->stall_alarm.bits.flags.l3_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l3_level, input->stall_alarm.bits.flags.l3_state, "Min Level 1");
                std::cout << " alarm states for [" << item->name << "] stall [" << std::hex<<(int)  rack_input->stall_alarm.bits.state<< std::dec << "]" << std::endl;
            }
        }
    }
    return 0;
}

// the alarm op saves its result in the outputs
int rack_alarm_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    int ret = rack_alarm_op(opdef.sys_map, &opdef.inputs, &opdef.outputs);
    printf("\n\nresults for [%s] \n\n", opdef.name.c_str());
    //    std::cout << "Results:" << std::endl;

    for (auto ob : opdef.outputs)
    {
        std::cout << ob->desc << std::endl;
    }
    std::cout << "==============" << std::endl;
    return ret;
}

// transfer outputs to inputs or to other outputs
int transfer_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    std::cout << "run  [" << opdef.name << "]" << std::endl;
    std::cout << "   opdefs outputs size [" << opdef.outputs.size() << "]" << std::endl;
    std::cout << "   opdefs inputs size [" << opdef.inputs.size() << "]" << std::endl;

    // switch inputs and outputs 
    // for now just the first output and transfer it to all the inputs, or transfer it to the second output
    if (opdef.outputs.size() == 2 ) 
    {
        opDef* src = find_opdef(opdef.outputs[0]->name.c_str());
        opDef* dest = find_opdef(opdef.outputs[1]->name.c_str());

        std::cout << "src [" << opdef.outputs[0]->name << "]" << std::endl;
        std::cout << "dest [" << opdef.outputs[1]->name << "]" << std::endl;

        dest->outputs = src->outputs;
        src->outputs.clear();
    }


    // opdef.inputs contain a list of opps to run 
    return 0;//alarm_op(opdef.sys_map, opdef.inputs, opdef.outputs);
}

// simple sequence
// todo add conditional 
int sequence_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    std::cout << "start running sequence [" << opdef.name << "]" << std::endl;

    opDef* last_def = nullptr;

    for (auto ob : opdef.inputs)
    {
        std::cout << std::endl;
        std::cout << " running op [" << ob->name << "]" << std::endl;
        last_def = run_opdef_name(ob->name.c_str(), inputs, outputs); 
    }
    // 
    // // may be this is the last pseudo op in the sequence
    // opdef.outputs = last_def->outputs;
    // last_def->outputs.clear();

    std::cout << "end running sequence [" << opdef.name << "]" << std::endl;


    // opdef.inputs contain a list of opps to run 
    return 0;//alarm_op(opdef.sys_map, opdef.inputs, opdef.outputs);
}

int timer_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    // set up timer and run the selected system until stopped 
    // the input name is the value  the item map will have period , offset and frequency 
    return 0;
}    

int case_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    // TODO pick an input that matches the opdef value 
    return 0;
}

int while_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
{
    //run while the opdef value matches something
    return 0;
}

void show_smvar(smVar* smvar)
{
    // Output the parsed values (for testing)
    std::cout << "Name: " << smvar->name;
    // std::cout << "Source: " << smvar->source << std::endl;
    // std::cout << "Reg Type: " << smvar->reg_type << std::endl;
    // std::cout << "SM Name: " << smvar->sm_name << std::endl;
    std::cout << " Offset: " << smvar->offset << std::endl;
    //std::cout << "Latched: " << smvar->latched << std::endl;
    if(smvar->num_max_alarms > 0)
    {
        std::cout << "Max Alarms " << smvar->max_alarm.l1_name << ":" << smvar->max_alarm.l1_level;
        if(smvar->num_max_alarms > 1)
            std::cout << ",  " << smvar->max_alarm.l2_name << ":" << smvar->max_alarm.l2_level;
        if(smvar->num_max_alarms > 2)
            std::cout << ",  " << smvar->max_alarm.l3_name << ":" << smvar->max_alarm.l3_level;
        std::cout << std::endl;
    }
    if(smvar->num_min_alarms > 0)
    {
        std::cout << "Min Alarms " << smvar->min_alarm.l1_name << ":" << smvar->min_alarm.l1_level;
        if(smvar->num_min_alarms > 1)
            std::cout << ",  " << smvar->min_alarm.l2_name << ":" << smvar->min_alarm.l2_level;
        if(smvar->num_min_alarms > 2)
            std::cout << ",  " << smvar->min_alarm.l3_name << ":" << smvar->min_alarm.l3_level;
        std::cout << std::endl;
    }
    if(smvar->num_stall_alarms > 0)
    {
        std::cout << "Stall Alarms " << smvar->stall_alarm.l1_name << ":" << smvar->stall_alarm.l1_level;
        if(smvar->num_stall_alarms > 1)
            std::cout << ",  " << smvar->stall_alarm.l2_name << ":" << smvar->stall_alarm.l2_level;
        if(smvar->num_stall_alarms > 2)
            std::cout << ",  " << smvar->stall_alarm.l3_name << ":" << smvar->stall_alarm.l3_level;
        std::cout << std::endl;
    }

}

// Function to show the contents of opDef
void show_opDef(const opDef& operation) {
    std::cout << "opDef Name: " << operation.name 
                << " Func: " << operation.func_name << std::endl;

    // Show sys_map
    std::cout << "\nSystem Configuration (sys_map):" << std::endl;
    for (const auto& sys_item : operation.sys_map) {
        std::cout << "  Name: " << sys_item.second->name 
                //<< ", Index: " << sys_item.second.idx 
                << std::endl;
    }

    // Show inputs
    std::cout << "\nInputs:" << std::endl;
    for (const auto& input : operation.inputs) {
        std::cout << "  Name: " << input->name 
                    //<< ", Index: " << input.idx 
                    << std::endl;
    }

    // Show outputs
    std::cout << "\nOutputs:" << std::endl;
    for (const auto& output : operation.outputs) {
        std::cout << "  Name: " << output->name 
                    //<< ", Index: " << output.idx 
                    << std::endl;
    }
}



int test_smVar() {
    // Example JSON input for parsing an operation
    std::string json_input = R"({
        "name": "group_curr","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x2","latched": false,
        "max_alarms": [{"level1": 3000, "level2": 3200, "level3": 3400, "hist": 50}],
        "min_alarms": [{"level1": -3000, "level2": -3200, "level3": -3400, "hist": 50}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    parse_smvar_str(json_input);


    // Create smVar object
    smVar* smvar; //("", ""); // Create with empty name and source
    //std::cout << " parsed [" << smvar->name << "] sm_name [" << smvar->sm_name << "]" << std::endl;  
    json_input = R"({
        "name": "group_volt","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x1","latched": false,
        "max_alarms": [{"level1": 13000, "level2": 13200, "level3": 13400, "hist": 100}],
        "min_alarms": [{"level1": 12600, "level2": 12500, "level3": 12400, "hist": 100}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    parse_smvar_str(json_input);

    json_input = R"({
        "name": "rack_curr","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x2","latched": false,
        "max_alarms": [{"level1": 3000, "level2": 3200, "level3": 3400, "hist": 50}],
        "min_alarms": [{"level1": -3000, "level2": -3200, "level3": -3400, "hist": 50}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    parse_smvar_str(json_input);

    json_input = R"({
        "name": "rack_volt","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x1","latched": false,
        "max_alarms": [{"level1": 13000, "level2": 13200, "level3": 13400, "hist": 100}],
        "min_alarms": [{"level1": 12600, "level2": 12500, "level3": 12400, "hist": 100}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    parse_smvar_str(json_input);

    json_input = R"({
        "name": "rack_curr","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x1","latched": false,
        "max_alarms": [{"level1": 13000, "level2": 13200, "level3": 13400, "hist": 100}],
        "min_alarms": [{"level1": 12600, "level2": 12500, "level3": 12400, "hist": 100}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    parse_smvar_str(json_input);

    

  
    // add this to the smvar map


// now we have to register a couple of functions
    register_func("agg_op",         agg_op_def);  // run the aggregate function on all "online" racks
    register_func("group_alarm_op", group_alarm_op_def); // run the alarm detection  function on the aggregate results 
    register_func("sequence_op",    sequence_op_def); // run a sequence of operations
    register_func("transfer_op",    transfer_op_def); // run a sequence of operations
    register_func("timer_op",       timer_op_def);    // run the sequence every n seconds

    std::cout << std::endl;


    // now we have to create an smOp to provide a context to run an agg_op 
    // for example to use parse_opdef

    //in this example we specify the names of the objects we want to aggregate 
    // in the fast version of this the name will be interpreted and an index in the smVar list

    // sys is defined but not used at the moment
    // we may want to add the rack online check into the sys area
    // the result, again is not uese here yet
    json_input = R"({
     "name": "globals", 
     "sys": {"enabled": true, "num_vars": 2, "lcl_rack_max":12 }
    })";
    parse_opdef_str(json_input);

    json_input = R"({
     "name": "run_agg", "func": "agg_op", 
     "inputs": [{"name": "group_curr"}, {"name": "group_volt"}],
     "outputs": [{"name": "result"}]
    })";
    parse_opdef_str(json_input);
     // possibly we create a named data block called results to get to this data 
    json_input = R"({"name": "run_group_alarms", "func": "group_alarm_op",
         "inputs": [{"name": "group_curr"}, {"name": "group_volt"}]})";
    parse_opdef_str(json_input);

     // final block to transfer run_group_alarms outputs to run_group_alarms  
    json_input = R"({"name": "transfer_group_alarms", "func": "transfer_op",
         "outputs": [{"name": "run_group_alarms"}, {"name": "run_alarm_seq"}]
         })";
    parse_opdef_str(json_input);

    // we may need global and individual enables on the sequence inputs
    json_input = R"({"name": "run_alarm_seq", "func": "sequence_op", 
        "inputs": [{"name": "run_agg"}, {"name": "run_group_alarms"}, {"name": "transfer_group_alarms"}]})";
    parse_opdef_str(json_input);

    // we may need global and individual enables on the sequence inputs
    json_input = R"({"name": "collect_rack_status", "func": "rack_agg_op", 
        "inputs": [{"name": "MinSOC", "rack_idx":34,"sbms_idx":201}, {"name": "run_group_alarms"}, {"name": "transfer_group_alarms"}]})";
    parse_opdef_str(json_input);

    lcl_rack_max = rack_max;
    if( get_opdef_map_int_name(lcl_rack_max, "globals", "lcl_rack_max") )
    {
        std::cout << " lcl_rack_max  [" << (int)lcl_rack_max<< "]" ;
    }

    for (int i = 0 ; i< lcl_rack_max; i++)
    {
        char json_char[256];
        snprintf(json_char, sizeof(json_char), "{\"name\":\"rack_volt_%02d\"}", i);
        json_input = json_char;
        parse_smvar_str(json_input);
        snprintf(json_char, sizeof(json_char), "{\"name\":\"rack_curr_%02d\"}", i);
        json_input = json_char;
        parse_smvar_str(json_input);
    }

    std::cout << "test smvars" << std::endl;
    std::cout << "==============================" << std::endl;
    for (int i = 0 ; i < num_smvars; i++)
    {
        if(i > 0)
            std::cout << "=============" << std::endl;
        show_smvar(&smvars[i]);
    }
    std::cout << "==============================" << std::endl;
    std::cout << std::endl;

    std::cout << "test opDefs: " << std::endl;
    std::cout << "==============================" << std::endl;
    for (int i = 0; i < num_opdefs; i++)
    {
        if(i > 0)
            std::cout << "======" << std::endl;

        show_opDef(opdefs[i]); 
    }
    std::cout << "==============================" << std::endl;


    //now we can run the opDef  
    // no specified inputs or outputs 
    run_opdef_name("run_alarm_seq", nullptr, nullptr); 

    //opDef* opdef = find_opdef("run_group_alarms");



    //run_opdef_name("run_agg"); 
    //run_opdef_name("run_alarms"); 

    for (int j = 0 ; j < num_smvars; j++)
    {
        smvar = &smvars[j];

        std::cout << " var idx ["<< j <<"]  name [" << smvar->name << "]";  
        if (smvar->sm_name[0] != 0)
            std::cout << " sm_name [" << smvar->sm_name << "]" ;
        std::cout << std::endl;  
    }
    int val;
    if( get_opdef_map_int_name(val, "globals", "lcl_rack_max") )
    {
        std::cout << " lcl_rack_max  [" << (int)val<< "]" ;
    }
    else
    {
        std::cout << " lcl_rack_max missing " ;
    }
    std::cout << std::endl;  

    opDef* opdef = find_opdef("run_group_alarms");
    for ( auto obj: opdef->outputs)
    {
        std::cout << " opdef string [" << obj->desc <<"]"<<std::endl;
    }
    return 0;
}
