/*
---

### Breakdown of Requirements

1. **Header Structure Representation**:
   - Define `typedef` structures with:
     - A name.
     - Overall size.
     - A list of elements.

2. **Elements**:
   - Each element has:
     - A name.
     - A type (with no circular references).
     - Byte offset (position in the typedef).
     - Optional bit number and bit offset (for bitfields).

3. **JSON Output**:
   - Serialize the `typedef` map into JSON, with one structure per element.

4. **Instances**:
   - Define instances of the typedefs with:
     - Names.
     - Offsets.
     - Optional bit definitions.

---

### Implementation Plan

1. **Define the Data Structures**:
   - Represent typedefs and elements in C++.

2. **Parsing the Header**:
   - Parse the header manually or with a library to populate typedefs and elements.

3. **Serialize to JSON**:
   - Use `nlohmann::json` to represent the typedefs and instances as JSON.

4. **Represent Instances**:
   - Decode the instances with offsets and optional bit definitions.

---

### C++ Code Implementation

#### Step 1: Define Data Structures

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Element structure
struct Element {
    std::string name;
    std::string type;
    uint32_t offset;       // Byte offset within the typedef
    uint32_t bit_number;   // Optional: Bit number for bitfields
    uint32_t bit_offset;   // Optional: Bit offset within the byte

    // Serialize to JSON
    json to_json() const {
        return {
            {"name", name},
            {"type", type},
            {"offset", offset},
            {"bit_number", bit_number},
            {"bit_offset", bit_offset}
        };
    }
};

// Typedef structure
struct Typedef {
    std::string name;
    uint32_t size;                    // Overall size of the typedef
    std::vector<Element> elements;   // List of elements

    // Serialize to JSON
    json to_json() const {
        json elements_json = json::array();
        for (const auto& elem : elements) {
            elements_json.push_back(elem.to_json());
        }
        return {
            {"name", name},
            {"size", size},
            {"elements", elements_json}
        };
    }
};

// Instance structure
struct Instance {
    std::string name;
    std::string typedef_name;
    uint32_t offset;   // Offset of the instance
    uint32_t bit_def;  // Optional: Bit definition

    // Serialize to JSON
    json to_json() const {
        return {
            {"name", name},
            {"typedef_name", typedef_name},
            {"offset", offset},
            {"bit_def", bit_def}
        };
    }
};
```

---

#### Step 2: Populate Typedefs and Elements

```cpp
void populate_typedefs(std::unordered_map<std::string, Typedef>& typedefs) {
    // AlarmHist Typedef
    Typedef alarm_hist;
    alarm_hist.name = "AlarmHist";
    alarm_hist.size = 0;  // Empty struct
    typedefs[alarm_hist.name] = alarm_hist;

    // Alarm Typedef
    Typedef alarm;
    alarm.name = "Alarm";
    alarm.size = 68;  // Example calculated size

    alarm.elements.push_back({"max_value", "uint32_t", 0, 0, 0});
    alarm.elements.push_back({"min_value", "uint32_t", 4, 0, 0});
    alarm.elements.push_back({"max_set", "uint16_t", 8, 0, 0});
    alarm.elements.push_back({"min_set", "uint16_t", 8, 1, 1});
    alarm.elements.push_back({"hist", "AlarmHist[16]", 10, 0, 0});

    typedefs[alarm.name] = alarm;
}
```

---

#### Step 3: Serialize Typedefs and Instances to JSON

```cpp
void export_to_json(const std::unordered_map<std::string, Typedef>& typedefs, 
                    const std::vector<Instance>& instances) {
    json output;

    // Serialize typedefs
    json typedefs_json = json::array();
    for (const auto& [name, tdef] : typedefs) {
        typedefs_json.push_back(tdef.to_json());
    }
    output["typedefs"] = typedefs_json;

    // Serialize instances
    json instances_json = json::array();
    for (const auto& inst : instances) {
        instances_json.push_back(inst.to_json());
    }
    output["instances"] = instances_json;

    // Print the JSON
    std::cout << output.dump(4) << std::endl;
}
```

---

#### Step 4: Main Function

```cpp
int main() {
    // Typedef map
    std::unordered_map<std::string, Typedef> typedefs;
    populate_typedefs(typedefs);

    // Instances
    std::vector<Instance> instances = {
        {"alarm1", "Alarm", 0, 0},
        {"alarm2", "Alarm", 100, 0}
    };

    // Export to JSON
    export_to_json(typedefs, instances);

    return 0;
}
```

---

### Example Output

For the given `AlarmHist` and `Alarm` typedefs and instances:

```json
{
    "typedefs": [
        {
            "name": "AlarmHist",
            "size": 0,
            "elements": []
        },
        {
            "name": "Alarm",
            "size": 68,
            "elements": [
                {
                    "name": "max_value",
                    "type": "uint32_t",
                    "offset": 0,
                    "bit_number": 0,
                    "bit_offset": 0
                },
                {
                    "name": "min_value",
                    "type": "uint32_t",
                    "offset": 4,
                    "bit_number": 0,
                    "bit_offset": 0
                },
                {
                    "name": "max_set",
                    "type": "uint16_t",
                    "offset": 8,
                    "bit_number": 0,
                    "bit_offset": 0
                },
                {
                    "name": "min_set",
                    "type": "uint16_t",
                    "offset": 8,
                    "bit_number": 1,
                    "bit_offset": 1
                },
                {
                    "name": "hist",
                    "type": "AlarmHist[16]",
                    "offset": 10,
                    "bit_number": 0,
                    "bit_offset": 0
                }
            ]
        }
    ],
    "instances": [
        {
            "name": "alarm1",
            "typedef_name": "Alarm",
            "offset": 0,
            "bit_def": 0
        },
        {
            "name": "alarm2",
            "typedef_name": "Alarm",
            "offset": 100,
            "bit_def": 0
        }
    ]
}
```

---

### Summary

1. **Typedefs and Elements**:
   - Represent the structure of `typedef`s, including elements with offsets, types, and optional bitfields.

2. **Instances**:
   - Represent actual instances of these typedefs with offsets and bit definitions.

3. **JSON Serialization**:
   - Converts the typedefs and instances into JSON format for further use or validation.

*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <cstddef>
#include <nlohmann/json.hpp>
#include "example.h"
using json = nlohmann::json;

#include <cassert>

// Map of fixed sizes for basic types
std::unordered_map<std::string, size_t> fixed_sizes = {
    {"uint32_t", sizeof(uint32_t)},
    {"int32_t",  sizeof(int32_t)},
    {"uint16_t", sizeof(uint16_t)},
    {"int16_t",  sizeof(int16_t)},
    {"uint8_t",  sizeof(uint8_t)},
    {"int8_t",   sizeof(int8_t)},
    {"bool",     sizeof(bool)},
    {"double",   sizeof(double)},
    {"float",    sizeof(float)}
};

// Element structure
struct Element {
    std::string name;
    std::string type;
    std::string m3;
    std::string m4;
    int offset;       // Byte offset within the typedef
    int bit_number;   // Optional: Bit number for bitfields
    int bit_offset;   // Optional: Bit offset within the byte
    int array_size;
    int size;

    json to_json() const {
        return {
             {"name", name}
            ,{"type", type}
            ,{"array_size", array_size}
            ,{"bit_number", bit_number}
        };
    }

};

// Typedef structure
struct Typedef {
    std::string name;
    uint32_t size;                    // Overall size of the typedef
    std::vector<Element> elements;   // List of elements

    json to_json() const {
        json elements_json = json::array();
        for (const auto& elem : elements) {
            elements_json.push_back(elem.to_json());
        }
        return {
            {"name", name},
            {"size", size},
            {"elements", elements_json}
        };
    }

    static Typedef from_json(const json& j) {
        Typedef tdef;
        tdef.name = j.at("name");
        int idx = 0;
        for (const auto& elem : j.at("elements")) {
            Element el;
            el.name = elem.at("name");
            el.type = elem.at("type");
            el.bit_number = elem.at("bit_number");
            el.array_size = elem.at("array_size");
            //printf("decode elem %d\n", idx++);
            tdef.elements.push_back(el);
        }
        return tdef;
    }
};


void show_path(std::vector<std::string>& components)
{
    int idx = 0;
    for (auto & comp : components)
    {
        std::cout << " idx " << idx <<" "<< comp << std::endl;
        idx++;
    }
}

size_t yresolve_offset(const std::unordered_map<std::string, Typedef>& typedefs,
                      Typedef& current_typedef,
                      const std::vector<std::string>& path_components,
                      size_t current_index = 0) {
    size_t offset = 0;

    // Validate the initial typedef
    if (current_index == 0) {
        const auto& comp = path_components[0];
        auto first_type = typedefs.find(comp);
        if (first_type == typedefs.end()) {
            throw std::runtime_error("Unable to find initial typedef: " + comp);
        }
        current_typedef = first_type->second;
        current_index++;
    }

    // Process the path components
    while (current_index < path_components.size()) {
        const std::string& component = path_components[current_index];
        bool found = false;

        for (const auto& elem : current_typedef.elements) {
            if (elem.name == component) {
                // Handle arrays if present
                if (elem.array_size > 0 && current_index + 1 < path_components.size()) {
                    std::string index_str = path_components[++current_index];
                    size_t array_index = std::stoul(index_str);

                    if (array_index >= elem.array_size) {
                        throw std::out_of_range("Array index out of bounds: " + index_str);
                    }

                    offset += elem.offset + array_index * typedefs.at(elem.type).size;
                } else {
                    offset += elem.offset;
                }

                // Recurse into the typedef if the type is nested
                if (typedefs.find(elem.type) != typedefs.end() && current_index + 1 < path_components.size()) {
                    current_typedef = typedefs.at(elem.type);
                    found = true;
                    break;
                }

                // Update the typedef for the current component
                if (typedefs.find(elem.type) != typedefs.end()) {
                    current_typedef = typedefs.at(elem.type);
                }
                return offset;
            }
        }

        if (!found) {
            throw std::runtime_error("Element not found in typedef: " + component);
        }
    }

    return offset;
}

size_t resolve_offset(const std::unordered_map<std::string, Typedef>& typedefs,
                      Typedef& current_typedef,
                      const std::vector<std::string>& path_components,
                      size_t current_index = 0) {
    size_t offset = 0;
    auto comp = path_components[0];
    auto first_type = typedefs.find(comp) ;
    if (first_type == typedefs.end())
    {
        std::cout << " unable to find first type " << comp << std::endl; 
        return 0;
    }
    current_typedef = typedefs.at(comp);
    current_index++;

    for (const auto& elem : current_typedef.elements) {
        if (elem.name == path_components[current_index]) {
            // Handle arrays
            if (elem.array_size > 0 && current_index + 1 < path_components.size()) {
                std::string index_str = path_components[++current_index];
                size_t array_index = std::stoul(index_str);

                if (array_index >= elem.array_size) {
                    throw std::out_of_range("Array index out of bounds");
                }

                offset += elem.offset + array_index * typedefs.at(elem.type).size;
            } else {
                offset += elem.offset;

                if (typedefs.find(elem.type) != typedefs.end() && current_index + 1 < path_components.size()) {
                    current_typedef = typedefs.at(elem.type);
                    return offset + resolve_offset(typedefs, current_typedef, path_components, current_index + 1);
                }
            }

            if (typedefs.find(elem.type) != typedefs.end()) {
                current_typedef = typedefs.at(elem.type);
            }
            return offset;
        }
    }
    throw std::runtime_error("Element not found: " + path_components[current_index]);
}
// 
void parse_path(std::vector<std::string>& components,const std::string& path) {
    //std::vector<std::string> components;
    std::stringstream ss(path);
    std::string item;

    // Regular expression to match array syntax (e.g., hist[3])
    std::regex array_regex(R"(^(\w+)\[(\d+)\]$)");
    std::smatch match;

    while (std::getline(ss, item, '.')) {
        // Check if the item matches the array syntax
        if (std::regex_match(item, match, array_regex)) {
            // Add the array name (e.g., "hist")
            components.push_back(match[1]);
            // Add the index (e.g., "3")
            components.push_back(match[2]);
        } else {
            // Add the item as-is
            components.push_back(item);
        }
    }

    return;// components;
}

void test_path(std::vector<std::string>& components, std::string& mypath)
{
    //std::string mypath("Alarm.hist[3].max_current");
    parse_path(components, mypath);
    show_path(components);
}


void validate_offsets_and_sizes() {
    // Validate AlarmHist
    std::cout << "Struct: AlarmHist" << std::endl;
    std::cout << "  offsetof(max_value): " << offsetof(AlarmHist, max_value) << std::endl;
    std::cout << "  offsetof(min_value): " << offsetof(AlarmHist, min_value) << std::endl;
    std::cout << "  sizeof(AlarmHist): " << sizeof(AlarmHist) << std::endl;

    // Validate Alarm
    std::cout << "\nStruct: Alarm" << std::endl;
    std::cout << "  offsetof(max_value): " << offsetof(Alarm, max_value) << std::endl;
    std::cout << "  offsetof(min_value): " << offsetof(Alarm, min_value) << std::endl;
    std::cout << "  offsetof(hist): " << offsetof(Alarm, hist) << std::endl;
    std::cout << "  sizeof(Alarm): " << sizeof(Alarm) << std::endl;
}

size_t test_calculate_typedef_size(Typedef& tdef, std::unordered_map<std::string, Typedef>& typedefs) {
    size_t current_offset = 0;
    size_t bit_offset = 0; // Current bit position in the unit
    size_t bit_size = 0;   // Size of the current unit in bits

    for (auto& elem : tdef.elements) {
        size_t elem_size = 0;

        // Check if the type is a fixed-size type
        if (fixed_sizes.find(elem.type) != fixed_sizes.end()) {
            elem_size = fixed_sizes[elem.type];
        }
        // Handle nested typedefs
        else if (typedefs.find(elem.type) != typedefs.end()) {
            elem_size = test_calculate_typedef_size(typedefs.at(elem.type), typedefs);
        } else {
            throw std::runtime_error("Unknown type: " + elem.type);
        }

        if (elem.bit_number > 0) {
            // Align to the next unit if the bit-field doesn't fit in the current unit
            if (bit_offset + elem.bit_number > bit_size) {
                current_offset += (bit_size + 7) / 8; // Move to the next unit
                bit_offset = 0;
            }

            elem.offset = current_offset;
            elem.bit_offset = bit_offset;
            elem.size = (elem.bit_number + 7) / 8; // Size in bytes, rounded up
            bit_offset += elem.bit_number;

            // If the bit-field spans multiple units, adjust the current offset
            if (bit_offset >= elem_size * 8) {
                current_offset += (bit_offset / (elem_size * 8)) * elem_size;
                bit_offset %= (elem_size * 8);
            }

        } else {
            // Finalize the current unit if there's an ongoing bit-field
            if (bit_offset > 0) {
                current_offset += (bit_size + 7) / 8; // Move to the next unit
                bit_offset = 0;
                bit_size = 0;
            }

            // Regular element alignment
            elem.size = elem_size;
            if (elem.array_size > 0) {
                elem.size = elem_size * elem.array_size;
            }

            // Align the current offset
            size_t alignment = elem_size;
            current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
            elem.offset = current_offset;

            // Update the current offset
            current_offset += elem.size;
        }
    }

    // Finalize any remaining bit-field unit
    if (bit_offset > 0) {
        current_offset += (bit_size + 7) / 8;
    }

    return current_offset;
}

// Function to calculate size for a typedef
size_t calculate_typedef_size(Typedef& tdef, std::unordered_map<std::string, Typedef>& typedefs) {
    size_t current_offset = 0;
    size_t bit_offset = 0;
    size_t bit_size = 0;

    for (auto& elem : tdef.elements) {
        size_t elem_size = 0;

        // Check if the type is a fixed size type
        if (fixed_sizes.find(elem.type) != fixed_sizes.end()) {
            elem_size = fixed_sizes[elem.type];
            //printf( " elem fixed size %d for type [%s]\n", (int)elem_size, elem.type.c_str());
        }
        // Handle nested typedefs
        else if (typedefs.find(elem.type) != typedefs.end()) {
            elem_size = calculate_typedef_size(typedefs.at(elem.type), typedefs);
        } else {
            throw std::runtime_error("Unknown type: " + elem.type);
        }

        if(elem.bit_number > 0)
        {
            elem.offset = current_offset;
            // this wont work if bit number >=elem_size * 8
            if(((bit_offset + elem.bit_number) > (elem_size * 8)))// || ( elem.size != bit_size))
            {
                bit_offset = 0;
                current_offset += elem_size;
            }
            if(bit_offset == 0)
            {
                elem.size = elem_size;
                bit_size = elem_size;
            }
            else
            {
                elem.size = 0;
            }
            elem.bit_offset = bit_offset+elem.bit_number -1;
            bit_offset+=elem.bit_number;
        }
        else
        {
            if(bit_size > 0)
            {
                current_offset += bit_size;
                bit_size = 0;
            }
            bit_offset = 0;

            elem.size = elem_size;
            // Adjust for arrays
            if(elem.array_size> 0)
                elem.size = elem_size * elem.array_size;

            // Align the offset to the element's size
            size_t alignment = elem_size;
            current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
            elem.offset = current_offset;

            // Update the current offset
            current_offset += elem.size;
        }
    }
    return current_offset;
}

int decode_array_size(const std::string& array_size) {
    if (array_size.empty()) {
        return -1; // Default value if m3 is empty
    }

    // Use a regex to extract the number inside the brackets
    std::regex number_in_brackets(R"(\[(\d+)\])");
    std::smatch match;
    if (std::regex_match(array_size, match, number_in_brackets)) {
        return std::stoi(match[1]); // Convert the captured number to an integer
    }

    return -1; // Default value if m3 does not match the pattern
}

void parse_header_file(const std::string& filename, std::unordered_map<std::string, Typedef>& typedefs) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::string line;
    Typedef* current_typedef = nullptr;
    uint32_t offset = 0;

    std::regex typedef_regex(R"(^typedef\s+struct\s*\{\s*)");
    std::regex element_regex(R"((\w+)\s+(\w+)(\[.*\])?(\s*:\s*(\d+))?;)");
    std::regex end_typedef_regex(R"(^\}\s*(\w+)\s*;)");

    while (std::getline(infile, line)) {

        // Check for typedef start
        if (std::regex_match(line, typedef_regex)) {
            //printf("start line [%s]\n", line.c_str());
            current_typedef = new Typedef();
            offset = 0;
            continue;
        }

        // Check for typedef end
        std::smatch match;
        if (current_typedef && std::regex_search(line, match, end_typedef_regex)) {
            //printf("end line [%s]\n", line.c_str());

            if (match.size() > 1) {
                // Safely extract typedef name
                current_typedef->name = match[1].str();
                //printf("Typedef name: [%s]\n", current_typedef->name.c_str());

                // Store the typedef in the map
                typedefs[current_typedef->name] = *current_typedef;

                // Clean up
                delete current_typedef;
                current_typedef = nullptr;
            } else {
                std::cerr << "Failed to extract typedef name from line: " << line << std::endl;
            }
            continue;
        }


        // Check for elements
        if (current_typedef && std::regex_search(line, match, element_regex)) {

            //printf("elem line [%s]\n", line.c_str());
            Element elem;
            elem.type = match[1];    // Type
            elem.name = match[2];    // Name
            elem.array_size = 0;    // zero array size
            if(match[3].matched)
            {
                auto array_size = match[3];    // Name
                int num = decode_array_size(array_size);
                //printf(" got num [%d]\n", num);
                elem.array_size = num;    // Name
            }
            if(match[4].matched)
            {
                //elem.m4 = match[4];    // Name
                //printf(" got m4  \n");
                //printf(" got m4 [%s] \n", std::string(match[4]).c_str());
            }
            elem.offset = offset;    // Current offset
            elem.bit_number = -1; //std::stoi(match[5]);
            elem.bit_offset = -1;//offset * 8;
            if (match[5].matched) {  // Bitfield details
                elem.bit_number = std::stoi(match[5]);
                //elem.bit_offset = offset * 8;
            }
            current_typedef->elements.push_back(elem);

            // Update offset
            offset += (match[5].matched) ? 2 : 4; // Increment offset based on size
        }
    }
    infile.close();
}

// Print the imported typedefs
void export_to_json(const std::unordered_map<std::string, Typedef>& typedefs, const std::string& output_filename) {

    std::ofstream outfile(output_filename);
    if (outfile.is_open()) {
        bool first_def  = true;
        outfile << "[" << std::endl;

        for (const auto& [name, tdef] : typedefs) {
            if (first_def)
                first_def  = false;
            else
                outfile << ",\n";

            outfile << "    { \"name\": \"" << tdef.name << "\", \"size\": " << tdef.size << "," << std::endl;
            outfile << "      \"elements\": [ ";
            bool first = true;
            for (const auto& elem : tdef.elements) {
                if(!first)
                    outfile << ",\n";
                else
                    outfile << "\n";

                first = false;
                outfile << "          { \"name\": \""<< elem.name <<"\""
                        << ", \"type\": \"" << elem.type<<"\"";
                        //<< ", \"offset\": " << elem.offset
                if(elem.array_size > 0)
                    outfile << ", \"array_size\": " << elem.array_size;
                if(elem.bit_number > 0)
                    outfile << ", \"bit_number\": " << elem.bit_number;
                outfile << "}";// << std::endl;
            }
            outfile << "\n      ]\n   }\n";
        }
        outfile << "]" << std::endl;

        outfile.close();
    } else {
        std::cerr << "Failed to open output file: " << output_filename << std::endl;
    }
}

// Function to load typedefs from a JSON file
void import_from_json(const std::string& filename, std::unordered_map<std::string, Typedef>& typedefs) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    json input_json;
    infile >> input_json;
    infile.close();

    for (const auto& tdef_json : input_json) {
        Typedef tdef;
        tdef.name = tdef_json["name"];
        for (const auto& elem_json : tdef_json["elements"]) {
            Element elem;
            elem.name = elem_json["name"];
            elem.type = elem_json["type"];
            elem.array_size = elem_json.value("array_size", 0); // Default to 0 if not present
            elem.bit_number = elem_json.value("bit_number", -1); // Default to -1 if not present
            elem.offset = 0;
            elem.bit_offset = -1;
            tdef.elements.push_back(elem);
        }
        typedefs[tdef.name] = tdef;
     }

    return;// typedefs;
}

void hexdump(const void* ptr, size_t size) {
    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            if (i > 0) std::cout << std::endl;
            std::cout << std::setw(4) << std::setfill('0') << i << ": ";
        }
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)byte_ptr[i] << " ";
    }
    std::cout << std::endl;
}

void show_struct(std::unordered_map<std::string, Typedef>& typedefs)
{
     for (auto& [name, tdef] : typedefs) {
        std::cout << " typedef struct {"<<std::endl;
        for (const auto& elem : tdef.elements) {
            std::cout << "    " << elem.type
                      << " " << elem.name;
            if(elem.array_size > 0)
            {
              std::cout << "[" << elem.array_size << "]";
            }
            if(elem.bit_number >= 0)
            {
              std::cout << ":" << std::dec<<(int)elem.bit_number;
            }

            std::cout 
                      << ";" 
                    //   << ", Offset: " << elem.offset
                    //   << ", Size: " << elem.size
                    //   << ", Array Size: " << elem.array_size
                    //   << ", Bitfield: " << (elem.bit_number>0 ? "Yes" : "No")
                      << std::endl;
        }

        std::cout << " } "<<name<<";"<<std::endl;

     }
}

template <typename T>
T get_value(void* base_address,
            const std::unordered_map<std::string, Typedef>& typedefs,
            Typedef& root_typedef,
            const std::string& path) {
    std::vector<std::string> components;
    parse_path(components, path);

    size_t offset = resolve_offset(typedefs, root_typedef, components,0);
    std::cout << " offset for [" << path <<"] is :"<<offset << std::endl;
    // Locate the element in the typedef
    //auto path_components = parse_path(path);
    const std::string& element_name = components.back();
    const Element* elem = nullptr;

    for (const auto& e : root_typedef.elements) {
        if (e.name == element_name) {
            elem = &e;
            break;
        }
    }

    if (!elem) {
        throw std::runtime_error("Element not found in typedef: " + element_name);
    }

    // Read the entire register
    T register_value = *reinterpret_cast<T*>(static_cast<uint8_t*>(base_address) + offset);

    // If bit-mapped, extract the value
    if (elem->bit_number > 0) {
        T mask = (1 << elem->bit_number) - 1;
        return (register_value >> elem->bit_offset) & mask;
    }

    // For complete registers, return the value
    return register_value;
    //return *reinterpret_cast<T*>(static_cast<uint8_t*>(base_address) + offset);
}

template <typename T>
void set_value(void* base_address,
               const std::unordered_map<std::string, Typedef>& typedefs,
               Typedef& root_typedef,
               const std::string& path,
               const T& value) {
    std::vector<std::string> components;
    parse_path(components, path);
    size_t offset = resolve_offset(typedefs, root_typedef, components, 0);
    
    // Locate the element in the typedef
    //auto path_components = parse_path(path);
    const std::string& element_name = components.back();
    const Element* elem = nullptr;

    for (const auto& e : root_typedef.elements) {
        if (e.name == element_name) {
            elem = &e;
            break;
        }
    }

    if (!elem) {
        throw std::runtime_error("Element not found in typedef: " + element_name);
    }

    // Read the entire register
    T* register_ptr = reinterpret_cast<T*>(static_cast<uint8_t*>(base_address) + offset);
    T register_value = *register_ptr;

    // If bit-mapped, modify only the relevant bits
    if (elem->bit_number > 0) {
        T mask = ((1 << elem->bit_number) - 1) << elem->bit_offset;
        register_value = (register_value & ~mask) | ((value << elem->bit_offset) & mask);
        *register_ptr = register_value;
    } else {
        // For complete registers, directly write the value
        *register_ptr = value;
    }

    //*reinterpret_cast<T*>(static_cast<uint8_t*>(base_address) + offset) = value;
}

Alarm alarm;

int main() {
    std::unordered_map<std::string, Typedef> typedefs;

    // Step 1: Parse the header file
    parse_header_file("example.h", typedefs);
    printf(" parse header example.h  done \n");


    // Calculate sizes
    for (auto& [name, tdef] : typedefs) {
        tdef.size = calculate_typedef_size(tdef, typedefs);
        if(0)std::cout << "xxxTypedef: " << tdef.name << ", Size: " << tdef.size << " bytes" << std::endl;
        for (const auto& elem : tdef.elements) {
            if(0)std::cout << "  Element: " << elem.name
                      << ", Type: " << elem.type
                      << ", Offset: " << elem.offset
                      << ", Size: " << elem.size
                      << ", Array Size: " << elem.array_size
                      << ", Bitfield: " << (elem.bit_number>0 ? "Yes" : "No")
                      << std::endl;
        }
    }
    // Step 2: Export typedefs to JSON

    std::cout << " create json file from imported example.h " << std::endl;
    export_to_json(typedefs, "typedefs.json");

    // Step 3: Clear typedefs and import from JSON
    typedefs.clear();
    import_from_json("typedefs2.json", typedefs);

    // Calculate sizes
    for (auto& [name, tdef] : typedefs) {
        tdef.size = calculate_typedef_size(tdef, typedefs);
        std::cout << "from typedefs2 Typedef: " << tdef.name << ", Size: " << tdef.size << " bytes" << std::endl;
        for (const auto& elem : tdef.elements) {
            std::cout << "  Element: " << elem.name
                      << ", Type: " << elem.type
                      << ", Offset: " << elem.offset
                      << ", Size: " << elem.size
                      << ", Array Size: " << elem.array_size
                      << ", Bitfield: " << (elem.bit_number>0 ? "Yes" : "No")
                      << std::endl;
        }
    }

    // Step 4: Print the imported typedefs
    for (const auto& [name, tdef] : typedefs) {
        std::cout << "{ \"name\": \"" << tdef.name << "\", \"size\": " << tdef.size << "," << std::endl;
        std::cout << "  \"elements\": [ "<< std::endl;
        bool first = true;
        for (const auto& elem : tdef.elements) {
            if(!first)
                std::cout << "           , ";
            else
                std::cout << "             ";

            first = false;
            std::cout << " { \"name\": \""<< elem.name <<"\""
                      << ", \"type\": \"" << elem.type<<"\"";
                      //<< ", \"offset\": " << elem.offset
            if(elem.array_size > 0)
                std::cout << ", \"array_size\": " << elem.array_size;
            if(elem.bit_number > 0)
                std::cout << ", \"bit_number\": " << elem.bit_number;
            std::cout << "}" << std::endl;
        }
        std::cout << "} \n";

    }

    validate_offsets_and_sizes();
    memset((char*)&alarm , sizeof(alarm), 0);

    alarm.test_set1 = 0x123;
    alarm.max_set = 1;
    alarm.min_set = 1;

    printf(" hexdump after test_set1 = 0x123, max_set = 1, min_set =1 \n");
    hexdump((const void*)&alarm, sizeof(alarm));
    printf(" hexdump after max_set2 = 1, min_set2 =1 \n");

    alarm.max_set2 = 1;
    alarm.min_set2 = 1;
    hexdump((const void*)&alarm, sizeof(alarm));

    std::vector<std::string> components;
    Typedef current_typedef;
    printf(" decode path Alarm.hist[3].max_current \n");

    std::string mypath("Alarm.hist[3].max_current");

    test_path(components, mypath);


    auto off = resolve_offset(typedefs,
                      current_typedef,
                      components,
                      0);

    std::cout << " offset found " << off << std::endl;

    std::cout << " show the typedefs as a structure (include file)" << std::endl;
    show_struct(typedefs);

     // Example memory block
    uint8_t memory[1024] = {};

    // Set a value
    //set_value<uint32_t>(memory, typedefs, alarm, "Alarm.hist[3].max_value", 12345);
    // Set a value
    set_value<uint32_t>(memory, typedefs, current_typedef, "Alarm.hist[3].max_value", 0x3456);
    set_value<uint32_t>(memory, typedefs, current_typedef, "Alarm.hist[6].max_value", 0x6543);
    set_value<uint16_t>(memory, typedefs, current_typedef, "Alarm.max_set", 1);
    // Get the value
    uint32_t value = get_value<uint32_t>(memory, typedefs, current_typedef, "Alarm.hist[3].max_value");
    uint16_t max_set = get_value<uint16_t>(memory, typedefs, current_typedef, "Alarm.max_set");
    std::cout << "used set_value(\"Alarm.hist[3].max_value\", 0x3456) " << std::hex << value << std::dec <<std::endl; // Output: 0x3456
    std::cout << "used set_value(\"Alarm.hist[6].max_value\", 0x6543) " << std::hex << value << std::dec <<std::endl; // Output: 0x3456
    std::cout << "used get_value(\"Alarm.max_set\") " << std::hex << max_set << std::dec <<std::endl; // Output: 0x3456
    //std::cout << "Alarm.max_set: " << std::hex << max_set << std::dec <<std::endl; // Output: 1

    hexdump((const void*)memory, sizeof(alarm));
    Alarm* alarmp;
    alarmp = (Alarm*)memory;
    value = alarmp->hist[3].max_value;
    std::cout << " get values directly from the structure \"Alarm * alarmp;\"" << std::endl;
    std::cout << "alarmp->hist[3].max_value: " << std::hex << value << std::dec <<std::endl; // Output: 0x3456
    max_set = alarmp->max_set;
    std::cout << "alarmp->max_set: " << std::hex << max_set << std::dec <<std::endl; // Output: 0x3456
    alarmp->max_set = 1;
    alarmp->max_set2 = 1;
    alarmp->test_set2 = 0xaa55;

    uint16_t test_set2 = get_value<uint16_t>(memory, typedefs, current_typedef, "Alarm.test_set2");
    std::cout << "alarmp->test_set2: " << std::hex << test_set2 << std::dec <<std::endl; // Output: 0xaa55

    hexdump((const void*)memory, sizeof(alarm));
    uint16_t* base = (uint16_t*)&alarmp->max_value;
    uint16_t* msp = (uint16_t*)&alarmp->test_set1;
    std::cout << "alarmp->test_set[1]: " << std::hex << msp[1] << std::dec <<std::endl; // Output: 0xaa55
    std::cout << "alarmp->test_set[2]: " << std::hex << msp[2] << std::dec <<std::endl; // Output: 0xaa55
    std::cout << "alarmp->test_set[3]: " << std::hex << msp[3] << std::dec <<std::endl; // Output: 0xaa55
   // Example: Access and modify the bit-field indirectly
  //  uint16_t* containing_value = reinterpret_cast<uint16_t*>(&alarmp->max_set);

    
    return 0;
}
