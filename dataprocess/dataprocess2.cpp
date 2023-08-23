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
    DataVal *value;
    std::map<int, DataVal*> params;
    std::map<int, std::function<void(DataVar&, double)>> operations;
};

int main() {
    std::map<int, std::map<int, DataVar*>> items;
    std::map<std::string, int> itemDict;
    std::map<std::string, int> idDict;

    // Add item and id names to dictionaries
    int itemID = 0;
    int idID = 0;
    itemDict["item123"] = itemID++;
    idDict["id456"] = idID++;

    // Add an item and id
    items[itemDict["item123"]][idDict["id456"]] = new DataVar();

    // Add an operation for the item and id
    items[itemDict["item123"]][idDict["id456"]]->operations[1] = [](DataVar& dv, double arg) {
        dv.params[1]->doubleValue += arg;
        std::cout << "Added " << arg << " to param1: " << dv.params[1]->doubleValue << std::endl;
    };

    // Create a DataVar object
    DataVar* dataVar = items[itemDict["item123"]][idDict["id456"]];

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
        recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // Parse JSON
        std::string jsonStr(buffer, bytesRead);
        json request = json::parse(jsonStr);

        std::string method = request["method"];
        int item = request["item"];
        int id = request["id"];
        std::string paramName = jsonObj["param"];
        json valueJson = jsonObj["value"];

        if (method == "set") {
            if (items.find(item) != items.end() && items[item].find(id) != items[item].end()) {
                DataVar* dataVar = items[item][id];

                if (dataVar->params.find(paramName) != dataVar->params.end()) {
                    DataVal* paramVal = dataVar->params[paramName];

                    if (paramVal->type == DataVal::DOUBLE) {
                        if (valueJson.is_number()) {
                            paramVal->doubleValue = valueJson.get<double>();
                        } else {
                            std::cerr << "Error: Value is not of double type" << std::endl;
                        }
                    } else if (paramVal->type == DataVal::INT) {
                        if (valueJson.is_number_integer()) {
                            paramVal->intValue = valueJson.get<int>();
                        } else {
                            std::cerr << "Error: Value is not of int type" << std::endl;
                        }
                    } else if (paramVal->type == DataVal::BOOL) {
                        if (valueJson.is_boolean()) {
                            paramVal->boolValue = valueJson.get<bool>();
                        } else {
                            std::cerr << "Error: Value is not of bool type" << std::endl;
                        }
                    } else if (paramVal->type == DataVal::STRING) {
                        if (valueJson.is_string()) {
                            paramVal->stringValue = valueJson.get<std::string>();
                        } else {
                            std::cerr << "Error: Value is not of string type" << std::endl;
                        }
                    } else {
                        std::cerr << "Error: Unknown value type" << std::endl;
                    }
                } else {
                    std::cerr << "Error: Parameter not found" << std::endl;
                }
            } else {
                std::cerr << "Error: Item or ID not found" << std::endl;
            }
        // Add more cases for other methods (get, show)
        } else if (method == "get") {
            json response;
            if (items.find(item) != items.end() && items[item].find(id) != items[item].end()) {
                if (items[item][id]->params.find("value") != items[item][id]->params.end()) {
                    DataVal* val = items[item][id]->params["value"];
                    if (val->type == DataVal::DOUBLE) {
                        response["status"] = "success";
                        response["value"] = val->doubleValue;
                    } else {
                        response["status"] = "error";
                        response["message"] = "Value is not of double type";
                    }
                } else {
                    response["status"] = "error";
                    response["message"] = "Parameter 'value' not found";
                }
            } else {
                response["status"] = "error";
                response["message"] = "Item or ID not found";
            }

            // Send JSON response
            std::string responseStr = response.dump();
            send(newsockfd, responseStr.c_str(), responseStr.size(), 0);
        } else {
            std::cerr << "Error: Invalid method" << std::endl;
        }

    }

        
    // Clean up
    delete dataVar;
    close(newsockfd);
    close(sockfd);

    return 0;
}


// import socket
// import json

// # Create a socket connection
// server_address = ('localhost', 12345)
// sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
// sock.connect(server_address)

// def send_json_message(message):
//     sock.sendall(json.dumps(message).encode())

// # Set a parameter value
// set_message = {
//     "method": "set",
//     "item": 1,
//     "id": 2,
//     "param": "param_name",
//     "value": 42
// }
// send_json_message(set_message)

// # Get a parameter value
// get_message = {
//     "method": "get",
//     "item": 1,
//     "id": 2,
//     "param": "param_name"
// }
// send_json_message(get_message)

// # Receive the response from the server
// response = sock.recv(1024)
// print("Received:", response.decode())

// # Close the socket connection
// sock.close()
