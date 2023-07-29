// logging_control_test.cpp

#include "gtest/gtest.h"
#include "logging_control_mk2.h"

//g++ -std=c++11 -Iinclude  src/logging_main_test.cpp -o test/logging_control_test.o -lgtest -lpthread

class LoggingControlTest : public ::testing::Test {
protected:
    LoggingControl logger;

    virtual void SetUp() override {
        logger.addUser("user1", "User One");
        logger.addUser("user2", "User Two");

        logger.addLogFormat("format1", "User: %s, Message: %s");
        logger.addLogFormat("format2", "User: %s, Log Count: %s");
    }
};

TEST_F(LoggingControlTest, LogMessageAndReplay) {
    std::string expected_output1 = "User: user1, Message: format1";
    std::string expected_output2 = "User: user2, Log Count: format2";
    
    // Set up the log output callback to capture the log message
    std::string actual_output1, actual_output2;
    logger.setLogOutputCallback([&actual_output1, &actual_output2](const LogMessage& log_message) {
        if (log_message.log_format_id == "format1") {
            actual_output1 = log_message.message;
        } else if (log_message.log_format_id == "format2") {
            actual_output2 = log_message.message;
        }
    });

    std::vector<std::string> data_values1 = {"user1", "format1", "Hello, World!"};
    logger.logMessage("user1", "format1", data_values1);

    std::vector<std::string> data_values2 = {"user2", "format2", "20"};
    logger.logMessage("user2", "format2", data_values2);

    std::cout << " expected [" << expected_output1 << "] actual ["<< actual_output1 << "]"<< std::endl;
    EXPECT_EQ(actual_output1, expected_output1);
    EXPECT_EQ(actual_output2, expected_output2);

    // Replay log messages and verify
    std::ostringstream log_messages_stream;
    logger.setLogOutputCallback([&log_messages_stream](const LogMessage& log_message) {
        log_messages_stream << log_message.message << "\n";
    });

    logger.replayLogMessages("format1");
    logger.replayLogMessages("format2");

    std::string replayed_log_messages = log_messages_stream.str();
    std::string expected_replay = expected_output1 + "\n" + expected_output2 + "\n";
    EXPECT_EQ(replayed_log_messages, expected_replay);
}

TEST_F(LoggingControlTest, GetLogFormatAndUserData) {
    const LogFormat* format1 = logger.getLogFormat("format1");
    const LogFormat* format3 = logger.getLogFormat("format3");

    const UserData* user1 = logger.getUserData("user1");
    const UserData* user3 = logger.getUserData("user3");

    EXPECT_NE(format1, nullptr);
    EXPECT_EQ(format3, nullptr);

    EXPECT_NE(user1, nullptr);
    EXPECT_EQ(user3, nullptr);

    EXPECT_EQ(format1->format_string, "User: %s, Message: %s");
    EXPECT_EQ(user1->name, "User One");
}

TEST_F(LoggingControlTest, MessageCountFilterAndBufferLength) {
    // Set the message repeats and buffer length
    logger.setMessageRepeats(3);
    logger.setBufferLength(5);

    std::ostringstream log_messages_stream;
    logger.setLogOutputCallback([&log_messages_stream](const LogMessage& log_message) {
        log_messages_stream << log_message.message << "\n";
    });

    std::vector<std::string> data_values = {"user1", "format1", "Hello, World!"};

    // Log 10 messages (should only receive 3 messages)
    for (int i = 0; i < 10; i++) {
        logger.logMessage("user1", "format1", data_values);
    }

    std::string received_messages = log_messages_stream.str();
    std::string expected_output = "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n"
                                  "User: user1, Message: format1\n";
    EXPECT_EQ(received_messages, expected_output);

    // Log more messages to fill the buffer (total 15 messages, but only 5 should be stored)
    for (int i = 0; i < 15; i++) {
        logger.logMessage("user1", "format1", data_values);
    }

    // Verify the buffer length
    EXPECT_EQ(logger.getLogFormat("format1")->log_messages.size(), 5);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}