#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <typeinfo>
#include <sstream>
#include <vector>
#include <algorithm>

// Define the DataMapEntry structure
struct DataMapEntry {
    std::string name;
    const std::type_info* type;
    size_t offset;

    DataMapEntry(const std::string& n, const std::type_info* t, size_t o)
        : name(n), type(t), offset(o) {}
};

// Define the DataMap structure to hold DataMapEntries
using DataMap = std::map<std::string, DataMapEntry>;

// Helper function to add DataMapEntry to the DataMap
template <typename T>
void AddDataMapEntry(DataMap& dataMap, const std::string& name, size_t offset) {
    dataMap.emplace(name, DataMapEntry(name, &typeid(T), offset));
}

// Helper function to create the DataMap for a structure defined in a file
DataMap CreateDataMapFromFile(const std::string& filePath, std::map<std::string, DataMap>& embeddedStructures) {
    DataMap dataMap;
    size_t offset = 0;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return dataMap;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string fieldType, fieldName;
        ss >> fieldType >> fieldName;

        if (!fieldType.empty() && !fieldName.empty()) {
            if (fieldName.back() == ';') {
                fieldName.pop_back();
            }

            if (fieldType == "struct") {
                std::string structName;
                ss >> structName;
                if (structName.back() == ';') {
                    structName.pop_back();
                }

                if (structName.front() == '#') {
                    structName.erase(0, 1);
                    if (embeddedStructures.find(structName) != embeddedStructures.end()) {
                        // Add the embedded structure's DataMap to the current DataMap
                        const DataMap& embeddedMap = embeddedStructures[structName];
                        for (const auto& entry : embeddedMap) {
                            dataMap.emplace(fieldName + "." + entry.second.name, DataMapEntry(entry.second.name, entry.second.type, offset + entry.second.offset));
                        }
                        offset += embeddedMap.size() * sizeof(int); // Offset for the whole embedded structure
                    }
                }
            } else if (fieldType == "int") {
                AddDataMapEntry<int>(dataMap, fieldName, offset);
                offset += sizeof(int);
            } else if (fieldType == "double") {
                AddDataMapEntry<double>(dataMap, fieldName, offset);
                offset += sizeof(double);
            } else if (fieldType == "char*") {
                AddDataMapEntry<char*>(dataMap, fieldName, offset);
                offset += sizeof(char*);
            }
            // Add more types as needed
        }
    }

    return dataMap;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <structure_definition_file> [output_file]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = (argc >= 3) ? argv[2] : "";

    // Create a map to hold embedded structure DataMaps
    std::map<std::string, DataMap> embeddedStructures;

    // Create the DataMap for the main structure defined in the file
    DataMap mainStructDataMap = CreateDataMapFromFile(inputFile, embeddedStructures);

    // Print the DataMap to console
    for (const auto& entry : mainStructDataMap) {
        std::cout << "Name: " << entry.second.name
                  << ", Type: " << entry.second.type->name()
                  << ", Offset: " << entry.second.offset << std::endl;
    }

    // Write the DataMap to the output file if specified
    if (!outputFile.empty()) {
        std::ofstream output(outputFile);
        if (output.is_open()) {
            for (const auto& entry : mainStructDataMap) {
                output << "Name: " << entry.second.name
                       << ", Type: " << entry.second.type->name()
                       << ", Offset: " << entry.second.offset << std::endl;
            }
            output.close();
            std::cout << "DataMap written to " << outputFile << std::endl;
        } else {
            std::cerr << "Failed to open output file: " << outputFile << std::endl;
        }
    }

    return 0;
}




// #include <iostream>
// #include <fstream>
// #include <map>
// #include <string>
// #include <typeinfo>
// #include <sstream>
// #include <vector>

// // Define the DataMapEntry structure
// struct DataMapEntry {
//     std::string name;
//     const std::type_info* type;
//     size_t offset;

//     DataMapEntry(const std::string& n, const std::type_info* t, size_t o)
//         : name(n), type(t), offset(o) {}
// };

// // Define the DataMap structure to hold DataMapEntries
// using DataMap = std::map<std::string, DataMapEntry>;

// // Helper function to add DataMapEntry to the DataMap
// template <typename T>
// void AddDataMapEntry(DataMap& dataMap, const std::string& name, size_t offset) {
//     dataMap.emplace(name, DataMapEntry(name, &typeid(T), offset));
// }

// // Helper function to create the DataMap for a structure defined in a file
// DataMap CreateDataMapFromFile(const std::string& filePath) {
//     DataMap dataMap;
//     size_t offset = 0;

//     std::ifstream file(filePath);
//     if (!file.is_open()) {
//         std::cerr << "Failed to open file: " << filePath << std::endl;
//         return dataMap;
//     }

//     std::string line;
//     while (std::getline(file, line)) {
//         std::istringstream ss(line);
//         std::string fieldType, fieldName;
//         ss >> fieldType >> fieldName;

//         if (!fieldType.empty() && !fieldName.empty()) {
//             if (fieldName.back() == ';') {
//                 fieldName.pop_back();
//             }

//             if (fieldType == "int") {
//                 AddDataMapEntry<int>(dataMap, fieldName, offset);
//                 offset += sizeof(int);
//             } else if (fieldType == "double") {
//                 AddDataMapEntry<double>(dataMap, fieldName, offset);
//                 offset += sizeof(double);
//             } else if (fieldType == "char*") {
//                 AddDataMapEntry<char*>(dataMap, fieldName, offset);
//                 offset += sizeof(char*);
//             }
//             // Add more types as needed
//         }
//     }

//     return dataMap;
// }

// int main(int argc, char* argv[]) {
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <structure_definition_file> [output_file]" << std::endl;
//         return 1;
//     }

//     std::string inputFile = argv[1];
//     std::string outputFile = (argc >= 3) ? argv[2] : "";

//     // Create the DataMap for the structure defined in the file
//     DataMap structDataMap = CreateDataMapFromFile(inputFile);

//     // Print the DataMap to console
//     for (const auto& entry : structDataMap) {
//         std::cout << "Name: " << entry.second.name
//                   << ", Type: " << entry.second.type->name()
//                   << ", Offset: " << entry.second.offset << std::endl;
//     }

//     // Write the DataMap to the output file if specified
//     if (!outputFile.empty()) {
//         std::ofstream output(outputFile);
//         if (output.is_open()) {
//             for (const auto& entry : structDataMap) {
//                 output << "Name: " << entry.second.name
//                        << ", Type: " << entry.second.type->name()
//                        << ", Offset: " << entry.second.offset << std::endl;
//             }
//             output.close();
//             std::cout << "DataMap written to " << outputFile << std::endl;
//         } else {
//             std::cerr << "Failed to open output file: " << outputFile << std::endl;
//         }
//     }

//     return 0;
// }

