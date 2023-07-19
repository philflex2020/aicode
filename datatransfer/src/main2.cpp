// main.cpp

#include <iostream>
#include "DataTransSmart.h"

int main() {
    DataTransferManager manager;

    // Add data objects with their respective transfer modes
    manager.addDataObject("data1", TransferMode::PERIODIC_WITH_INPUT, 1000);
    manager.addDataObject("data2", TransferMode::IMMEDIATE);
    manager.addDataObject("data3", TransferMode::PERIODIC_WITHOUT_INPUT, 2000);

    // Set input callbacks for each data object
    manager.setInputCallback("data1", [](int data, std::chrono::system_clock::time_point timestamp) {
        std::cout << "Data1 received: " << data << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << "ms\n";
    });

    manager.setInputCallback("data2", [](int data, std::chrono::system_clock::time_point timestamp) {
        std::cout << "Data2 received: " << data << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << "ms\n";
    });

    manager.setInputCallback("data3", [](int data, std::chrono::system_clock::time_point timestamp) {
        std::cout << "Data3 received: " << data << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << "ms\n";
    });

    // Start data transfer for all data objects
    manager.start();

    // Simulate data production for data1, data2, and data3
    for (int i = 1; i <= 10; ++i) {
        manager.addData("data1", i);
        manager.addData("data2", i * 2);
        manager.addData("data3", i * 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Stop data transfer
    manager.stop();

    return 0;
}
