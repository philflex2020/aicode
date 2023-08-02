markdown
Copy code
# Logging Control System Documentation

The Logging Control System is a C++ library that provides a centralized logging mechanism with the ability to manage log messages, log formats, and users. It allows users to generate log messages using predefined formats and sends them to a callback function. The system also supports filtering and storage of log messages based on log IDs.

## Table of Contents

- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building the Library](#building-the-library)
- [Usage](#usage)
  - [Include the Library](#include-the-library)
  - [Initializing the Logging Control](#initializing-the-logging-control)
  - [Adding Users and Log Formats](#adding-users-and-log-formats)
  - [Generating Log Messages](#generating-log-messages)
  - [Retrieving Stored Log Messages](#retrieving-stored-log-messages)
- [Callback Function](#callback-function)
- [Examples](#examples)
  - [Example 1: Basic Usage](#example-1-basic-usage)
  - [Example 2: Adding Users and Formats](#example-2-adding-users-and-formats)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)

## Getting Started

### Prerequisites

- C++11 compatible compiler
- CMake (>= 3.1) for building the library
- Google Test for testing (optional)

### Building the Library

1. Clone the repository:

```bash
git clone https://github.com/your-username/logging-control.git
cd logging-control
Create a build directory and generate the Makefile:
bash
Copy code
mkdir build
cd build
cmake ..
Build the library and examples:
bash
Copy code
make
Usage
Include the Library
To use the logging control system in your C++ project, include the logging_control.hpp header file:

cpp
Copy code
#include "logging_control.hpp"
Initializing the Logging Control
Before using the logging control system, you need to create an instance of the LoggingControl class and set the log output callback function.

cpp
Copy code
#include "logging_control.hpp"

// Define the log output callback function
void logOutputCallback(const std::string& logMessage) {
    // Implement your custom log output logic here
    // For example, send logMessage to a log file, console, etc.
}

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Set the log output callback function
    loggingControl.setLogOutputCallback(logOutputCallback);

    // Rest of your application logic...
}
Adding Users and Log Formats
You can add users and log formats dynamically to the logging control system during runtime. Each user and log format is identified by a unique ID.

cpp
Copy code
#include "logging_control.hpp"

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Add a user with ID 1 and name "User1"
    loggingControl.addUser(1, "User1");

    // Add a log format with ID 100 and name "Format1" and a maximum log message count of 10
    loggingControl.addLogFormat(100, "Format1", 10);

    // Rest of your application logic...
}
Generating Log Messages
To generate a log message, get the log format by its ID and then use it to create the log message.

cpp
Copy code
#include "logging_control.hpp"

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Add a user with ID 1 and name "User1"
    loggingControl.addUser(1, "User1");

    // Add a log format with ID 100 and name "Format1" and a maximum log message count of 10
    loggingControl.addLogFormat(100, "Format1", 10);

    // Generate a log message with user ID 1 and log format ID 100
    std::string logMessage = "This is a log message with data";
    loggingControl.generateLogMessage(1, 100, logMessage);

    // Rest of your application logic...
}
Retrieving Stored Log Messages
You can retrieve stored log messages for a specific log format ID using the getStoredLogMessages() function. The messages will be sent to the log output callback function.

cpp
Copy code
#include "logging_control.hpp"

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Set the log output callback function
    loggingControl.setLogOutputCallback(logOutputCallback);

    // Add a user with ID 1 and name "User1"
    loggingControl.addUser(1, "User1");

    // Add a log format with ID 100 and name "Format1" and a maximum log message count of 10
    loggingControl.addLogFormat(100, "Format1", 10);

    // Generate a log message with user ID 1 and log format ID 100
    std::string logMessage = "This is a log message with data";
    loggingControl.generateLogMessage(1, 100, logMessage);

    // Retrieve stored log messages for log format ID 100
    loggingControl.getStoredLogMessages(100);

    // Rest of your application logic...
    return 0;
}
Callback Function
The log output callback function is responsible for handling the log messages generated by the logging control system. It receives the log message as a string and can be customized to perform specific actions such as writing to a log file, printing to the console, etc.

cpp
Copy code
#include "logging_control.hpp"

// Log output callback function
void logOutputCallback(const std::string& logMessage) {
    // Implement your custom log output logic here
    // For example, send logMessage to a log file, console, etc.
}
Examples
Example 1: Basic Usage
This example demonstrates basic usage of the logging control system to generate and output log messages.

cpp
Copy code
#include "logging_control.hpp"

// Log output callback function
void logOutputCallback(const std::string& logMessage) {
    // Implement your custom log output logic here
    // For example, send logMessage to a log file, console, etc.
}

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Set the log output callback function
    loggingControl.setLogOutputCallback(logOutputCallback);

    // Add a user with ID 1 and name "User1"
    loggingControl.addUser(1, "User1");

    // Add a log format with ID 100 and name "Format1" and a maximum log message count of 10
    loggingControl.addLogFormat(100, "Format1", 10);

    // Generate a log message with user ID 1 and log format ID 100
    std::string logMessage = "This is a log message with data";
    loggingControl.generateLogMessage(1, 100, logMessage);

    // Retrieve stored log messages for log format ID 100
    loggingControl.getStoredLogMessages(100);

    // Rest of your application logic...
    return 0;
}
Example 2: Adding Users and Formats
This example demonstrates how to add multiple users and log formats dynamically.

cpp
Copy code
#include "logging_control.hpp"

// Log output callback function
void logOutputCallback(const std::string& logMessage) {
    // Implement your custom log output logic here
    // For example, send logMessage to a log file, console, etc.
}

int main() {
    // Create an instance of the LoggingControl class
    LoggingControl loggingControl;

    // Set the log output callback function
    loggingControl.setLogOutputCallback(logOutputCallback);

    // Add users and log formats
    loggingControl.addUser(1, "User1");
    loggingControl.addUser(2, "User2");

    loggingControl.addLogFormat(100, "Format1", 10);
    loggingControl.addLogFormat(200, "Format2", 5);

    // Generate log messages with user ID 1 and log format ID 100
    std::string logMessage1 = "This is a log message with data from User1";
    loggingControl.generateLogMessage(1, 100, logMessage1);

    std::string logMessage2 = "This is another log message with data from User1";
    loggingControl.generateLogMessage(1, 100, logMessage2);

    // Generate log messages with user ID 2 and log format ID 200
    std::string logMessage3 = "This is a log message with data from User2";
    loggingControl.generateLogMessage(2, 200, logMessage3);

    // Retrieve stored log messages for log format ID 100
    loggingControl.getStoredLogMessages(100);

    // Rest of your application logic...
    return 0;
}
Testing
The library includes unit tests using Google Test. To run the tests, use the following command:

bash
Copy code
make test
Contributing
Contributions to the Logging Control System library are welcome! If you find any issues or have suggestions for improvements, please feel free to open an issue or submit a pull request.

License
This project is licensed under the MIT License - see the LICENSE file for details.

Copy code



