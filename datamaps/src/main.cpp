
#include "AssetVar.h"

// Define the example_struct
struct example_struct {
    int intValue[10];
    double doubleValue[10];
    char charValue[10];
    // Add more elements as needed
};

// Sample function 1: Print the integer value at the given offset in the data area
void* printIntData(uint8_t* dataArea, void* arg1, void* arg2, void* arg3) {
    bool args = false;
    if(arg2) args = true;
    if(arg3) args = true;
    if(args)
      std::cout << __func__<< "we got args"<<std::endl;
    int offset = *static_cast<int*>(arg1);
    int value = *(int*)&dataArea[offset];
    std::cout << "Integer value at offset " << offset << ": " << value << std::endl;
    return nullptr;
}

// Sample function 2: Increment the integer value at the given offset in the data area
void* incrementIntData(uint8_t* dataArea, void* arg1, void* arg2, void* arg3) {
    bool args = false;
    //if(arg2) args = true;
    if(arg3) args = true;
    if(args)
      std::cout << __func__<<"we got args"<<std::endl;
    int offset = *static_cast<int*>(arg1);
    int incrementValue = *static_cast<int*>(arg2);
    int& value = *(int*)&dataArea[offset];
    value += incrementValue;
    return nullptr;
}

// Example usage
std::map<std::string, std::map<std::string, AssetVar*>> vmap;

int main() {
    AssetManager assetManager;

    // Example DataMap object
    DataMap dataMapObject;

    dataMapObject.dataSize = sizeof(example_struct); // Updated to use example_struct size
    auto mapdata = new example_struct;
    dataM

    

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
    std::cout << " mapdata int value  :"<< mapdata->intValue[0] << std::endl;
    std::cout << " mapdata double value  :"<< mapdata->doubleValue[0] << std::endl;

    // now overwrite the values in example_map with data from the assetVars
    // see below for the block transfer way of doing this.
    setDataItem(&assetManager, &dataMapObject, amapName, mapName);
    setDataItem(&assetManager, &dataMapObject, "dValue", "doubleValue");

    
    std::cout << " mapdata value has changed to :"<< mapdata->intValue[0] << std::endl;
    std::cout << " mapdata double value  :"<< mapdata->doubleValue[0] << std::endl;

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


    

    // create a transfer block called tblock to define vars used in in our data map
    // now these bits add <amapName, mapName> pairs to t transfer block called tblock
    dataMapObject.addTransferItem("tblock", "intValue", "intValue");
    dataMapObject.addTransferItem("tblock", "dValue", "doubleValue");


    // take a look at it
    dataMapObject.showTransferItems("tblock");
    // load up our tblock items from the assetVars

    // lock 
    // sched getFromAmap request

    dataMapObject.getFromAmap("tblock", &assetManager); 


    // run code
    // TODO

    // send our tblock items to the assetVars
    dataMapObject.sendToAmap("tblock", &assetManager);


    return 0;
}


void sample_code() // thread
{
    // set up schedItem getfromamapfcn (amname, transfer_data_block, tnow+time, reptime ) which unlocks
    while (true)
    {

    // lock assetManager
    // wait sched getFromAmap request to be run 

    // this has run   dataMapObject.getFromAmap("tblock", &assetManager); 
    // and unlocks

    // run out example code in thread
    // TODO

    // send our tblock items to the assetVars
    dataMapObject.sendToAmap("tblock", &assetManager);
    // lock assetManager
    // adjust schedItem sendToAmap (amname, ttansfer_data_block, tnow+time, reptime ) 
    // sched SendToAmap request
    }
    
}
