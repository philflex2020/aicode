
#include "AssetVar.h"

void DataMap::addDataItem(char *name, int offset, char *type , int size) 
{
    auto dataItem = new DataItem;
    dataItem->name = name;
    dataItem->offset = offset;
    dataItem->type = type;
    dataItem->size = size;
    dataItems[std::string(name)] = dataItem;
}


void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject);
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);
void* printIntData(char* dataArea, void* arg1, void* arg2, void* arg3);
void* incrementIntData(char* dataArea, void* arg1, void* arg2, void* arg3);

// Function to add a named DataMap object to the map of DataMap objects in AssetManager
void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject) {
    assetManager.dataMapObjects[name] = dataMapObject;
}

// Function to map data from the asset_manager to the DataMap data area
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName) {
    if (am->amap.find(itemName) != am->amap.end() && dataMap->dataItems.find(itemName) != dataMap->dataItems.end()) {
        DataItem *dataItem = dataMap->dataItems[itemName];
        if (dataItem->type == "int") {
            *(int *)(&dataMap->dataArea[dataItem->offset]) = am->amap[itemName].value.intValue;
        } else if (dataItem->type == "double") {
            *(double *)(&dataMap->dataArea[dataItem->offset]) = am->amap[itemName].value.doubleValue;
        } else if (dataItem->type == "char") {
            *(char*)(&dataMap->dataArea[dataItem->offset]) = am->amap[itemName].value.charValue;
        }
        // Add more cases for other data types as needed
    }
}

// Function to map data from the DataMap data area to the asset_manager
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName) {
    if (am->amap.find(itemName) != am->amap.end() && dataMap->dataItems.find(itemName) != dataMap->dataItems.end()) {
        DataItem *dataItem = dataMap->dataItems[itemName];
        if (dataItem->type == "int") {
            am->amap[itemName].value.intValue = *(int*)(&dataMap->dataArea[dataItem->offset]);
        } else if (dataItem->type == "double") {
            am->amap[itemName].value.doubleValue = *(double*)(&dataMap->dataArea[dataItem->offset]);
        } else if (dataItem->type == "char") {
            am->amap[itemName].value.charValue = *(char *)(&dataMap->dataArea[dataItem->offset]);
        }
        // Add more cases for other data types as needed
    }
}

// Sample function 1: Print the integer value at the given offset in the data area
void* printIntData(char* dataArea, void* arg1, void* arg2, void* arg3) {
    bool args = false;
    if(arg2) args = true;
    if(arg3) args = true;
    if(args)
      std::cout << "we got args"<<std::endl;
    int offset = *static_cast<int*>(arg1);
    int value = *(int*)&dataArea[offset];
    std::cout << "Integer value at offset " << offset << ": " << value << std::endl;
    return nullptr;
}

// Sample function 2: Increment the integer value at the given offset in the data area
void* incrementIntData(char* dataArea, void* arg1, void* arg2, void* arg3) {
    bool args = false;
    if(arg2) args = true;
    if(arg3) args = true;
    if(args)
      std::cout << "we got args"<<std::endl;
    int offset = *static_cast<int*>(arg1);
    int incrementValue = *static_cast<int*>(arg2);
    int& value = *(int*)&dataArea[offset];
    value += incrementValue;
    return nullptr;
}
