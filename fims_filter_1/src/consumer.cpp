#include "consumer.hpp"
#include <iostream>
#include <sstream>
#include "json.hpp"

using json = nlohmann::json;

// Consumer::Consumer(std::queue<DataObject>& dataQueue, std::mutex& queueMutex)
//     : dataQueue(dataQueue), queueMutex(queueMutex) {}

void Consumer::run() {
    running = true;
    while (running) {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!dataQueue.empty()) {
            DataObject data = dataQueue.front();
            dataQueue.pop();
            // Check if a command is received
            if (data.uri == "command") {
                if (data.id == "produce_simple_json") {
                    command = CommandType::ProduceSimpleJson;
                } else if (data.id == "produce_nested_json") {
                    command = CommandType::ProduceNestedJson;
                } else if (data.id == "clear_data") {
                    command = CommandType::ClearData;
                } else {
                    std::cout << "Invalid command received: " << data.id << std::endl;
                }
            } else {
                // Process data and update dataMap accordingly
                dataMap[data.uri][data.id] = data.value;
            }
        }

        // Check the command and take appropriate action
        if (command == CommandType::ProduceSimpleJson) {
            std::string simpleJsonOutput = produceSimpleJson();
            std::cout << "Simple JSON Output:\n" << simpleJsonOutput << std::endl;
        } else if (command == CommandType::ProduceNestedJson) {
            std::string nestedJsonOutput = produceNestedJson();
            std::cout << "Nested JSON Output:\n" << nestedJsonOutput << std::endl;
        } else if (command == CommandType::ClearData) {
            clearDataMap();
            std::cout << "DataMap cleared." << std::endl;
        }
        
        // Reset the command
        command = CommandType::None;
    }
}

void Consumer::stop() {
    running = false;
}

DataMap Consumer::getDataMap() {
    return dataMap;
}

std::string Consumer::produceSimpleJson() {
    json jsonData;
    for (const auto& entry : dataMap) {
        for (const auto& subEntry : entry.second) {
            jsonData[entry.first][subEntry.first] = subEntry.second;
        }
    }
    return jsonData.dump(4);
}

std::string Consumer::produceNestedJson() {
    json jsonData;
    for (const auto& entry : dataMap) {
        for (const auto& subEntry : entry.second) {
            jsonData[entry.first][subEntry.first]["value"] = subEntry.second;
        }
    }
    return jsonData.dump(4);
}

void Consumer::clearDataMap() {
    dataMap.clear();
}
std::string Consumer::getJsonOutput() const {
    return jsonOutput;
}

void Consumer::processCommand(const DataObject& command) {
    // Process different commands here
    if (command.id == "produce_simple_json") {
        // Produce simple JSON output
        jsonOutput = json(dataMap).dump();
    } else if (command.id == "produce_nested_json") {
        // Produce nested JSON output
        jsonOutput = json({{command.uri, dataMap[command.uri]}}).dump();
    } else if (command.id == "clear_data") {
        // Clear the data map
        dataMap.clear();
    }
}

void Consumer::processLogData(const DataObject& dataObject) {
    // Process log data and update data map
    dataMap[dataObject.uri][dataObject.id] = dataObject.value;
}
