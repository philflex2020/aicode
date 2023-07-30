
#include "AssetVar.h"

// Example usage
int main() {
    AssetManager assetManager;

    // Example DataMap object
    DataMap dataMapObject;

    dataMapObject.dataSize = sizeof(example_struct); // Updated to use example_struct size
    dataMapObject.dataArea = (char*) new example_struct; // Example data area with type example_struct

    // Example DataItem registration from example_struct
    dataMapObject.addDataItem((char*)"intValue", offsetof(example_struct, intValue),(char*)"int",sizeof(int));
    dataMapObject.addDataItem((char*)"doubleValue", offsetof(example_struct, doubleValue),(char*)"double",sizeof(double));
    dataMapObject.addDataItem((char*)"charValue", offsetof(example_struct, charValue),(char*)"char",sizeof(char));

    

    // Adding sample functions to the DataMap object
    dataMapObject.namedFunctions["print_int_data"] = &printIntData;
    dataMapObject.namedFunctions["increment_int_data"] = &incrementIntData;

    // Adding the DataMap object to the AssetManager
    addDataMapObject(assetManager, "example_data_map", dataMapObject);

    // Create a dummy AssetVar to simulate data in the asset_manager
    AssetVar dummyAssetVar;
    dummyAssetVar.name = "intValue";
    dummyAssetVar.uri = "/components/example_data/intValue";
    dummyAssetVar.type = "int";
    dummyAssetVar.value.intValue = 42; // Set initial value to 42

    // Add the dummy AssetVar to the asset_manager's Amap
    assetManager.amap[dummyAssetVar.name] = dummyAssetVar;

    // Example mapping data from asset_manager to DataMap data area
    std::string itemName = "intValue";
    setDataItem(&assetManager, &dataMapObject, itemName);

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
        getDataItem(&assetManager, &retrievedDataMapObject, itemName);
        std::cout << "Value in asset_manager for '" << itemName << "': " << assetManager.amap[itemName].value.intValue << std::endl;
    } else {
        std::cout << "DataMap Object not found in AssetManager." << std::endl;
    }

    // Don't forget to free the memory allocated for the data area
    //delete static_cast<example_struct*>(dataMapObject.dataArea);

    return 0;
}
