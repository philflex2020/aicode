# GCOM Smart Logger Specification

## Overview

The GCOM Smart Logger is a logging component that aims to provide detailed logging information for system errors without overwhelming the logging system with almost identical log messages. It enables components to issue log messages with an origin ID, log ID, and message string. The Smart Logger uses the log ID to retrieve a format string, which serves as the basis for the log message. The user then fills out the format string with specific data to create the final log message, which is passed back into the logging system.

The Smart Logger operates with the following key features:

1. Log Message Categorization: Each log message is associated with an origin ID and log ID, allowing for efficient categorization and retrieval of log messages.

2. Repeated Message Handling: The Smart Logger avoids excessive logging of identical messages. It outputs a system log the first time it receives a specific log message with the same origin ID and log ID. Subsequent identical log messages are logged every tenth occurrence and then only once every ten seconds. These repetition thresholds are configurable.

3. Message Retention: The Smart Logger retains up to 1024 repeated messages, which can be recovered based on the origin ID and log ID. This retention capability ensures that important log information is not lost due to repetition.

4. Clearing Messages: The logging system can be set to clear all messages associated with specific origin and/or log IDs, allowing for selective message deletion.

5. User Registration: The Smart Logger supports user registration by allowing the mapping of user names to user IDs. This feature is useful for identifying users responsible for specific log messages.

6. Log Format String Registration: The logging system supports log format string registration, where format strings can be associated with specific log IDs. This association simplifies log message composition and ensures consistency in log formatting.

## Operation

The GCOM Smart Logger operates as follows:

1. Logging a Message:
   - A component generates a log message with an origin ID and log ID.
   - The Smart Logger retrieves the log format string associated with the log ID (if registered).
   - The component fills out the format string with specific data to create the log message.
   - The filled-out log message is passed back to the Smart Logger for logging.
   - The Smart Logger checks if it has previously logged the same log message with the same origin ID and log ID.
   - If it is a new log message, it is immediately logged as a system log.
   - If the log message has occurred before, the Smart Logger checks the repetition count:
     - If the count is less than the configured threshold (10), the message is not logged.
     - If the count reaches the threshold, the message is logged, and the count is reset to 0.
   - Subsequent identical log messages are logged every tenth occurrence and then only once every ten seconds.

2. Message Retention:
   - The Smart Logger retains up to 1024 repeated messages (configurable).
   - The retained messages can be recovered based on the origin ID and log ID.

3. Clearing Messages:
   - The logging system can be set to clear all messages associated with specific origin and/or log IDs.

4. User Registration:
   - The Smart Logger supports user registration by allowing the mapping of user names to user IDs.
   - This feature facilitates identification of users responsible for specific log messages.

5. Log Format String Registration:
   - The logging system supports log format string registration, where format strings can be associated with specific log IDs.
   - This association simplifies log message composition and ensures consistency in log formatting.

## Conclusion

The GCOM Smart Logger provides an efficient and effective logging mechanism for the GCOM system. It categorizes log messages based on origin and log IDs, avoids excessive logging of repeated messages, retains important log data, and offers user registration and log format string registration capabilities. By implementing the Smart Logger as part of the GCOM project, the system will have a powerful tool for detailed logging while maintaining log system efficiency.