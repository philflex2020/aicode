#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DataVal {
public:
    enum ValueType { INT, DOUBLE, BOOL, STRING };
    ValueType type;
    union {
        int intValue;
        double doubleValue;
        bool boolValue;
        std::string stringValue;
    };
};

class DataVar {
public:
    DataVal* value;
    std::map<int, DataVal*> params;
    std::map<int, std::function<void(DataVar&, double)>> operations;
};

    std::map<int, std::map<int, DataVar*>> items;
    std::map<std::string, int> itemDict;
    std::map<std::string, int> idDict;

// Method to set data value
void set(int item, int id, int param, double value) {
    if (items.find(item) != items.end() && items[item].find(id) != items[item].end()) {
        DataVar* dataVar = items[item][id];

        if (dataVar->params.find(param) != dataVar->params.end()) {
            DataVal* paramVal = dataVar->params[param];
            if (paramVal->type == DataVal::DOUBLE) {
                paramVal->doubleValue = value;
            } else {
                std::cerr << "Error: Value is not of double type" << std::endl;
            }
        } else {
            std::cerr << "Error: Parameter not found" << std::endl;
        }
    } else {
        std::cerr << "Error: Item or ID not found" << std::endl;
    }
}

// Method to get data value as a JSON response
json get(int item, int id, int param) {
    json response;
    if (items.find(item) != items.end() && items[item].find(id) != items[item].end()) {
        DataVar* dataVar = items[item][id];
        if (dataVar->params.find(param) != dataVar->params.end()) {
            DataVal* paramVal = dataVar->params[param];
            if (paramVal->type == DataVal::DOUBLE) {
                response["status"] = "success";
                response["value"] = paramVal->doubleValue;
            } else {
                response["status"] = "error";
                response["message"] = "Value is not of double type";
            }
        } else {
            response["status"] = "error";
            response["message"] = "Parameter not found";
        }
    } else {
        response["status"] = "error";
        response["message"] = "Item or ID not found";
    }
    return response;
}
int main() {

    // ... Code to populate itemDict and idDict ...

    // ... Code to add items and operations ...
    // Populate itemDict and idDict
    itemDict["item123"] = 0;  // Assign an ID to item "item123"
    idDict["id456"] = 0;      // Assign an ID to id "id456"

    // Add an item and id
    int itemID = itemDict["item123"];
    int idID = idDict["id456"];
    items[itemID][idID] = new DataVar();

    // Add an operation for the item and id
    items[itemID][idID]->operations[1] = [](DataVar& dv, double arg) {
        dv.params[1]->doubleValue += arg;
        std::cout << "Added " << arg << " to param1: " << dv.params[1]->doubleValue << std::endl;
    };

    // Set up socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8080);
    bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(sockfd, 5);
    int newsockfd = accept(sockfd, NULL, NULL);

    // Receive and process JSON data
    char buffer[256];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            break;
        }

        std::string jsonStr(buffer, bytesRead);
        json jsonObj = json::parse(jsonStr);

        std::string method = jsonObj["method"];
        int item = jsonObj["item"];
        int id = jsonObj["id"];
        std::string paramName = jsonObj["param"];
        json valueJson = jsonObj["value"];

        //TODO
        int param = 1;
        double value = 12.34;

        if (method == "set") {
            set(item, id, param, value);
            // ... Code for setting values ...

        } else if (method == "get") {
            json response = get(item, id, param);
            // ... Code for getting values ...
            // Send JSON response
            std::string responseStr = response.dump();
            send(newsockfd, responseStr.c_str(), responseStr.size(), 0);
        } else {
            std::cerr << "Error: Invalid method" << std::endl;
        }
    }

    // Clean up
    close(newsockfd);
    close(sockfd);

    return 0;
}
