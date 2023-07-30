
#include <queue>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include "data_object.hpp"
#include "producer.hpp"
#include "consumer.hpp"
//#include "nlohman/json.hpp"
#include "json.hpp"

using json = nlohmann::json;

std::mutex coutMutex; // Mutex for thread-safe output

// void printJSONOutput(const DataMap& dataMap) {
//     json jsonData;
//     for (const auto& entry : dataMap) {
//         json uriData;
//         for (const auto& subEntry : entry.second) {
//             json idData;
//             if (subEntry.second.isValue) {
//                 idData["value"] = subEntry.second.value;
//             } else {
//                 idData = subEntry.second.value;
//             }
//             uriData[subEntry.first] = idData;
//         }
//         jsonData[entry.first] = uriData;
//     }

//     // Print JSON data to console
//     std::lock_guard<std::mutex> lock(coutMutex);
//     std::cout << "JSON Output:\n" << jsonData.dump(4) << std::endl;
// }

int main() {
    // Create a data queue and a mutex to protect the queue
    std::queue<DataObject> dataQueue;
    std::mutex queueMutex;

    // Create producer and consumer objects
    Producer producer(dataQueue, queueMutex);
    Consumer consumer(dataQueue, queueMutex);

    // Start the consumer thread
    std::thread consumerThread(&Consumer::run, &consumer);

    // Start the producer thread
    std::thread producerThread(&Producer::run, &producer);

    // Let the producer and consumer threads run for a while
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop the producer and consumer threads
    producer.stop();
    consumer.stop();

    // Join the threads to wait for them to finish
    producerThread.join();
    consumerThread.join();

    // Get the data map from the consumer
    DataMap dataMap = consumer.getDataMap();

    // // Output the data map as JSON
    // printJSONOutput(dataMap);
    // Output the data map as JSON in simple format
    std::string simpleJsonOutput = consumer.produceSimpleJson();
    std::cout << "Simple JSON Output:\n" << simpleJsonOutput << std::endl;

    // Output the data map as JSON in nested format
    std::string nestedJsonOutput = consumer.produceNestedJson();
    std::cout << "Nested JSON Output:\n" << nestedJsonOutput << std::endl;
    

    return 0;
}
