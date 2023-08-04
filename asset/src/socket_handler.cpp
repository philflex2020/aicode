#include "socket_handler.h"
#include "asset.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include <chrono>

using json = nlohmann::json;

SocketHandler::SocketHandler(int port) : serverSocket(-1), portNumber(port) {}

SocketHandler::~SocketHandler() {
    if (serverSocket >= 0) {
        close(serverSocket);
    }
}

void SocketHandler::run() {
    // Create a TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    // Bind the socket to a specific address and port
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(portNumber);
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        close(serverSocket);
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Error listening for connections" << std::endl;
        close(serverSocket);
        return;
    }

    while (true) {
        // Accept a new connection
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Add the new client socket to the vector
        clientSockets.push_back(clientSocket);

        // Start a new thread to handle the client connection
        std::thread clientThread(&SocketHandler::handleClient, this, clientSocket);

        // Detach the thread to allow it to run in the background
        clientThread.detach();
    }
}

void SocketHandler::handleClient(int clientSocket) {
    while (true) {
        // Read the message from the client
        char buffer[4096];
        int bytesRead = read(clientSocket, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            // Client disconnected or error occurred
            close(clientSocket);
            return;
        }

        // Parse the message as JSON
        std::string message_str(buffer, bytesRead);
        json message = json::parse(message_str);

        // Check the method and handle the request
        if (message.contains("method")) {
            std::string method = message["method"];
            if (method == "get" && message.contains("uri") && message.contains("id")) {
                std::string uri = message["uri"];
                std::string assetId = message["id"];

                std::string response = handleGet(uri, assetId);

                // Send the response back to the client
                write(clientSocket, response.c_str(), response.size());
            } else if (method == "set" && message.contains("body")) {
                // Handle "set" requests
                // ... (same set code as before)
            } else if (method == "pub" && message.contains("uri") && message.contains("interval")) {
                std::string uri = message["uri"];
                int interval = message["interval"];

                pubSubMap[uri] = std::to_string(clientSocket);

                // Start a new thread for publishing at the specified interval
                std::thread pubThread(&SocketHandler::publishData, this, uri, interval, clientSocket);
                pubThread.detach();
            } else {
                std::cerr << "Invalid message: Unknown method or missing required parameters" << std::endl;
            }
        }
    }
}

std::string SocketHandler::handleGet(const std::string& uri, const std::string& assetId) {
    // Retrieve the asset variable based on the provided URI and assetId
    // For demonstration purposes, I'll create a simple map to store the asset variables.
    std::map<std::string, AssetVar> assetVarMap;

    // Sample asset variables for demonstration
    assetVarMap["/my/json/uri"]["myassetid"].setVal("this is the assetval");
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param1", 1);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param2", false);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param3", 3.45);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param4", "this is param4");

    json response;

    if (assetVarMap.find(uri) != assetVarMap.end() && assetVarMap[uri].getParam<std::string>("id") == assetId) {
        // Asset variable found for the given URI and assetId
        AssetVar assetVar = assetVarMap[uri];
        response["uri"] = uri;
        response["id"] = assetId;
        response["value"] = assetVar.getVal<std::string>();

        // Add parameters to the response
        for (const auto& param : assetVar.params) {
            const std::string& paramName = param.first;
            const AssetVal& paramValue = param.second;

            // For simplicity, we assume all parameters are of type int or string.
            if (paramValue.getType() == AssetVal::Type::Int) {
                response[paramName] = paramValue.getVal<int>();
            } else if (paramValue.getType() == AssetVal::Type::CharPtr) {
                response[paramName] = paramValue.getVal<const char*>();
            }
        }
    } else {
        // Asset variable not found
        response["error"] = "Asset variable not found for the given URI and assetId.";
    }

    // Convert the JSON response to a string and return it
    return response.dump();
}
void SocketHandler::publishData(const std::string& uri, int interval, int clientSocket) {
    // Retrieve the asset variable based on the provided URI (similar to handleGet)
    // For demonstration purposes, I'll create a simple map to store the asset variables.
    std::map<std::string, AssetVar> assetVarMap;

    // Sample asset variables for demonstration
    assetVarMap["/my/json/uri"]["myassetid"].setVal("this is the assetval");
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param1", 1);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param2", false);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param3", 3.45);
    assetVarMap["/my/json/uri"]["myassetid"].setParam("param4", "this is param4");

    while (true) {
        if (pubSubMap.find(uri) != pubSubMap.end() && pubSubMap[uri] == std::to_string(clientSocket)) {
            // Asset variable found for the given URI and clientSocket
            AssetVar assetVar = assetVarMap[uri];

            json response;
            response["uri"] = uri;
            response["id"] = assetVar.getParam<std::string>("id");
            response["value"] = assetVar.getVal<std::string>();

            // Add parameters to the response
            for (const auto& param : assetVar.params) {
                const std::string& paramName = param.first;
                const AssetVal& paramValue = param.second;

                // For simplicity, we assume all parameters are of type int or string.
                if (paramValue.getType() == AssetVal::Type::Int) {
                    response[paramName] = paramValue.getVal<int>();
                } else if (paramValue.getType() == AssetVal::Type::CharPtr) {
                    response[paramName] = paramValue.getVal<const char*>();
                }
            }

            // Convert the JSON response to a string and send it to the client
            std::string response_str = response.dump();
            write(clientSocket, response_str.c_str(), response_str.size());
        }

        // Sleep for the specified interval before publishing the next data
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
}

// std::string SocketHandler::handleGet(const std::string& uri, const std::string& assetId) {
//     // ... (handleGet implementation as before)
// }

// void SocketHandler::publishData(const std::string& uri, int interval, int clientSocket) {
//     // ... (publishData implementation as before)
// }
