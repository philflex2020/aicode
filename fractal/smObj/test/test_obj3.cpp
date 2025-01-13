#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <unordered_map>

#include <nlohmann/json.hpp> // Include the JSON library

// cd test
//g++ -g -o smobj -I ../inc test_obj3.cpp 

using json = nlohmann::json;
// struct smObj;

// struct smExt {
//     // Parent and child references
//     std::shared_ptr<smObj> parent;
//     std::shared_ptr<smObj> child;


//     // Function registry with the updated signature
//     std::unordered_map<std::string, std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>> func_map;

//     // Register a named function
//     void register_func(const std::string& name, const std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>& func) {
//         func_map[name] = func;
//     }

//     // Call a registered function
//     int call_func(const std::string& name, std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs) {
//         if (func_map.find(name) != func_map.end()) {
//             return func_map[name](opdef, inputs, outputs);
//         } else {
//             throw std::runtime_error("Function '" + name + "' not found in smExt.");
//         }
//     }
// };

// struct smObj {
//     std::string name;
//     std::map<std::string, std::shared_ptr<smObj>> obj_map; // Child objects
//     std::vector<std::shared_ptr<smObj>> children;     // Array of child objects
//     std::string value;                                      // Stores the value if not an object or array
//     std::shared_ptr<smExt> ext; // Optional extension
//     //std::shared_ptr<smObj> child;
//     //std::shared_ptr<smObj> parent;


//     smObj(const std::string& name, const std::string& value = "")
//         : name(name), value(value), ext(nullptr) {}


//     // Create or access the extension object
//     std::shared_ptr<smExt> get_ext() {
//         if (!ext) {
//             ext = std::make_shared<smExt>();
//         }
//         return ext;
//     }

//     // Convert this object to JSON
//     json to_json() const {
//         json j;

//         if (!value.empty()) {
//             // Check if the value represents a primitive type (number, boolean, or string)
//             try {
//                 if (value == "true" || value == "false") {
//                     return value == "true";
//                 } else if (value.find('.') != std::string::npos) {
//                     return std::stod(value); // Float or double
//                 } else {
//                     return std::stoi(value); // Integer
//                 }
//             } catch (...) {
//                 // If parsing fails, treat it as a string
//                 if (value.front() == '"' && value.back() == '"') {
//                     return value.substr(1, value.size() - 2); // Remove quotes
//                 }
//                 return value; // Return as-is if not quoted
//             }
//         }

//         // Add obj_map ects
//         for (const auto& [key, child] : obj_map) {
//             j[key] = child->to_json();
//         }

//         // Add children
//         if (!children.empty()) {
//             json array_json = json::array();
//             for (const auto& child : children) {
//                 array_json.push_back(child->to_json());
//             }
//             j = array_json;
//         }

//         return j;
//     }
// };


#include "smObj.h"

int sequence_op(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs) {
    if (!opdef) {
        std::cerr << "Error: opdef is null." << std::endl;
        return -1;
    }

    std::cout << "Start running sequence [" << opdef->name << "]" << std::endl;

    std::shared_ptr<smObj> last_op = nullptr;

    for (const auto& input_op : opdef->children) { // Assuming children holds input operations
        std::cout << "Running op [" << input_op->name << "]" << std::endl;

        // Call another operation (mocked as an example)
        last_op = std::make_shared<smObj>("last_op_result"); // Replace with actual call logic
    }

    if (last_op) {
        // Propagate last operation's outputs to the sequence outputs
        outputs->obj_map = last_op->obj_map;
    }

    std::cout << "End running sequence [" << opdef->name << "]" << std::endl;
    return 0;
}



void register_sequence_op(std::shared_ptr<smExt> ext) {
    ext->register_func("sequence_op", sequence_op);
}

void test_function()
{
    auto opdef = std::make_shared<smObj>("sequence_op");
    auto inputs = std::make_shared<smObj>("inputs");
    auto outputs = std::make_shared<smObj>("outputs");

    auto ext = std::make_shared<smExt>();
    register_sequence_op(ext);

    try {
        // TODO opDef->ext->call_func(root, opdef, inputs, outputs);

        int result = ext->call_func("sequence_op", opdef, inputs, outputs);
        std::cout << "Sequence operation result: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Helper function to split paths
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;

    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    return parts;
}

void set_obj_value(std::shared_ptr<smObj>& obj, const std::string& path, const std::string& value) {
    auto parts = split_path(path);
    auto current = obj;

    for (size_t i = 0; i < parts.size(); ++i) {
        std::string part = parts[i];

        // Handle array indices like "inputs[2]"
        size_t open_bracket = part.find('[');
        if (open_bracket != std::string::npos) {
            std::string array_name = part.substr(0, open_bracket);
            size_t index = std::stoul(part.substr(open_bracket + 1, part.find(']') - open_bracket - 1));

            // Ensure the array object exists
            if (current->obj_map.find(array_name) == current->obj_map.end()) {
                current->obj_map[array_name] = std::make_shared<smObj>(array_name);
            }

            current = current->obj_map[array_name];

            // Ensure the array has enough elements
            while (current->children.size() <= index) {
                current->children.push_back(std::make_shared<smObj>("array_item"));
            }

            current = current->children[index];
        } else {
            // Handle regular child objects
            if (current->obj_map.find(part) == current->obj_map.end()) {
                current->obj_map[part] = std::make_shared<smObj>(part);
            }

            current = current->obj_map[part];
        }

        // Set the value at the last part
        if (i == parts.size() - 1) {
            current->value = value;
        }
    }
}

// Get a value from the smObj hierarchy
std::shared_ptr<smObj> get_obj_value(const std::shared_ptr<smObj>& obj, const std::string& path);

//std::shared_ptr<smObj> get_obj_value(const std::shared_ptr<smObj>& obj, const std::string& path);

// Get the size of an array at the specified path
size_t get_array_size(const std::shared_ptr<smObj>& obj, const std::string& path) {
    auto target = get_obj_value(obj, path);
    if (!target) {
        std::cerr << "Error: Path does not exist.\n";
        return 0;
    }

    return target->children.size();
}

// Append a new smObj to an array at the specified path
void append_to_array(std::shared_ptr<smObj>& obj, const std::string& path, const std::shared_ptr<smObj>& new_obj) {
    auto parts = split_path(path);
    auto current = obj;

    for (const auto& part : parts) {
        size_t open_bracket = part.find('[');
        if (open_bracket != std::string::npos) {
            std::cerr << "Error: Path to append must not include an array index.\n";
            return;
        }

        if (current->obj_map.find(part) == current->obj_map.end()) {
            current->obj_map[part] = std::make_shared<smObj>(part);
        }
        current = current->obj_map[part];
    }

    current->children.push_back(new_obj);
}

// Get a value from the smObj hierarchy
std::shared_ptr<smObj> get_obj_value(const std::shared_ptr<smObj>& obj, const std::string& path) {
    auto parts = split_path(path);
    auto current = obj;

    for (const auto& part : parts) {
        // Handle array indices like "inputs[2]"
        size_t open_bracket = part.find('[');
        if (open_bracket != std::string::npos) {
            std::string array_name = part.substr(0, open_bracket);
            size_t index = std::stoul(part.substr(open_bracket + 1, part.find(']') - open_bracket - 1));

            if (current->obj_map.find(array_name) == current->obj_map.end()) {
                return nullptr; // Path does not exist
            }

            current = current->obj_map.at(array_name);

            if (index >= current->children.size()) {
                return nullptr; // Index out of bounds
            }

            current = current->children[index];
        } else {
            // Handle regular child objects
            if (current->obj_map.find(part) == current->obj_map.end()) {
                return nullptr; // Path does not exist
            }

            current = current->obj_map.at(part);
        }
    }

    return current;
}

// how to decode json
void dump_item_type(const json& item)
{
    // Determine the type of the item
    std::string item_type;
    if (item.is_object()) {
        item_type = "object";
    } else if (item.is_array()) {
        item_type = "array";
    } else if (item.is_string()) {
        item_type = "string";
        //item.dump();
    } else if (item.is_boolean()) {
        item_type = "boolean";
    } else if (item.is_number_integer()) {
        item_type = "integer";
    } else if (item.is_number_unsigned()) {
        item_type = "unsigned integer";
    } else if (item.is_number_float()) {
        item_type = "float";
    } else if (item.is_null()) {
        item_type = "null";
    } else {
        item_type = "unknown";
    }
    // Print the item type for debugging
    std::cout << "Array item  is of type: " << item_type << std::endl;
    if (item.is_string()) {
        std::cout << " with value: [" << item.get<std::string>()<<"]"<<std::endl;
    }
}

// Parse the entire JSON object recursively into smObj
// normally calues are dumped but we need some special cases here
std::shared_ptr<smObj>parse_object(const json& j, const std::string& name = "root") {
    auto obj = std::make_shared<smObj>(name);
    if (j.is_string()) {
        std::string objstr = j.get<std::string>();
        //std::cout << " with value: [" << objstr<<"]"<<std::endl;
        obj = std::make_shared<smObj>(objstr,objstr);
    }
    else if (j.is_object()) {
        for (const auto& field : j.items()) {
            const std::string& key = field.key();
            const auto& value = field.value();

            if (value.is_object() || value.is_array()) {
                obj->obj_map[key] = parse_object(value, key);
            } else {
                obj->obj_map[key] = std::make_shared<smObj>(key, value.dump());
            }
        }
    } else if (j.is_array()) {
        for (const auto& item : j) {
            obj->children.push_back(parse_object(item, ""));
        }
    } else {
        obj->value = j.dump();
    }

    return obj;
}

// deep copy , values only
std::shared_ptr<smObj> deep_copy(const std::shared_ptr<smObj>& source) {
    if (!source) {
        return nullptr;
    }

    // Create a new smObj with the same name and value
    auto copy = std::make_shared<smObj>(source->name, source->value);

    // Recursively copy obj_map
    for (const auto& [key, child] : source->obj_map) {
        copy->obj_map[key] = deep_copy(child);
    }

    // Recursively copy array children
    for (const auto& array_child : source->children) {
        copy->children.push_back(deep_copy(array_child));
    }

    return copy;
}

// Print function to recreate JSON from smObj
void print_as_json(const std::shared_ptr<smObj>& obj) {
    json j = obj->to_json();
    std::cout << j.dump(4) << std::endl; // Pretty print with 4 spaces indentation
}


//reference code
void test_child()
{
    auto root = std::make_shared<smObj>("root");
    auto child = std::make_shared<smObj>("child");

    root->get_ext()->child = child;
    child->get_ext()->parent = root;
}


void add_map_item(std::shared_ptr<smObj> root, const std::string& group, const std::string& name, const std::string& json_string)
{
    auto j = json::parse(json_string);
    auto smVar = parse_object(j);
    if(root->obj_map.find(group) == root->obj_map.end())
        root->obj_map[group] = std::make_shared<smObj>(group, name);
    smVar->name = name;
    root->obj_map[group]->obj_map[name] = smVar;//std::make_shared<smObj>("smVars");
}

std::shared_ptr<smObj>find_map_item(std::shared_ptr<smObj> root, const std::string& group, const std::string& name="")
{
    if(name.empty())
    {
        if(root->obj_map.find(group) != root->obj_map.end())
        {
            return root->obj_map[group]; 
        }
    }
    else
    {
        if(root->obj_map.find(group) != root->obj_map.end())
            if (root->obj_map[group]->obj_map.find(name) !=  root->obj_map[group]->obj_map.end())
                return root->obj_map[group]->obj_map[name];
    }
    return nullptr;
}


// // lets get closer to a more capable, flexible system
// // we'll define the aggregate as an op , one of a list of ops each op will have a name  and a dict 
// // we dont need shm defs since we have abstracted those away
// // todo this first time it runs it will "compile" stuff to resolve varaibles
// //  (bcm_data->Evbcm_Sample_Data.group_curr + 16000);


// // Derived class
// class smVar : public smObj {
// public:
//     int extra_int;
//     // Additional members...
// };
// I want to put smVars into a vector of smObjs  and then recast them to smVars
int inputs_main() {
    // Create a vector of shared_ptr to smObj
    std::vector<std::shared_ptr<smObj>> inputs;

    std::vector<std::shared_ptr<smObj>> new_inputs;

    // Populate the vector with instances of mysmObj
    inputs.push_back(std::make_shared<smObj>("var1"));
    inputs.push_back(std::make_shared<smObj>("var2"));

    // Iterate over the vector and downcast to mysmObj
    for (auto& item : inputs) {

        new_inputs.push_back(std::make_shared<smVar>(item->name));
    }

    //inputs.push_back(std::make_shared<smVar>());

    // Iterate over the vector and downcast to mysmObj
    for (auto& item : new_inputs) {
        // Attempt to downcast to shared_ptr<mysmObj>
        std::shared_ptr<smVar> smVarPtr = std::dynamic_pointer_cast<smVar>(item);
        if (smVarPtr) {
            // Downcast successful, access additional members
            smVarPtr->extra_int = 1234;
            std::cout << "extra_int set to " << smVarPtr->extra_int << std::endl;
        } else {
            // Downcast failed, handle accordingly
            std::cout << "Downcast failed." << std::endl;
        }
    }

    return 0;
}

//
// this demonstrates getting a list of inputs
int agg_op(std::shared_ptr<smObj>root, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
{
    int my_rack_max = 12;
    // we have a definition of an input value TODO
    // if( get_opdef_map_int_name(my_rack_max, "globals", "lcl_rack_max") )
    // {
    //     std::cout << " my_rack_max  [" << (int)my_rack_max<< "]" ;
    // }

    // we need a couple of maps for cpounts and totals
    std::map<std::shared_ptr<smObj>, int> count_map;
    std::map<std::shared_ptr<smObj>, int> total_map;


    std::cout << " agg_op running on " << my_rack_max << " racks" << std::endl;
    if(!inputs)
    {
        std::cout << " agg_op no inputs found " << std::endl;
        return 0;
    }
    show_array_names(inputs);

    for (auto item: inputs->children)
    {
        // the smVar has all the info on how to aggregate the variable 
        // ie rack src ref aff dest ref etc   
        auto smVar = find_map_item(root ,"smVars", item->name);
        if(smVar)
        {
            std::cout << "    found smVar [" << item->name<< "]" << std::endl;
            count_map[smVar] = 0;
            total_map[smVar] = 0;
        }
    }


    for (int rack_num = 0; rack_num < my_rack_max; rack_num++)
    {
        //void* sm_rtos = get_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
        //void* sm_rack = get_rack_mem(rack_num, shm_rack, rack_max, rack_size);

        //SbmuRack* my_rack = (SbmuRack*) ((uint8_t*)sm_rack + RACK_SM_MEM_SIZE);

        // if (my_rack->rack_online)
        // {
            std::cout << " rack " << rack_num << " is online" << std::endl;
        //     use_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
             //for each input item
            for (auto item: inputs->children)
            {
                auto smVar = find_map_item(root ,"smVars", item->name);
                count_map[smVar]+=1;
                int value = 1234;
                total_map[smVar]+=value;
            }
    }

    // we should put the total and the count into the smVar
    for (auto [key, item]: count_map)
    {
        std::cout << " Output name ["<< key->name <<" count :" << count_map[key] << " total :"<< total_map[key]<<std::endl; 
    } 
    

    //     //         smVar* output;
    //     //         // agg works on just the input variables
    //     //         output = input;
    //     //         int value = lcl_get_mem_int_val_rack(input->sm_name, rack_num, input->reg_type, input->offset);
    //     //         // add the value to the total

    //     //         output->total += value;

    //     //         if(output->count == 0)
    //     //         {
    //     //             output->min = value;
    //     //             output->max = value;
    //     //         }
    //     //         else
    //     //         {
    //     //             if (value < output->min) 
    //     //             {
    //     //                 output->min = value;
    //     //                 output->min_rack = rack_num;
    //     //             }
    //     //             if (value > output->max)
    //     //             {
    //     //                 output->max = value;
    //     //                 output->max_rack = rack_num;
    //     //             }
    //     //         }
    //     //         output->count++;
    //     //     }
    //     //     release_rack_mem();
    //     // }
    //     // else
    //     // {
    //     //     std::cout << " rack " << rack_num << " is offline" << std::endl;
    //     }
    // }
    std::cout << " all racks finshed" << std::endl;
    return 0;
}


//int sequence_op_def(std::shared_ptr<smObj> root, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs) 
// //{
//      "name": "run_agg", "func": "agg_op", 
//      "inputs": [{"name": "group_curr"}, {"name": "group_volt"}],
//      "outputs": [{"name": "result"}]
//     })";


// not used 
int run_named_op_def(std::shared_ptr<smObj>root, const std::string& name, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
{
    //add_map_item(root ,"opdefs", "run_agg", json_string);
    auto opdef = find_map_item(root, "opdefs", "run_agg");

    // "name": "run_agg", "func": "agg_op", 
    //  "inputs": [{"name": "group_curr"}, {"name": "group_volt"}],
    //  "outputs": [{"name": "result"}]
    if(!opdef)
    {    
        std::cout << "Error no opdef  " << name << " found in opdefs"<< std::endl;
        return 0;
    }
    std::cout << ">>> opdef " << name << " found in opdefs"<< std::endl;
    std::cout << ">>> opdef name " << opdef->name << std::endl;
    std::cout << "opdef obj_map" << std::endl;
    for (auto [key, item] : opdef->obj_map)
    {
        std::cout << " key :["<< key <<"]"<< std::endl;
    }
    std::cout << "opdef children" << std::endl;

    for (auto key : opdef->children)
    {
        std::cout << " key :["<< key->name <<"]"<< std::endl;
    }

    std::shared_ptr<smObj> odinputs;
    if(opdef->obj_map.find("inputs") != opdef->obj_map.end())
    {
        odinputs = opdef->obj_map["inputs"];
    }  
    else 
    {
        odinputs = inputs;
    }
    std::shared_ptr<smObj> odoutputs;
    if(opdef->obj_map.find("outputs") != opdef->obj_map.end())
    {
        odoutputs = opdef->obj_map["outputs"];
    }  
    else 
    {
        odoutputs = outputs;
    }

    int ret = agg_op(root, odinputs, odoutputs);

    std::cout << "Agg_op finished " << std::endl;
    return ret;
}

// // for each rack
// // this can be run on the collected aggregate on on each rack with different limits
// // this one scans the inputs and makes sure the values do not exceed the max/min/stall alarm limits
// // for now the limits are saved in the input vars
// // the outputs map is a map of all the valid alarms under an expanded name
// int group_alarm_op(std::map<std::string,smObj*>&sys, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
// {

//     // Iterate through the vector and delete each dynamically allocated smObj
//     for (auto* obj : *outputs) {
//         delete obj;  // Delete the dynamically allocated smObj
//     }
//     outputs->clear();   

//     for (auto item: *inputs)
//     {   
//         smVar* input = find_smvar(item->name.c_str());
//         smVar* rack_input = input;
//         if(!input)
//             continue;
//         input->value = input->total;
//         std::cout << " run_alarms value for [" << item->name << "]" 
//                     << " input name [" << input->name << "]"
//                     << " idx [" << (int)  item->idx << "]"
//                     << " value [" << (int)  input->value << "]"
//                     << " num_max [" << (int)  input->num_max_alarms << "]"
//                     << " num_min [" << (int)  input->num_min_alarms << "]"
//                     << " num_stall [" << (int)  input->num_stall_alarms << "]"<<std::endl;

//         // Handle alarms
//         if(input->num_max_alarms > 0) // just one for now
//         {
//             // TODO check if there are any alarms defined 
//             input->max_alarm.bits.flags.l1_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l1_level, input->max_alarm.bits.flags.l1_state, "Max Level 1");
//             input->max_alarm.bits.flags.l2_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l2_level, input->max_alarm.bits.flags.l2_state, "Max Level 2");
//             input->max_alarm.bits.flags.l3_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l3_level, input->max_alarm.bits.flags.l3_state, "Max Level 3");
//             std::cout << " alarm states for [" << item->name << "] max [" << std::hex<<(int) input->max_alarm.bits.state<< std::dec << "]" << std::endl;
//         }
//         if(input->num_min_alarms > 0) // just one for now
//         {
//             input->min_alarm.bits.flags.l1_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l1_level, input->min_alarm.bits.flags.l1_state, "Min Level 1");
//             input->min_alarm.bits.flags.l2_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l2_level, input->min_alarm.bits.flags.l2_state, "Min Level 2");
//             input->min_alarm.bits.flags.l3_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l3_level, input->min_alarm.bits.flags.l3_state, "Min Level 3");
//             std::cout << " alarm states for [" << item->name << "] min [" << std::hex<<(int) input->min_alarm.bits.state<< std::dec << "]" << std::endl;
//         }
//         if(input->num_stall_alarms > 0) // just one for now
//         {
//             input->stall_alarm.bits.flags.l1_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l1_level, input->stall_alarm.bits.flags.l1_state, "Min Level 1");
//             input->stall_alarm.bits.flags.l2_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l2_level, input->stall_alarm.bits.flags.l2_state, "Min Level 1");
//             input->stall_alarm.bits.flags.l3_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l3_level, input->stall_alarm.bits.flags.l3_state, "Min Level 1");
//             std::cout << " alarm states for [" << item->name << "] stall [" << std::hex<<(int) input->stall_alarm.bits.state<< std::dec << "]" << std::endl;
//         }
//     }
//     return 0;
// }


// // the alarm op saves its result in the outputs
// int group_alarm_op_def(opDef& opdef, std::vector<smObj*>*inputs, std::vector<smObj*>*outputs)
// {
//     int ret = group_alarm_op(opdef.sys_map, &opdef.inputs, &opdef.outputs);

//     printf("\n\nresults for [%s] \n\n", opdef.name.c_str());
//     //    std::cout << "Results:" << std::endl;
//     for (auto ob : opdef.outputs)
//     {
//         std::cout << ob->desc << std::endl;
//     }
//     std::cout << "==============" << std::endl;
//     opDef* opdef2 = find_opdef("run_group_alarms");
//     printf("\nusing  def \nresults for [%s] \n\n", opdef2->name.c_str());
//     //    std::cout << "Results:" << std::endl;
//     for (auto ob : opdef2->outputs)
//     {
//         std::cout << ob->desc << std::endl;
//     }
//     std::cout << "==============" << std::endl;


//     return ret;
// }


// // for each rack
// // this can be run on the collected aggregate on on each rack with different limits
// // this one scans the inputs and makes sure the values do not exceed the max/min/stall alarm limits
// // for now the limits are saved in the input vars
// // the outputs map is a map of all the valid alarms under an expanded name
// // the inputs in this case are all the rack limit variables we have to create one  
// // but we have to get the value from eack rack using rack_num
// int rack_alarm_op(std::map<std::string,std::shared_ptr<smObj>>&sys, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {

//     outputs->clear();
//     for (int rack_num = 0 ; rack_num < lcl_rack_max; rack_num++)
//     {
//         void* sm_rtos = get_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);
//         void* sm_rack = get_rack_mem(rack_num, shm_rack, rack_max, rack_size);

//         SbmuRack* my_rack = (SbmuRack*) ((uint8_t*)sm_rack + RACK_SM_MEM_SIZE);
//         if (!my_rack->rack_online)
//         {
//             std::cout << " rack " << rack_num << " is offline" << std::endl;
//             continue;
//         }

//         std::cout << " rack " << rack_num << " is online" << std::endl;
//         use_rack_mem(rack_num, shm_rtos, rack_max, rtos_size);

//         for (auto item: *inputs)
//         {   
//             smVar* input = find_smvar(item->name.c_str());
//             if(!input)
//             {
//                 std::cout << "     input [" << item->name << "] not found, skipping it "  << std::endl;
//                 continue;
//             }
//             smVar* output;
//             //find the individual rack var and get the value using rack_num
//             smVar* rack_input = find_rack_smvar(item->name.c_str(), rack_num);
//             if(!rack_input)
//             {
//                 std::cout << "     rack_input [" << item->name << "] not found, skipping it "  << std::endl;
//                 continue;
//             }

//             int value = lcl_get_mem_int_val_rack(input->sm_name, rack_num, input->reg_type, input->offset);
//             rack_input->value = value;//find_smvar(item.name.c_str());

//             std::cout << " run_rack_alarms value for [" << item->name << "]" 
//                     << " input name [" << input->name << "]"
//                     << " idx [" << (int)  item->idx << "]"
//                     << " value [" << (int)  rack_input->value << "]"
//                     << " num_max [" << (int)  input->num_max_alarms << "]"
//                     << " num_min [" << (int)  input->num_min_alarms << "]"
//                     << " num_stall [" << (int)  input->num_stall_alarms << "]"
//                     <<std::endl;

//             // Handle alarms
//             if(input->num_max_alarms > 0) // just one for now
//             {
//                 // TODO check if there are any alarms defined 
//                 input->max_alarm.bits.flags.l1_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l1_level, input->max_alarm.bits.flags.l1_state, "Max Level 1");
//                 input->max_alarm.bits.flags.l2_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l2_level, input->max_alarm.bits.flags.l2_state, "Max Level 2");
//                 input->max_alarm.bits.flags.l3_state = handle_max_alarm_var(outputs, input, rack_input, input->max_alarm.l3_level, input->max_alarm.bits.flags.l3_state, "Max Level 3");
//                 std::cout << " alarm states for [" << item->name << "] max [" << std::hex<<(int)  rack_input->max_alarm.bits.state<< std::dec << "]" << std::endl;
//             }
//             if(input->num_min_alarms > 0) // just one for now
//             {
//                 input->min_alarm.bits.flags.l1_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l1_level, input->min_alarm.bits.flags.l1_state, "Min Level 1");
//                 input->min_alarm.bits.flags.l2_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l2_level, input->min_alarm.bits.flags.l2_state, "Min Level 2");
//                 input->min_alarm.bits.flags.l3_state = handle_min_alarm_var(outputs, input, rack_input, input->min_alarm.l3_level, input->min_alarm.bits.flags.l3_state, "Min Level 3");
//                 std::cout << " alarm states for [" << item->name << "] min [" << std::hex<<(int)  rack_input->min_alarm.bits.state<< std::dec << "]" << std::endl;
//             }
//             if(input->num_stall_alarms > 0) // just one for now
//             {
//                 input->stall_alarm.bits.flags.l1_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l1_level, input->stall_alarm.bits.flags.l1_state, "Min Level 1");
//                 input->stall_alarm.bits.flags.l2_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l2_level, input->stall_alarm.bits.flags.l2_state, "Min Level 1");
//                 input->stall_alarm.bits.flags.l3_state = handle_stall_alarm_var(outputs, input, rack_input, input->stall_alarm.l3_level, input->stall_alarm.bits.flags.l3_state, "Min Level 1");
//                 std::cout << " alarm states for [" << item->name << "] stall [" << std::hex<<(int)  rack_input->stall_alarm.bits.state<< std::dec << "]" << std::endl;
//             }
//         }
//     }
//     return 0;
// }

// // the alarm op saves its result in the outputs
// int rack_alarm_op_def(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {
//     int ret = rack_alarm_op(opdef.sys_map, &opdef.inputs, &opdef.outputs);
//     printf("\n\nresults for [%s] \n\n", opdef.name.c_str());
//     //    std::cout << "Results:" << std::endl;

//     for (auto ob : opdef.outputs)
//     {
//         std::cout << ob->desc << std::endl;
//     }
//     std::cout << "==============" << std::endl;
//     return ret;
// }

// // transfer outputs to inputs or to other outputs
// int transfer_op_def(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {
//     std::cout << "run  [" << opdef.name << "]" << std::endl;
//     std::cout << "   opdefs outputs size [" << opdef.outputs.size() << "]" << std::endl;
//     std::cout << "   opdefs inputs size [" << opdef.inputs.size() << "]" << std::endl;

//     // switch inputs and outputs 
//     // for now just the first output and transfer it to all the inputs, or transfer it to the second output
//     if (opdef.outputs.size() == 2 ) 
//     {
//         opDef* src = find_opdef(opdef.outputs[0]->name.c_str());
//         opDef* dest = find_opdef(opdef.outputs[1]->name.c_str());

//         std::cout << "src [" << opdef.outputs[0]->name << "]" << std::endl;
//         std::cout << "dest [" << opdef.outputs[1]->name << "]" << std::endl;

//         dest->outputs = src->outputs;
//         src->outputs.clear();
//     }


//     // opdef.inputs contain a list of opps to run 
//     return 0;//alarm_op(opdef.sys_map, opdef.inputs, opdef.outputs);
// }

// int timer_op_def(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {
//     // set up timer and run the selected system until stopped 
//     // the input name is the value  the item map will have period , offset and frequency 
//     return 0;
// }    

// int case_op_def(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {
//     // TODO pick an input that matches the opdef value 
//     return 0;
// }

// int while_op_def(std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
// {
//     //run while the opdef value matches something
//     return 0;
// }

// run the function named in the opdef with its optional inpouts and outputs
int run_opdef_func(std::shared_ptr<smObj>root, std::shared_ptr<smObj> opDef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
{
    //std::cout << " run_opdef_func root name  " << root->name << std::endl; 
    //std::cout << " opdef name  " << opDef->name << std::endl; 
    auto obj = find_map_item(opDef, "func");
    if (!obj)
    {
        std::cout << " no function  in opDef  " << opDef->name << std::endl; 
        return 0;

    }
    auto fname = obj->value;
    std::cout << " found func name [" << opDef->obj_map["func"]->value   << "] in opDef"<< std::endl;        

    if (root->ext == nullptr)
    {
        std::cout << " no root extensions in " << root->name << std::endl; 
        return 0;
    }


    if (opDef->ext == nullptr)
    {
        std::cout << " no extensions in " << opDef->name << " use root ext instead " << std::endl; 

        root->ext->call_func(fname, root, inputs, outputs);

        return 0;
    }

    // this runs the opdef in its own environement if needed
    // note we have dropped root
    // TODO maybe opDef->ext->call_func(root, opdef, inputs, outputs);
    opDef->ext->call_func(fname, opDef, inputs, outputs);
    return 0;
}


// find an opdef by name in root.opdefs
// if we find it run it
// it may have inputs and outputs and will have a func
std::shared_ptr<smObj>find_opdef(std::shared_ptr<smObj> root, const std::string &name)
{
    return find_map_item(root, "opdefs", name);
}

void show_map_keys(std::shared_ptr<smObj> root)
{
    std::cout<< " map keys for obj ["<<root->name<<"]"<< std::endl;
    for (auto [key , item]: root->obj_map) 
    {
        std::cout<< " -- ["<<key<<"]"<< std::endl;
    } 
}

void show_array_names(std::shared_ptr<smObj>root) 
{
    std::cout << " Array items for ["<< root->name << "]"<<std::endl; 
    for (auto item: root->children)
    {   
        std::cout << " -- ["<< item->name <<"]"<< std::endl;;
    }
}



// thisone
std::shared_ptr<smObj>run_opdef_name(std::shared_ptr<smObj> root, const std::string &name, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs)
{

    auto opDef = find_opdef(root, name);
    if(!opDef)
    {
        std::cout << " run_opdef_name >>> did not find root opDef ["<< name <<"]"<< std::endl;
        return nullptr;
    }

    //show_map_keys(opDef);

    std::cout << " run_opdef_name >>> found root opDef ["<< name <<"]"<< std::endl;
    auto obj = find_map_item(opDef, "inputs");
    if (obj)
    {
        inputs = obj;
        std::cout << " run_opdef_name >>> found  root opDef ["<< name <<"] inputs"<< std::endl;
    }
    obj = find_map_item(opDef, "outputs");
    if (obj)
    {
        outputs = obj;
        std::cout << " run_opdef_name >>> found  root opDef ["<< name <<"] outputs"<< std::endl;
    }

    obj = find_map_item(opDef, "func");
    if (obj)
    {
        //outputs = obj;
        std::cout << " run_opdef_name >>> found  root opDef ["<< name <<"] func ["<< obj->value<<"]" << std::endl;
    }

    if(obj)
    {
        auto fname = obj->value;
        std::cout << " found func value [" << fname   << "] in opDef"<< std::endl;        
        // the function name was found in the opdef
        // the functipn will be defined in the opDef extObj or the root extObj
        run_opdef_func(root, opDef, inputs, outputs);
        return opDef;
    }

    return nullptr;


}


    // we may need global and individual enables on the sequence inputs
    // run_alarm_seq opdef = R"({"name": "run_alarm_seq", "func": "sequence_op", 
    //     "inputs": [{"name": "run_agg"}, {"name": "run_group_alarms"}, {"name": "transfer_group_alarms"}]})";

// find all the opdefs names in inputs  and run them 
int sequence_op_def(std::shared_ptr<smObj> root, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs) 
{
    // // find the opdefs in root
    // if (!opdef) {
    //     std::cerr << "Error: opdef is null." << std::endl;
    //     return -1;
    // }

    // std::cout << "Start running sequence [" << opdef->name << "]" << std::endl;

    // std::shared_ptr<smObj> last_op = nullptr;

    // for (const auto& op : opdef->children) { // Assuming obj_vec holds input operations
    //     std::cout << "Running op [" << op->name << "]" << std::endl;

    //     // Call another operation (mocked as an example)
    //     //last_op = std::make_shared<smObj>("last_op_result"); // Replace with actual call logic
    //     last_def = run_opdef_name(op->name.c_str(), inputs, outputs); 

    // }

    // if (last_op) {
    //     // Propagate last operation's outputs to the sequence outputs
    //     outputs->obj_map = last_op->obj_map;
    // }

    std::cout << "End running sequence [" << root->name << "]" << std::endl;
    return 0;
}

//void register_func(root, "sequence_op",    sequence_op_def); // run a sequence of operations
void register_func(std::shared_ptr<smObj>root, const std::string& name,  const std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>& func) // run a sequence of operations
{
    auto ext = root->get_ext();
    ext->register_func(name, func);
}

// Test the new functions
int main() {
        std::string json_string = R"({
        "sys":{"env":{"svar1":"svarone","ivar1":234,"fvar1":34.56}},
        "func":"myfunc",
        "args":{"arg1":"svarone","arg2":234,"arg3":34.56},
        "inputs": [
            {
                "name": "invar1",
                "bval": false,
                "children": {
                    "child1": {"subchild": 10},
                    "child2": [1, 2, 3]
                }
            },
            {
                "name": "var2",
                "ival": 21
            },
            [
                {"array_child": "value1"},
                {"array_child": "value2"}
            ]
        ],
        "outputs": [
            {
                "name": "outvar1",
                "bval": false,
                "children": {
                    "child1": {"subchild": 10},
                    "child2": [1, 2, 3]
                }
            },
            {
                "name": "var2",
                "ival": 21
            },
            [
                {"array_child": "value1"},
                {"array_child": "value2"}
            ]
        ]

    })";

    // Parse the JSON string
    json j = json::parse(json_string);

    // Parse the entire JSON object into smObj
    auto root = parse_object(j);

    json_string = R"({
        "name": "group_curr","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x2","latched": false,
        "max_alarms": [{"level1": 3000, "level2": 3200, "level3": 3400, "hist": 50}],
        "min_alarms": [{"level1": -3000, "level2": -3200, "level3": -3400, "hist": 50}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    add_map_item(root ,"smVars", "group_curr", json_string);

    json_string = R"({
        "name": "group_volt","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x1","latched": false,
        "max_alarms": [{"level1": 13000, "level2": 13200, "level3": 13400, "hist": 100}],
        "min_alarms": [{"level1": 12600, "level2": 12500, "level3": 12400, "hist": 100}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    add_map_item(root ,"smVars", "group_volt", json_string);

    json_string = R"({
        "name": "rack_curr","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x2","latched": false,
        "max_alarms": [{"level1": 3000, "level2": 3200, "level3": 3400, "hist": 50}],
        "min_alarms": [{"level1": -3000, "level2": -3200, "level3": -3400, "hist": 50}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    add_map_item(root ,"smVars", "rack_curr", json_string);

    json_string = R"({
        "name": "rack_volt","source": "rack","reg_type": "mb_input","sm_name": "rack","offset": "0x1","latched": false,
        "max_alarms": [{"level1": 13000, "level2": 13200, "level3": 13400, "hist": 100}],
        "min_alarms": [{"level1": 12600, "level2": 12500, "level3": 12400, "hist": 100}],
        "stall_alarms": [{"level1": 1000, "level2": 1100, "level3": 1200, "hist": 30}]
        })";
    add_map_item(root ,"smVars", "rack_volt", json_string);

    json_string = R"({
     "name": "globals", 
     "sys": {"enabled": true, "num_vars": 2, "lcl_rack_max":12 }
    })";
    add_map_item(root ,"opdefs", "globals", json_string);

    json_string = R"({
     "name": "run_agg", "func": "agg_op", 
     "inputs": ["group_curr", "group_volt"],
     "outputs": [{"name": "result"}]
    })";
    add_map_item(root ,"opdefs", "run_agg", json_string);

     // possibly we create a named data block called results to get to this data 
    json_string = R"({"name": "run_group_alarms", "func": "group_alarm_op",
         "inputs": [{"name": "group_curr"}, {"name": "group_volt"}]})";
    add_map_item(root ,"opdefs", "run_group_alarm", json_string);

     // final block to transfer run_group_alarms outputs to run_group_alarms  
    json_string = R"({"name": "transfer_group_alarms", "func": "transfer_op",
         "outputs": [{"name": "run_group_alarms"}, {"name": "run_alarm_seq"}]
         })";

    add_map_item(root ,"opdefs", "transfer_group_alarms", json_string);

    // we may need global and individual enables on the sequence inputs
    json_string = R"({"name": "run_alarm_seq", "func": "sequence_op", 
        "inputs": [{"name": "run_agg"}, {"name": "run_group_alarms"}, {"name": "transfer_group_alarms"}]})";
    add_map_item(root ,"opdefs", "run_alarm_seq", json_string);

    // we may need global and individual enables on the sequence inputs
    json_string = R"({"name": "collect_rack_status", "func": "rack_agg_op", 
        "inputs": [{"name": "MinSOC", "rack_idx":34,"sbms_idx":201}, {"name": "run_group_alarms"}, {"name": "transfer_group_alarms"}]})";
    add_map_item(root ,"opdefs", "collect_rack_status", json_string);

    register_func(root, "sequence_op",    sequence_op_def); // run a sequence of operations
    register_func(root, "agg_op",         agg_op);  // run the aggregate function on all "online" racks


    // j = json::parse(json_string);
    // auto smVar = parse_object(j);


    // //set_obj_value(root, "smVars.group_curr", smVar);

    // //auto root = std::make_shared<smObj>("root");
    // root->obj_map["smVars"] = std::make_shared<smObj>("smVars");
    // root->obj_map["smVars"]->obj_map["group_curr"] = smVar;//std::make_shared<smObj>("smVars");


    // Set values
    set_obj_value(root, "args.arg4", "1234");
    set_obj_value(root, "inputs[0].name", "var_name");
    set_obj_value(root, "outputs[1].value", "142");
    set_obj_value(root, "outputs[3].value", "242");

// Append new items to an array
    append_to_array(root, "inputs", std::make_shared<smObj>("new_item1", "value1"));
    append_to_array(root, "inputs", std::make_shared<smObj>("new_item2", "value2"));

    auto obj31 = std::make_shared<smObj>("new_item3", "value3");

    // Get and print values
    auto obj1 = get_obj_value(root, "args.arg4");
    auto obj2 = get_obj_value(root, "inputs[0].name");
    auto obj3 = get_obj_value(root, "outputs[1].value");

    if (obj1) std::cout << "args.arg4: " << obj1->value << std::endl;
    if (obj2) std::cout << "inputs[0].name: " << obj2->value << std::endl;
    if (obj3) std::cout << "outputs[1].value: " << obj3->value << std::endl;

    print_as_json(root);
    // Get and print array size
    size_t inputs_size = get_array_size(root, "inputs");
    std::cout << "Size of 'inputs': " << inputs_size << std::endl;
    if(root->obj_map.find("inputs") != root->obj_map.end()) 
    {
        std::cout << " found the inputs object" << std::endl;
        auto inputs =root->obj_map["inputs"];
        root->obj_map["test_inputs"] = inputs;
        inputs->children.emplace_back (obj31);
    inputs_size = get_array_size(root, "inputs");
    std::cout << "new Size of 'inputs': " << inputs_size << std::endl;
    print_as_json(inputs);
    }
    std::cout << " final obj" << std::endl;
    set_obj_value(root, "test_inputs[0].test0.value", "test_val_3");
    set_obj_value(root, "test_inputs[0].test0.ival", "130");
    set_obj_value(root, "test_inputs[1].test1.ival", "131");
    set_obj_value(root, "test_inputs[8].test8.ival", "138");

    print_as_json(root);

    //print_as_json(smVar);

    printf(" size of smObj %d\n", (int)sizeof(smObj));

    //do whatever the opdef run_agg wants to do
    // use default inputs and outputs
    run_opdef_name(root, "run_agg", nullptr, nullptr); 

    

    return 0;
}

#if 0
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
#endif
