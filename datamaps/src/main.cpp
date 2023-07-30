
#include "AssetVar.h"

// Define the example_struct
struct example_struct {
    int intValue[10];
    double doubleValue[10];
    char charValue[10];
    // Add more elements as needed
};

// Example usage
std::map<std::string, std::map<std::string, AssetVar*>> vmap;

int main() {
    AssetManager assetManager;

    // Example DataMap object
    DataMap dataMapObject;

    dataMapObject.dataSize = sizeof(example_struct); // Updated to use example_struct size
    auto mapdata = new example_struct;
    dataMapObject.dataArea = (char*)mapdata; // Example data area with type example_struct
    mapdata->intValue[0]=1234;


    // Example DataItem registration from example_struct
    dataMapObject.addDataItem((char*)"intValue", offsetof(example_struct, intValue),(char*)"int",sizeof(int));
    dataMapObject.addDataItem((char*)"doubleValue", offsetof(example_struct, doubleValue),(char*)"double",sizeof(double));
    dataMapObject.addDataItem((char*)"charValue", offsetof(example_struct, charValue),(char*)"char",sizeof(char));

    

    // Adding sample functions to the DataMap object
    dataMapObject.namedFunctions["print_int_data"] = &printIntData;
    dataMapObject.namedFunctions["increment_int_data"] = &incrementIntData;

    // Adding the DataMap object to the AssetManager
    addDataMapObject(assetManager, "example_data_map", dataMapObject);
   
    // Add the dummy AssetVar to the asset_manager's Amap
    assetManager.amap["intValue"] = setVarIVal(vmap, "/components/example_data","intValue", 42);
    assetManager.amap["dValue"]   = setVarDVal(vmap, "/components/example_data","dValue", 4.2);
    assetManager.amap["cValue"]   = setVarCVal(vmap, "/components/example_data","cValue", 'a');

    // Example mapping data from asset_manager to DataMap data area
    std::string amapName = "intValue";
    std::string mapName = "intValue";

    //to map data from assetvars into the map we use setDataTime with the amapName and the Map item name 
    std::cout << " mapdata value  :"<< mapdata->intValue[0] << std::endl;
    setDataItem(&assetManager, &dataMapObject, amapName, mapName);
    std::cout << " mapdata value has changed to :"<< mapdata->intValue[0] << std::endl;

    // Example retrieval of DataMap object from AssetManager
    std::string dataMapObjectName = "example_data_map";
    if (assetManager.dataMapObjects.find(dataMapObjectName) != assetManager.dataMapObjects.end()) {
        DataMap retrievedDataMapObject = assetManager.dataMapObjects[dataMapObjectName];

        // Example: Print the data area size
        std::cout << "DataMap Object '" << dataMapObjectName << "' Data Area Size: " << retrievedDataMapObject.dataSize << std::endl;

        // Example: Execute the 'print_int_data' function on the data area
        int offset = offsetof(example_struct, intValue);
        retrievedDataMapObject.namedFunctions["print_int_data"](retrievedDataMapObject.dataArea, &offset, nullptr, nullptr);

        // Example: Execute the 'increment_int_data' function on the data area
        int incrementValue = 5;
        retrievedDataMapObject.namedFunctions["increment_int_data"](retrievedDataMapObject.dataArea, &offset, &incrementValue, nullptr);

        // Print the updated data
        retrievedDataMapObject.namedFunctions["print_int_data"](retrievedDataMapObject.dataArea, &offset, nullptr, nullptr);

        // Example mapping data from DataMap data area back to asset_manager
        getDataItem(&assetManager, &retrievedDataMapObject, amapName, mapName);
        std::cout << "Value in asset_manager for '" << amapName << "': " << assetManager.amap[amapName]->value.intValue << std::endl;
    } else {
        std::cout << "DataMap Object not found in AssetManager." << std::endl;
    }

    // Don't forget to free the memory allocated for the data area
    //delete static_cast<example_struct*>(dataMapObject.dataArea);

    return 0;
}
