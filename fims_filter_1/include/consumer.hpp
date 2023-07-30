#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include <queue>
#include <mutex>
#include <map>
#include "data_object.hpp"
#include "json.hpp"

using json = nlohmann::json;

enum class CommandType {
    None,
    ProduceSimpleJson,
    ProduceNestedJson,
    ClearData
};

class Consumer {
public:
    void run();
    void stop();
    DataMap getDataMap(); // New member function to retrieve DataMap
    std::string getJsonOutput() const;


private:
    std::queue<DataObject>& dataQueue; // Reference to the data queue
    std::mutex& queueMutex; // Reference to the mutex for data queue access
    DataMap dataMap;
    bool running;
    CommandType command;
    // Helper functions to produce simple and nested JSON formats
    std::string jsonOutput;

    void processCommand(const DataObject& command);
    void processLogData(const DataObject& dataObject);


public:
    //std::map<std::string, std::map<std::string, std::string>> dataMap; // Map to store data
    Consumer(std::queue<DataObject>& dataQueue, std::mutex& queueMutex) 
        : dataQueue(dataQueue), queueMutex(queueMutex) {}

    std::string produceSimpleJson();
    std::string produceNestedJson();
    void clearDataMap();


};

#endif // CONSUMER_HPP