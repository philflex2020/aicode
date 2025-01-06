#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

//g++ -o defs defs.cpp 

using json = nlohmann::json;

// Element definition
struct Element {
    std::string name;
    std::string type;
    uint32_t element_size = 0; 

    uint32_t bit_size = 0; // For bitfields
    uint32_t num = 1;      // Default to 1, for arrays
    uint32_t offset = 0;   // Optional offset
};

// Typedef definition
struct Typedef {
    std::string typedef_name;
    uint32_t typedef_size = 0; 
    std::vector<Element> elements;
};

// Shared memory structure definition
struct SharedMemory {
    std::string sm_name;
    std::unordered_map<std::string, Typedef> typedefs;
};

SharedMemory shared_mem;


uint32_t get_element_size(const std::string& type) {
    static const std::unordered_map<std::string, uint32_t> type_sizes = {
        {"bool", 1},
        {"double", sizeof(double)},
        {"float", sizeof(float)},
        {"int8_t", 1},
        {"int16_t", 2},
        {"int32_t", 4},
        {"uint8_t", 1},
        {"uint16_t", 2},
        {"uint32_t", 4},
        {"bitfield16", 2},
        {"bitfield32", 4}
    };

    auto it = type_sizes.find(type);
    return (it != type_sizes.end()) ? it->second : 0; // Default to 0 if type is unknown
}

Typedef* get_typedef(std::string& sm_name, std::string &typedef_name, int size = 0)
{
    // Create or get the typedef
    auto& typedef_entry = shared_mem.typedefs[typedef_name];
    typedef_entry.typedef_name = typedef_name;
    typedef_entry.typedef_size = size;
    return &typedef_entry;
}

uint32_t calculate_offset(const Typedef& typedef_entry, uint32_t element_size) {
    uint32_t current_offset = typedef_entry.typedef_size;
    uint32_t alignment = element_size;

    // Align to the nearest boundary
    if (alignment > 0) {
        current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
    }
    return current_offset;
}

// Function to parse a single JSON object
void parse_json_object(const json& obj, SharedMemory& shared_mem) {
    std::string sm_name = obj["sm_name"];
    std::string typedef_name = obj["typedef"];
    std::string element_name = obj["name"];
    std::string element_type = obj["type"];

    // Create or get the typedef
    // auto& typedef_entry = shared_mem.typedefs[typedef_name];
    // typedef_entry.typedef_name = typedef_name;

    Typedef* typedef_entry = get_typedef(sm_name, typedef_name);

    // Create the element
    Element elem;
    elem.name = element_name;
    elem.type = element_type;

    // Optional fields
    if (obj.contains("bit_size")) {
        elem.bit_size = obj["bit_size"];
    }
    if (obj.contains("num")) {
        elem.num = obj["num"];
    }
    if (obj.contains("offset")) {
        elem.offset = obj["offset"];
    }

    // Add the element to the typedef
    typedef_entry->elements.push_back(elem);
}

// Function to read and process a file line by line
void process_file(const std::string& filename, SharedMemory& shared_mem) {
    std::ifstream infile(filename);

    if (!infile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue; // Skip empty lines

        try {
            json obj = json::parse(line);
            parse_json_object(obj, shared_mem);
        } catch (const json::exception& e) {
            std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        }
    }

    infile.close();
}




int main() {
    const std::string filename = "data.json"; // Replace with your file path
    std::string sm_name;
    std::string mytype;
    sm_name="rack";
    mytype="unit32_t";
    get_typedef(sm_name, mytype, 4);

    mytype="unit16_t";
    get_typedef(sm_name, mytype, 2);

    mytype="unit8_t";
    get_typedef(sm_name, mytype, 1);

    process_file(filename, shared_mem);

    // Print parsed typedefs for verification
    for (const auto& [typedef_name, typedef_entry] : shared_mem.typedefs) {
        std::cout << "Typedef: " << typedef_entry.typedef_name <<" size: "<< typedef_entry.typedef_size<< std::endl;
        for (const auto& elem : typedef_entry.elements) {
            std::cout << "  Element Name: " << elem.name 
                      << ", Type: " << elem.type
                      << ", Bit Size: " << elem.bit_size
                      << ", Num: " << elem.num
                      << ", Offset: " << elem.offset
                      << std::endl;
        }
    }

    return 0;
}