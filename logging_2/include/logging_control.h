#ifndef LOGGING_CONTROL_HPP
#define LOGGING_CONTROL_HPP

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct LogMessage {
    std::string userid;
    std::string log_format_id;
    std::string message;
    std::string timestamp;
};

class LoggingControl {
public:
    LoggingControl();
    ~LoggingControl();

    void addUser(const std::string& userid, const std::string& name);
    void addLogFormat(const std::string& log_format_id, const std::string& format_string, int max_message_count = 100);

    void setLogOutputCallback(std::function<void(const LogMessage&)> callback);

    void logMessage(const std::string& userid, const std::string& log_format_id, const std::vector<std::string>& data_values);

    void resetMessageCountFilter(const std::string& log_format_id);

    void recallLogMessages(const std::string& log_format_id, std::function<void(const LogMessage&)> callback);

private:
    struct UserData {
        std::string name;
    };

    struct LogFormat {
        std::string format_string;
        int max_message_count;
        int current_message_count;
        int message_counter;
    };

    std::unordered_map<std::string, UserData> users_;
    std::unordered_map<std::string, LogFormat> log_formats_;
    std::function<void(const LogMessage&)> log_output_callback_;
    std::vector<LogMessage> log_messages_;

    std::string getCurrentTimestamp() const;
};

#include <ctime>

LoggingControl::LoggingControl() {}

LoggingControl::~LoggingControl() {}

void LoggingControl::addUser(const std::string& userid, const std::string& name) {
    users_[userid] = {name};
}

void LoggingControl::addLogFormat(const std::string& log_format_id, const std::string& format_string, int max_message_count) {
    log_formats_[log_format_id] = {format_string, max_message_count, 0, 0};
}

void LoggingControl::setLogOutputCallback(std::function<void(const LogMessage&)> callback) {
    log_output_callback_ = callback;
}

void LoggingControl::logMessage(const std::string& userid, const std::string& log_format_id, const std::vector<std::string>& data_values) {
    auto user_it = users_.find(userid);
    auto format_it = log_formats_.find(log_format_id);
    if (user_it != users_.end() && format_it != log_formats_.end()) {
        LogFormat& log_format = format_it->second;
        if (log_format.current_message_count >= log_format.max_message_count) {
            log_format.current_message_count = 0;
            log_format.message_counter++;
        }
        log_format.current_message_count++;

        LogMessage log_message;
        log_message.userid = userid;
        log_message.log_format_id = log_format_id;
        log_message.timestamp = getCurrentTimestamp();

        // Create the log message string using the format string and data values
        // Add implementation logic here based on the provided format string and data values
        // ...

        log_messages_.push_back(log_message);
        if (log_output_callback_) {
            log_output_callback_(log_message);
        }
    }
}

void LoggingControl::resetMessageCountFilter(const std::string& log_format_id) {
    auto format_it = log_formats_.find(log_format_id);
    if (format_it != log_formats_.end()) {
        format_it->second.current_message_count = 0;
        format_it->second.message_counter = 0;
    }
}

void LoggingControl::recallLogMessages(const std::string& log_format_id, std::function<void(const LogMessage&)> callback) {
    for (const auto& log_message : log_messages_) {
        if (log_message.log_format_id == log_format_id) {
            callback(log_message);
        }
    }
}

std::string LoggingControl::getCurrentTimestamp() const {
    // Add implementation to get the current timestamp as a string
    // For example, using std::chrono and std::put_time
    // ...

    // Dummy implementation (returns empty string)
    return "";
}
#endif // LOGGING_CONTROL_HPP
Now, you can simply include the logging_control.hpp header in your application, and it will automatically include the implementation of the LoggingControl class methods from the logging_control.cpp file.

Remember to compile and link both the logging_control.cpp and your application code to use the LoggingControl class functionalities correctly.





User

what about the gtest code
ChatGPT
To write gtest code for testing the LoggingControl class, you need to create a separate source file containing the test cases. Here's an example of how you can write gtest code to test the functionalities of the LoggingControl class:

First, install and set up Google Test (gtest) on your system.

Create a new source file named logging_control_test.cpp for the gtest code.

cpp
Copy code
// logging_control_test.cpp

#include <gtest/gtest.h>
#include "logging_control.hpp"

// Define a fixture for the LoggingControl class
class LoggingControlTest : public ::testing::Test {
protected:
    LoggingControl logger;

    void SetUp() override {
        // Setup any common resources for the test cases
        logger.addUser("user1", "User One");
        logger.addLogFormat("format1", "User: %s, Message: %s");
    }

    void TearDown() override {
        // Clean up any resources used in the test cases
    }
};

TEST_F(LoggingControlTest, TestLogMessage) {
    std::vector<std::string> data_values = {"user1", "format1"};
    logger.logMessage("user1", "format1", data_values);

    // Perform assertions to check if the log message was generated and stored correctly
    // You can add more detailed assertions based on your implementation
    // For example:
    // ASSERT_EQ(logger.getLogMessageCount("format1"), 1);
    // ASSERT_EQ(logger.getLogMessages().back().userid, "user1");
    // ASSERT_EQ(logger.getLogMessages().back().log_format_id, "format1");
}

TEST_F(LoggingControlTest, TestRecallLogMessages) {
    // Test the recallLogMessages function
    // ...

    // ASSERT_XXX() assertions to check the recall functionality
}

// Add more test cases to cover other functionalities of the LoggingControl class

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

====
Compile the logging_control_test.cpp source file with the gtest and LoggingControl implementation.
bash
Copy code
g++ logging_control_test.cpp logging_control.cpp -lgtest -pthread -o logging_control_test
Run the compiled test binary.
bash
Copy code
./logging_control_test
This will execute the gtest cases defined in logging_control_test.cpp and provide you with test results.

Please note that this is just a basic example of gtest code for testing the LoggingControl class. 
You may need to write more detailed and comprehensive test cases based on the implementation and requirements of your LoggingControl class. Additionally, 
you can use gtest assertions like ASSERT_EQ, ASSERT_TRUE, ASSERT_FALSE, etc., 
to perform various checks and validate the correctness of the class functionalities in your test cases.