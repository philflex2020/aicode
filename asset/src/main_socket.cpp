#include "socket_handler.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    SocketHandler socketHandler(port);

    // Start the server in a new thread
    socketHandler.run();

    return 0;
}
