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

void DataMap::addTransferItem(std::string bname, std::string amap, std::string dmap)
{
    std::pair<std::string,std::string>item = std::make_pair(amap,dmap);
    transferBlocks[bname].push_back(item);

}

void DataMap::showTransferItems(std::string bname)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Transfer Block name [" << bname << "] "<< std::endl;
        for (auto xx : transferBlocks[bname])
        {
            std::cout << "  [" << xx.first << "]  <=> ["<<xx.second<<"]"<< std::endl;
        }
    }

}

void DataMap::getFromAmap(std::string bname, AssetManager* am)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Get mystruct from Amap using Transfer Block name [" << bname << "] "<< std::endl;
        for (auto xx : transferBlocks[bname])
        {
            setDataItem(am, this, xx.first, xx.second);
        }
    }
}

void DataMap::sendToAmap(std::string bname, AssetManager* am)
{
    if (transferBlocks.find(bname) != transferBlocks.end())
    {
        std::cout << " Send mystruct to Amap using Transfer Block name [" << bname << "] "<< std::endl;
        for (auto xx : transferBlocks[bname])
        {
            getDataItem(am, this, xx.first, xx.second);
        }
    }

}

void DataMap::printDataMap(){

    std::cout << "\nPrinting full DataMap" << std::endl;

    std::cout << "  -printing all dataItems: " << std::endl;
    for (const auto& map_item : dataItems) {
        std::cout << map_item.first 
        << ": offset:" << (int)map_item.second->offset 
        << " type:" << map_item.second->type 
        << " size:" << map_item.second->size;
        if (map_item.second->type == "char")
        {
            std::cout << "\n  char value: " << (char)this->dataArea[map_item.second->offset];
        }
        else if (map_item.second->type == "int")
        {
            std::cout << "\n  int value: " << (int)this->dataArea[map_item.second->offset];
        }
        else if (map_item.second->type == "double")
        {
            std::cout << "\n  double value: " << (double)this->dataArea[map_item.second->offset];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "  -printing all namedFunctions: " << std::endl;
    for (const auto& map_item : namedFunctions) {
        std::cout << map_item.first << std::endl;
    }
    std::cout << std::endl;

    std::cout << "  -printing all transferBlocks: " << std::endl;
    for (const auto& map_item : transferBlocks) {
        std::cout << "map.string:" << map_item.first << " ";

        for (const auto& next_level : map_item.second) {
            
            std::cout << "map.vector.string1: " << next_level.first << " map.vector.string2: " << next_level.second << std::endl;

        }
    }
    std::cout << "\n" << std::endl;
}

void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject);
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& itemName);



// Function to add a named DataMap object to the map of DataMap objects in AssetManager
void addDataMapObject(AssetManager& assetManager, const std::string& name, DataMap dataMapObject) {
    assetManager.dataMapObjects[name] = dataMapObject;
}

// Function to map data from the asset_manager to the DataMap data area
void setDataItem(AssetManager* am, DataMap* dataMap, const std::string& amapName, const std::string& mapName) {
    if (am->amap.find(amapName) != am->amap.end() && dataMap->dataItems.find(mapName) != dataMap->dataItems.end()) {
        DataItem *dataItem = dataMap->dataItems[mapName];
        if (dataItem->type == "int") {
            *(int *)(&dataMap->dataArea[dataItem->offset]) = am->amap[amapName]->value.intValue;
        } else if (dataItem->type == "double") {
            *(double *)(&dataMap->dataArea[dataItem->offset]) = am->amap[amapName]->value.doubleValue;
        } else if (dataItem->type == "char") {
            *(char*)(&dataMap->dataArea[dataItem->offset]) = am->amap[amapName]->value.charValue;
        }
        // Add more cases for other data types as needed
    }
}

// Function to map data from the DataMap data area to the asset_manager
void getDataItem(AssetManager* am, DataMap* dataMap, const std::string& amapName, const std::string& mapName) {
    if (am->amap.find(amapName) != am->amap.end() && dataMap->dataItems.find(mapName) != dataMap->dataItems.end()) {
        DataItem *dataItem = dataMap->dataItems[mapName];
        if (dataItem->type == "int") {
            am->amap[amapName]->value.intValue = *(int*)(&dataMap->dataArea[dataItem->offset]);
        } else if (dataItem->type == "double") {
            am->amap[amapName]->value.doubleValue = *(double*)(&dataMap->dataArea[dataItem->offset]);
        } else if (dataItem->type == "char") {
            am->amap[amapName]->value.charValue = *(char *)(&dataMap->dataArea[dataItem->offset]);
        }
        // Add more cases for other data types as needed
    }
}



AssetVar *setAVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , std::string type)
{
    AssetVar * av = nullptr;
    if (vmap.find(uri) == vmap.end() )
    {
        std::map<std::string, AssetVar*> ci;
        vmap[uri] = ci;
    }
    auto cx = vmap[uri];
    if (cx.find(id) == cx.end())
    {
        cx[id] = new AssetVar;
        cx[id]->name = id;
        cx[id]->uri = uri;
        cx[id]->type = type;
    }
    av = cx[id];
    return av;
}

AssetVar *setVarIVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , int value )
{
    AssetVar *av = setAVal(vmap, uri, id , "int");
    av->value.intValue = value;
    return av;
}
AssetVar *setVarDVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , double value )
{
    AssetVar *av = setAVal(vmap, uri, id , "double");
    av->value.doubleValue = value;
    return av;
}
AssetVar *setVarCVal(std::map<std::string, std::map<std::string, AssetVar*>> &vmap , std::string uri, std::string id , char value )
{
    AssetVar *av = setAVal(vmap, uri, id , "char");
    av->value.charValue = value;
    return av;
}