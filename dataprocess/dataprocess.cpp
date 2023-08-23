#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Define data types
enum class DataType { BOOL, INT, DOUBLE, STRING };

// Data item structure
struct DataItem {
    int itemIdx;
    int idIdx;
};


class DataProcessor {
public:
    // ... (rest of the class definition)

    // Example function to add a parameter to a value
    void AddParamToValue(const std::string& item, const std::string& id, double paramValue) {
        // Translate item and id to indices using your lookup tables
        int itemIdx = /* Translate item to index */;
        int idIdx = /* Translate id to index */;

        // Create a DataItem for the lookup
        DataItem dataItem = { itemIdx, idIdx };

        // Check if the data item exists in the setparams category
        auto it = setparamsData.find(dataItem);
        if (it != setparamsData.end()) {
            // Update the value by adding the paramValue
            double currentValue = std::stod(it->second);
            currentValue += paramValue;

            // Update the data storage
            it->second = std::to_string(currentValue);
            std::cout << "Updated value for " << item << " " << id << ": " << currentValue << std::endl;
        } else {
            std::cerr << "Data item not found in setparams category." << std::endl;
        }
    }

    // ... (rest of the class definition)
};

// Hash function for DataItem
struct DataItemHash {
    size_t operator()(const DataItem& item) const {
        return std::hash<int>()(item.itemIdx) ^ std::hash<int>()(item.idIdx);
    }
};

// Equality function for DataItem
struct DataItemEqual {
    bool operator()(const DataItem& a, const DataItem& b) const {
        return a.itemIdx == b.itemIdx && a.idIdx == b.idIdx;
    }
};

// Define data storage for setparams and setvalue categories
using DataStorage = std::unordered_map<DataItem, std::string, DataItemHash, DataItemEqual>;

// Define data storage for config objects
using ConfigStorage = std::unordered_map<DataItem, std::string, DataItemHash, DataItemEqual>;

class DataProcessor {
public:
    DataProcessor() {
        // Initialize your lookup tables here
        // ...
    }

    void Run(int port) {
        // Create and bind socket
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        listen(serverSocket, 5);

        while (true) {
            struct sockaddr_in clientAddr;
            socklen_t clientSize = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientSize);
            
            std::thread([this, clientSocket]() {
                HandleClient(clientSocket);
            }).detach();
        }
    }

private:
    void HandleClient(int clientSocket) {
        // Handle client communication here
        // ...
    }

    // Define your data processing functions here
    // ...

    // Define your config processing functions here
    // ...
};

int main() {
    DataProcessor dataProcessor;
    dataProcessor.Run(2022); // Replace with desired port

    return 0;
}
