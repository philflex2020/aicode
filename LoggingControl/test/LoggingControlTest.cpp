#include "LoggingControl.h"

#include <gtest/gtest.h>

TEST(LoggingControlObjectTest, LogMessageGeneration) {
    LoggingControlObject logger;

    LoggingControlObject::User user1;
    user1.name = "John";
    user1.id = 1;
    logger.addUser(user1);

    LoggingControlObject::FormatString formatString1;
    formatString1.name = "Format 1";
    formatString1.id = 100;
    logger.addFormatString(formatString1);

    std::vector<LoggingControlObject::LogMessage> loggedMessages;

    logger.setLogOutputCallback([&loggedMessages](const LoggingControlObject::LogMessage& logMessage) {
        loggedMessages.push_back(logMessage);
    });

    logger.generateLogMessage(1, 100);

    ASSERT_EQ(loggedMessages.size(), 1);
    ASSERT_EQ(loggedMessages[0].message, "Format 1");
    ASSERT_EQ(loggedMessages[0].userId, 1);
    ASSERT_EQ(loggedMessages[0].formatId, 100);
}

// Add more tests as needed

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}