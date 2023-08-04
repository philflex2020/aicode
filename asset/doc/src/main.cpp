#include "socket_handler.h"
#include "asset_var_map.h"
#include <iostream>

// Global assetVarMap instance
AssetVarMap assetVarMap;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // Create some AssetVar objects and store them in the assetVarMap
    assetVarMap.setAssetVar("/my/json/uri", "myassetid", new AssetVar("this is the assetval"));
    assetVarMap.getAssetVar("/my/json/uri", "myassetid")->setParam("param1", 1);
    assetVarMap.getAssetVar("/my/json/uri", "myassetid")->setParam("param2", false);
    assetVarMap.getAssetVar("/my/json/uri", "myassetid")->setParam("param3", 3.45);
    assetVarMap.getAssetVar("/my/json/uri", "myassetid")->setParam("param4", "this is param4");

    SocketHandler socketHandler(port, assetVarMap);

    // Start the server in a new thread
    socketHandler.run();

    return 0;
}