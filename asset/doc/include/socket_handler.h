#ifndef SOCKET_HANDLER_H
#define SOCKET_HANDLER_H

#include "asset_var_map.h"
#include <map>
#include <string>

class SocketHandler {
public:
    SocketHandler(int port, AssetVarMap& assetVarMapRef);
    ~SocketHandler();

    void run();

private:
    int serverSocket;
    int portNumber;
    AssetVarMap& assetVarMapRef;

    // ... (other private member functions)
};

#endif // SOCKET_HANDLER_H