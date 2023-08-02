//src/main.cpp:
//cpp
//Copy code
#include "asset.h"

int main() {
    // Create an instance of AssetVar
    AssetVar assetVar;

    // Set the value of AssetVar
    assetVar.setVal(42);

    // Get the value from AssetVar
    int intValue = assetVar.getVal<int>();
    std::cout << "Value: " << intValue << std::endl;

    // Set a parameter value in AssetVar
    assetVar.setParam("param1", 3.14);

    // Get the parameter value from AssetVar
    double paramValue = assetVar.getParam("param1").getVal<double>();
    std::cout << "Param Value: " << paramValue << std::endl;

    return 0;
}