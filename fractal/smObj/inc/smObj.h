#ifndef SMOBJ_H
#define SMOBJ_H

#include <any>
#include <typeinfo>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp> // Include the JSON library

using json = nlohmann::json;


// Forward declaration
class smExt;

// smObj structure
class smObj {
    public:
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
    smObj(){};
    virtual ~smObj() = default;

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
class smExt {
    public:
    std::shared_ptr<smObj> parent;
    std::shared_ptr<smObj> child;


        // Function registry with the updated signature
    std::unordered_map<std::string, std::function<int(std::shared_ptr<smObj>, std::shared_ptr<smObj>, std::shared_ptr<smObj>)>> func_map;

    // Map to store any type of value
    std::unordered_map<std::string, std::any> value_map;

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

    // Set a value in the value_map
    template <typename T>
    void set_value(const std::string& key, const T& value) {
        value_map[key] = value;
    }

    // Get a value from the value_map
    template <typename T>
    T get_value(const std::string& key) {
        if (value_map.find(key) == value_map.end()) {
            throw std::runtime_error("Key '" + key + "' not found in value_map.");
        }
        try {
            return std::any_cast<T>(value_map[key]);
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error("Type mismatch for key '" + key + "': " + std::string(e.what()));
        }
    }

    // Check the type of a value
    std::string get_type(const std::string& key) {
        if (value_map.find(key) == value_map.end()) {
            return "Key not found";
        }

        const std::type_info& type = value_map[key].type();

        if (type == typeid(bool)) {
            return "bool";
        } else if (type == typeid(int)) {
            return "int";
        } else if (type == typeid(float)) {
            return "float";
        } else if (type == typeid(std::string)) {
            return "string";
        } else if (type == typeid(std::shared_ptr<smObj>)) {
            return "smObj";
        } else {
            return "unknown type";
        }
    }

};


// Derived class
class smVar : public smObj {
public:
    int extra_int;
    smVar(const std::string& n) : smObj(n), extra_int(0) {}
    // Additional members...
};

void show_map_keys(std::shared_ptr<smObj> root);
void show_array_names(std::shared_ptr<smObj>root);


#endif // SMOBJ_H