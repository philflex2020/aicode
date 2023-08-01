#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <cstdint>

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

struct AssetManager;

struct DataMap {
    std::unordered_map<std::string, DataItem*> dataItems;
    uint8_t* dataArea;
    size_t dataSize;

    std::map<std::string, void* (*)(uint8_t*, void*, void*, void*)> namedFunctions;
    std::map<std::string, std::vector<std::pair<std::string,std::string>>> transferBlocks;
    
    void addDataItem(char *name, int offset, char *type , int size);
    void addTransferItem(std::string, std::string, std::string);
    void showTransferItems(std::string bname);
    void sendToAmap(std::string bname, AssetManager* am);
    void getFromAmap(std::string bname, AssetManager* am);

};

struct AssetManager {
    std::unordered_map<std::string, DataMap> dataMapObjects;
    std::unordered_map<std::string, AssetVar*> amap;
};

void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject);
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& amapName, const std::string& mapName);
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& amapName, const std::string& mapName);
AssetVar *setVarIVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , int value );
AssetVar *setVarDVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , double value );
AssetVar *setVarCVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , char value );

