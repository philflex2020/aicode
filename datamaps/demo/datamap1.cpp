Sure, I can help you with that. Here's a C++ function that creates a `DataMap` object based on a C++ structure:

```cpp
#include <iostream>
#include <map>
#include <string>
#include <typeinfo>

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

// Helper function to create the DataMap
template <typename T>
DataMap CreateDataMap() {
    DataMap dataMap;
    size_t offset = 0;

    // Iterate through the structure fields
    std::string structName = typeid(T).name();
    for (int i = 0; i < structName.length(); ++i) {
        if (structName[i] == '#') {
            offset += 1; // Skip the '#'
            continue;
        }

        AddDataMapEntry<T>(dataMap, std::string(1, structName[i]), offset);
        offset += sizeof(typename T::value_type);
    }

    return dataMap;
}

// Define a sample structure
struct EmbeddedStruct {
    int x;
    double y;
};

struct SampleStruct {
    int a;
    double b;
    char c;
    EmbeddedStruct embedded;
};

int main() {
    // Create the DataMap for SampleStruct
    DataMap sampleStructDataMap = CreateDataMap<SampleStruct>();

    // Print the DataMap
    for (const auto& entry : sampleStructDataMap) {
        std::cout << "Name: " << entry.second.name
                  << ", Type: " << entry.second.type->name()
                  << ", Offset: " << entry.second.offset << std::endl;
    }

    return 0;
}
```

This code defines a `DataMap` structure to hold information about each field in the structure. The `CreateDataMap` function generates the `DataMap` by iterating through the structure fields and calculating offsets. The '#' character is used to denote embedded structures.

Feel free to modify the structure definitions and adapt the code to your specific needs.