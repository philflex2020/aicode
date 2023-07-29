// main.cpp
//g++ --std=c++17 -Iinclude -o build/lm src/logging_main.cpp

#include <iostream>
#include "logging_control_mk2.h"

int main() {
    LoggingControl logger;

    logger.addUser("user1", "User One");
    logger.addUser("user2", "User Two");

    logger.addLogFormat("format1", "User: %s, Message: %s");
    logger.addLogFormat("format2", "User: %s, Log Count: %s");
    logger.addLogFormat("format1++", "User: %s, Message: %s Data %d");

    logger.setLogOutputCallback([](const LogMessage& log_message) {
        std::cout << log_message.timestamp << " - " << log_message.message << std::endl;
    });

    const LogFormat *lf = logger.getLogFormat("format1++");
    std::vector<std::string> data_values1 = {"user1", "format1 1", "Hello, World! 1"};

    if (!lf)
    {
        std::cout << "Boo" << std::endl;
    }
    else
    {
     // Create the log message string using the format string and data values
        char buffer[256]; // Assuming the log message won't exceed 256 characters
        std::snprintf(buffer, sizeof(buffer), lf->format_string.c_str(), data_values1[0].c_str(), data_values1[1].c_str(), 1234);
        logger.logMessage("user1", "format1++", buffer);
    }
    logger.logMessage("user1", "format1", data_values1);

    data_values1 = {"user1", "format1 2", "Hello, World! 2 "};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 3", "Hello, World! 3"};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 4", "Hello, World! 4 "};
    logger.logMessage("user1", "format1", data_values1);

    std::vector<std::string> data_values2 = {"user2", "format2", "20"};
    logger.logMessage("user2", "format2", data_values2);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 5", "Hello, World! 5"};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 6", "Hello, World! 6 "};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 7", "Hello, World! 7"};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 8", "Hello, World! 8 "};
    logger.logMessage("user1", "format1", data_values1);

    std::vector<std::string> data_values3 = {"user1", "format1", "Another log message"};
    logger.logMessage("user1", "format1", data_values3);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 9", "Hello, World! 9"};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 10", "Hello, World! 10 "};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 11", "Hello, World! 11"};
    logger.logMessage("user1", "format1", data_values1);

    //std::vector<std::string> 
    data_values1 = {"user1", "format1 12", "Hello, World! 12"};
    logger.logMessage("user1", "format1", data_values1);

    std::vector<std::string> data_values4 = {"user2", "format2", "30"};
    logger.logMessage("user2", "format2", data_values4);

    std::cout << "=== Replay Log Messages for format1 ===" << std::endl;
    logger.replayLogMessages("format1");

    std::cout << "=== Replay All Log Messages ===" << std::endl;
    logger.replayAllLogMessages();

    return 0;
}