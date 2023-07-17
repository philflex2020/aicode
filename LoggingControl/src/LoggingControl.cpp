#include "LoggingControl.h"

int main() {
    LoggingControlObject logger;

    LoggingControlObject::User user1;
    user1.name = "John";
    user1.id = 1;
    logger.addUser(user1);

    LoggingControlObject::FormatString formatString1;
    formatString1.name = "Format 1";
    formatString1.id = 100;
    logger.addFormatString(formatString1);

    logger.setLogOutputCallback([](const LoggingControlObject::LogMessage& logMessage) {
        std::cout << "Log Message: " << logMessage.message << ", User ID: " << logMessage.userId << ", Format ID: " << logMessage.formatId << std::endl;
    });

    logger.generateLogMessage(1, 100);

    return 0;
}