#include <iostream>
#include "asset.h"

int main() {
    AssetVar assetVar;

    // Set params
    assetVar.setParam("param1", 42);
    assetVar.setParam("param2", 3.14);
    assetVar.setParam("param3", (const char *)"hello");
    assetVar.setParam("param4", true);

    // // Get params
    int param1Value = assetVar.getParam<int>("param1");
    double param2Value = assetVar.getParam<double>("param2");
    std::string param3Value = assetVar.getParam<const char *>("param3");
    bool param4Value = assetVar.getParam<bool>("param4");

    std::cout << "param1: " << param1Value << std::endl;
    std::cout << "param2: " << param2Value << std::endl;
    std::cout << "param3: " << param3Value << std::endl;
    std::cout << "param4: " << param4Value<< std::endl;

    return 0;
}

// //src/main.cpp:
// //cpp
// //Copy code
// #include "asset.h"

// int main() {
//     // Create an instance of AssetVar
//     AssetVar assetVar;

//     // Set the value of AssetVar
//     assetVar.setVal(42);

//     // Get the value from AssetVar
//     int intValue = assetVar.getVal<int>();
//     std::cout << "Value: " << intValue << std::endl;

//     // Set a parameter value in AssetVar
//     assetVar.setParam("param1", 3.14);

//     // Get the parameter value from AssetVar
//     double paramValue = assetVar.getParam("param1").getVal<double>();
//     std::cout << "Param Value: " << paramValue << std::endl;

//     return 0;
// }