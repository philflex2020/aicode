#ifndef SMOBJ_H
#define SMOBJ_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp> // Include the JSON library

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
//     std::vector<std::shared_ptr<smObj>> obj_vec;     // Array of child objects
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

//         // Add obj_vec
//         if (!obj_vec.empty()) {
//             json array_json = json::array();
//             for (const auto& child : obj_vec) {
//                 array_json.push_back(child->to_json());
//             }
//             j = array_json;
//         }

//         return j;
//     }
// };



// Forward declaration
struct smExt;

// smObj structure
struct smObj {
    std::string name;                                 // Object name

    std::map<std::string, int> attributes;           // Key-value attributes
    std::map<std::string, std::shared_ptr<smObj>>obj_map;           // Key-value attributes
 
    std::vector<std::shared_ptr<smObj>> children;    // Child objects
    int size = 1;                                    // Default size
    int location = -1;                               // Shared memory location (-1 if unset)
    std::string value;                                      // Stores the value if not an object or array

    std::shared_ptr<smExt> ext;                      // Optional extensions

    //smObj(const std::string& obj_name) : name(obj_name), ext(nullptr) {}
    smObj(const std::string& name, const std::string& value = "")
         : name(name), value(value), ext(nullptr) {}

    // Get the smExt, create if necessary
    std::shared_ptr<smExt> get_ext() {
        if (!ext) {
            ext = std::make_shared<smExt>();
        }
        return ext;
    }

    // Debug print
    void debug_print(int depth = 0) const {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Object: " << name << ", Size: " << size << ", Location: " << location << "\n";
        for (const auto& [key, value] : attributes) {
            std::cout << indent << "  Attribute: " << key << " = " << value << "\n";
        }
        for (const auto& child : children) {
            child->debug_print(depth + 1);
        }
    }

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
        if (!children.empty()) {
            json array_json = json::array();
            for (const auto& child : children) {
                array_json.push_back(child->to_json());
            }
            j = array_json;
        }

        return j;
    }
};

// smExt structure for extensions
struct smExt {
    std::shared_ptr<smObj> parent;
    std::shared_ptr<smObj> child;

        // Function registry with the updated signature
    std::unordered_map<std::string, std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>> func_map;

    // Register a named function
    void register_func(const std::string& name, const std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>& func) {
        func_map[name] = func;
    }

    // Call a registered function
    int call_func(const std::string& name, std::shared_ptr<smObj> opdef, std::shared_ptr<smObj> inputs, std::shared_ptr<smObj> outputs) 
    {
        std::string fname = name;
        if (fname.front() == '"' && fname.back() == '"') {
            fname = fname.substr(1, fname.size() - 2); // Remove quotes
        }

        if (func_map.find(fname) != func_map.end()) {
            return func_map[fname](opdef, inputs, outputs);
        } else {
            std::cout << " function [" << fname << "] not found in "<< opdef->name << std::endl;
            std::cout << " function keys:" << std::endl;
            for ( auto [key, item]: func_map )
            {
                std:: cout << " key ["<< key <<"]" << std::endl;
            }
            //throw std::runtime_error("Function '" + name + "' not found in smExt.");
        }
        return 0;
    }


    // std::map<std::string, std::function<int(std::shared_ptr<smObj>, std::string)>> func_map; // Function registry

    // // Register a function
    // void register_func(const std::string& name, const std::function<int(std::shared_ptr<smObj>, std::string)>& func) {
    //     func_map[name] = func;
    // }

    // // Call a registered function
    // int call_func(const std::string& name, std::shared_ptr<smObj> obj, const std::string& args) {
    //     if (func_map.find(name) != func_map.end()) {
    //         return func_map[name](obj, args);
    //     } else {
    //         throw std::runtime_error("Function '" + name + "' not found in smExt.");
    //     }
    // }
};

#endif // SMOBJ_H