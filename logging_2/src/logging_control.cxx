// main.cpp

#include <iostream>
#include "logging_control.hpp"

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl logger;

    // Add users and log formats
    logger.addUser("user1", "User One");
    logger.addUser("user2", "User Two");
    logger.addLogFormat("format1", "User: %s, Message: %s");
    logger.addLogFormat("format2", "User: %s, Log Count: %d");

    // Set the log output callback function
    logger.setLogOutputCallback([](const LogMessage& log_message) {
        std::cout << log_message.timestamp << ": " << log_message.message << std::endl;
    });

    // Log some messages
    std::vector<std::string> data_values1 = {"user1", "format1", "Hello, World!"};
    logger.logMessage("user1", "format1", data_values1);

    std::vector<std::string> data_values2 = {"user2", "format2", "20"};
    logger.logMessage("user2", "format2", data_values2);

    std::vector<std::string> data_values3 = {"user1", "format1", "Another log message"};
    logger.logMessage("user1", "format1", data_values3);

    std::vector<std::string> data_values4 = {"user2", "format2", "30"};
    logger.logMessage("user2", "format2", data_values4);

    // Recall log messages for format1
    logger.recallLogMessages("format1", [](const LogMessage& log_message) {
        std::cout << "Recalled: " << log_message.message << std::endl;
    });

    return 0;
}