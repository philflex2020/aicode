#include "smObj.h"
#include "server.cpp" // Include your server code

int main() {
    // Initialize root object
    auto root = std::make_shared<smObj>("root");

    // Add some initial attributes and children
    root->attributes["temperature"] = 25;
    root->attributes["pressure"] = 1013;

    auto child = std::make_shared<smObj>("child");
    child->attributes["humidity"] = 60;
    root->children.push_back(child);

    // Start the socket server
    std::cout << "Starting server for object: " << root->name << std::endl;
    start_socket_server(root);

    return 0;
}
