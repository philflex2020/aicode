#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

struct AssetVar {
    std::string name;
    std::string uri;
    std::string type;
    union {
        int intValue;
        double doubleValue;
        char charValue;
        // Add more data types as needed
    } value;
};

struct DataItem {
    std::string name;
    size_t offset;
    std::string type;
    size_t size;
};

// Define the example_struct
struct example_struct {
    int intValue[10];
    double doubleValue[10];
    char charValue[10];
    // Add more elements as needed
};

struct DataMap {
    std::unordered_map<std::string, DataItem*> dataItems;
    char* dataArea;
    size_t dataSize;
    std::map<std::string, void* (*)(char*, void*, void*, void*)> namedFunctions;
    //std::map<std::string, DataItem*> dataItems;

    void addDataItem(char *name, int offset, char *type , int size) ;

};
struct AssetManager {
    std::unordered_map<std::string, DataMap> dataMapObjects;
    std::unordered_map<std::string, AssetVar> amap;
};

void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject);
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);
void* printIntData(char* dataArea, void* arg1, void* arg2, void* arg3);
void* incrementIntData(char* dataArea, void* arg1, void* arg2, void* arg3);

// // Function to add a named DataMap object to the map of DataMap objects in AssetManager
// void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject) {
//     assetManager.dataMapObjects[name] = dataMapObject;
// }

// // Function to map data from the asset_manager to the DataMap data area
// void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName) {
//     if (am->amap.find(itemName) != am->amap.end() && dataMap->dataItems.find(itemName) != dataMap->dataItems.end()) {
//         DataItem dataItem = dataMap->dataItems[itemName];
//         if (dataItem.type == "int") {
//             dataMap->dataArea->intValue[dataItem.offset] = am->amap[itemName].value.intValue;
//         } else if (dataItem.type == "double") {
//             dataMap->dataArea->doubleValue[dataItem.offset] = am->amap[itemName].value.doubleValue;
//         } else if (dataItem.type == "char") {
//             dataMap->dataArea->charValue[dataItem.offset] = am->amap[itemName].value.charValue;
//         }
//         // Add more cases for other data types as needed
//     }
// }

// // Function to map data from the DataMap data area to the asset_manager
// void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName) {
//     if (am->amap.find(itemName) != am->amap.end() && dataMap->dataItems.find(itemName) != dataMap->dataItems.end()) {
//         DataItem dataItem = dataMap->dataItems[itemName];
//         if (dataItem.type == "int") {
//             am->amap[itemName].value.intValue = dataMap->dataArea->intValue[dataItem.offset];
//         } else if (dataItem.type == "double") {
//             am->amap[itemName].value.doubleValue = dataMap->dataArea->doubleValue[dataItem.offset];
//         } else if (dataItem.type == "char") {
//             am->amap[itemName].value.charValue = dataMap->dataArea->charValue[dataItem.offset];
//         }
//         // Add more cases for other data types as needed
//     }
// }

// // Sample function 1: Print the integer value at the given offset in the data area
// void* printIntData(void* dataArea, void* arg1, void* arg2, void* arg3) {
//     int offset = *static_cast<int*>(arg1);
//     int value = static_cast<example_struct*>(dataArea)->intValue[offset];
//     std::cout << "Integer value at offset " << offset << ": " << value << std::endl;
//     return nullptr;
// }

// // Sample function 2: Increment the integer value at the given offset in the data area
// void* incrementIntData(void* dataArea, void* arg1, void* arg2, void* arg3) {
//     int offset = *static_cast<int*>(arg1);
//     int incrementValue = *static_cast<int*>(arg2);
//     int& value = static_cast<example_struct*>(dataArea)->intValue[offset];
//     value += incrementValue;
//     return nullptr;
// }

