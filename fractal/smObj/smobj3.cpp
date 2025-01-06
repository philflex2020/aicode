#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp> // Include the JSON library

//g++ -g -o smobj smobj3.cpp 

using json = nlohmann::json;

struct smObj {
    std::string name;
    std::map<std::string, std::shared_ptr<smObj>> obj_map; // Child objects
    std::vector<std::shared_ptr<smObj>> obj_vec;     // Array of child objects
    std::string value;                                      // Stores the value if not an object or array
    std::shared_ptr<smObj> child;
    std::shared_ptr<smObj> parent;


    smObj(const std::string& name, const std::string& value = "")
        : name(name), value(value), child(nullptr), parent(nullptr) {}

    // Convert this object to JSON
    json to_json() const {
        json j;

        if (!value.empty()) {
            // Check if the value represents a primitive type (number, boolean, or string)
            try {
                if (value == "true" || value == "false") {
                    return value == "true";
                } else if (value.find('.') != std::string::npos) {
                    return std::stod(value); // Float or double
                } else {
                    return std::stoi(value); // Integer
                }
            } catch (...) {
                // If parsing fails, treat it as a string
                if (value.front() == '"' && value.back() == '"') {
                    return value.substr(1, value.size() - 2); // Remove quotes
                }
                return value; // Return as-is if not quoted
            }
        }

        // Add obj_map ects
        for (const auto& [key, child] : obj_map) {
            j[key] = child->to_json();
        }

        // Add obj_vec
        if (!obj_vec.empty()) {
            json array_json = json::array();
            for (const auto& child : obj_vec) {
                array_json.push_back(child->to_json());
            }
            j = array_json;
        }

        return j;
    }
};

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
            while (current->obj_vec.size() <= index) {
                current->obj_vec.push_back(std::make_shared<smObj>("array_item"));
            }

            current = current->obj_vec[index];
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

    return target->obj_vec.size();
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

    current->obj_vec.push_back(new_obj);
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

            if (index >= current->obj_vec.size()) {
                return nullptr; // Index out of bounds
            }

            current = current->obj_vec[index];
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

// Parse the entire JSON object recursively into smObj
std::shared_ptr<smObj> parse_object(const json& j, const std::string& name = "root") {
    auto obj = std::make_shared<smObj>(name);

    if (j.is_object()) {
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
            obj->obj_vec.push_back(parse_object(item, "array_item"));
        }
    } else {
        obj->value = j.dump();
    }

    return obj;
}

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
    for (const auto& array_child : source->obj_vec) {
        copy->obj_vec.push_back(deep_copy(array_child));
    }

    return copy;
}

// Print function to recreate JSON from smObj
void print_as_json(const std::shared_ptr<smObj>& obj) {
    json j = obj->to_json();
    std::cout << j.dump(4) << std::endl; // Pretty print with 4 spaces indentation
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
    
    j = json::parse(json_string);
    auto smVar = parse_object(j);

    //auto root = std::make_shared<smObj>("root");

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
        inputs->obj_vec.emplace_back (obj31);
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

    print_as_json(smVar);

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
