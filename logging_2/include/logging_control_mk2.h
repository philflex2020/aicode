#ifndef LOGGING_CONTROL_MK2_HPP
#define LOGGING_CONTROL_MK2_HPP

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <functional>
#include <ctime>
#include <algorithm>

struct LogMessage {
    std::string userid;
    std::string log_format_id;
    std::string message;
    std::string timestamp;
};

struct UserData {
        std::string name;
    };

struct LogFormat {
        std::string format_string;
        int max_message_count;
        int current_message_count;
        int message_counter;
        int message_repeats;
        int buffer_length;
        std::deque<LogMessage> log_messages; // Fixed-length buffer for log messages
    };

class LoggingControl {
public:
    LoggingControl();
    ~LoggingControl();

    void addUser(const std::string& userid, const std::string& name);
    void addLogFormat(const std::string& log_format_id, const std::string& format_string, int max_message_count = 100);

    void setLogOutputCallback(std::function<void(const LogMessage&)> callback);

    void logMessage(const std::string& userid, const std::string& log_format_id, const std::vector<std::string>& data_values);
    void logMessage(const std::string& userid, const std::string& log_format_id, char *buffer);

    void resetMessageCountFilter(const std::string& log_format_id);
    void setMessageRepeats(int log_repeats); // Set the number of repeated messages to trigger callback
    void setBufferLength(int log_buffer_length); // Set the buffer length for log messages

    void replayLogMessages(const std::string& log_format_id);
    void replayAllLogMessages();

    // Retrieve the log format based on log_format_id
    const LogFormat* getLogFormat(const std::string& log_format_id) const;

    // Retrieve the user data based on userid
    const UserData* getUserData(const std::string& userid) const;

private:
    // struct UserData {
    //     std::string name;
    // };

    // struct LogFormat {
    //     std::string format_string;
    //     int max_message_count;
    //     int current_message_count;
    //     int message_counter;
    //     int message_repeats;
    //     int buffer_length;
    //     std::deque<LogMessage> log_messages; // Fixed-length buffer for log messages
    // };

    std::unordered_map<std::string, UserData> users_;
    std::unordered_map<std::string, LogFormat> log_formats_;
    std::function<void(const LogMessage&)> log_output_callback_;

    std::string getCurrentTimestamp() const;
};


LoggingControl::LoggingControl() {}

LoggingControl::~LoggingControl() {}

void LoggingControl::addUser(const std::string& userid, const std::string& name) {
    users_[userid] = {name};
}

void LoggingControl::addLogFormat(const std::string& log_format_id, const std::string& format_string, int max_message_count) {
    log_formats_[log_format_id] = {format_string, max_message_count, 0, 0, 10, 100, {}};
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
        char buffer[256]; // Assuming the log message won't exceed 256 characters
        std::snprintf(buffer, sizeof(buffer), log_format.format_string.c_str(), data_values[0].c_str(), data_values[1].c_str());
        log_message.message = buffer;

        log_format.log_messages.push_back(log_message);

        // Check if the message needs to be sent to the callback function based on the repeat count
        if (((int)log_format.log_messages.size() == (int)log_format.message_repeats) 
            || ((int)log_format.log_messages.size() % (int)log_format.message_repeats == 0)
            || ((int)log_format.log_messages.size() == 1)
            || ((int)log_format.log_messages.size() == 10)
            ) 
            {
            if (log_output_callback_) {
                log_output_callback_(log_message);
            }
        }

        // If the buffer length is reached, drop the oldest message
        if ((int)log_format.log_messages.size() > (int)log_format.buffer_length) {
            log_format.log_messages.pop_front();
        }
    }
}
void LoggingControl::logMessage(const std::string& userid, const std::string& log_format_id, char *buffer) {
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
        // char buffer[256]; // Assuming the log message won't exceed 256 characters
        // std::snprintf(buffer, sizeof(buffer), log_format.format_string.c_str(), data_values[0].c_str(), data_values[1].c_str());
        log_message.message = buffer;

        log_format.log_messages.push_back(log_message);

        // Check if the message needs to be sent to the callback function based on the repeat count
        if (((int)log_format.log_messages.size() == (int)log_format.message_repeats) 
            || ((int)log_format.log_messages.size() % (int)log_format.message_repeats == 0)
            || ((int)log_format.log_messages.size() == 1)
            || ((int)log_format.log_messages.size() == 10)
            ) 
            {
            if (log_output_callback_) {
                log_output_callback_(log_message);
            }
        }

        // If the buffer length is reached, drop the oldest message
        if ((int)log_format.log_messages.size() > (int)log_format.buffer_length) {
            log_format.log_messages.pop_front();
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

void LoggingControl::setMessageRepeats(int log_repeats) {
    for (auto& log_format : log_formats_) {
        log_format.second.message_repeats = log_repeats;
    }
}

void LoggingControl::setBufferLength(int log_buffer_length) {
    for (auto& log_format : log_formats_) {
        log_format.second.buffer_length = log_buffer_length;
        // If the buffer length is less than the current size, truncate the buffer
        if ((int)log_format.second.log_messages.size() > (int)log_buffer_length) {
            log_format.second.log_messages.resize(log_buffer_length);
        }
    }
}

void LoggingControl::replayLogMessages(const std::string& log_format_id) {
    auto format_it = log_formats_.find(log_format_id);
    if (format_it != log_formats_.end()) {
        for (const auto& log_message : format_it->second.log_messages) {
            if (log_output_callback_) {
                log_output_callback_(log_message);
            }
        }
    }
}

void LoggingControl::replayAllLogMessages() {
    for (const auto& log_format : log_formats_) {
        for (const auto& log_message : log_format.second.log_messages) {
            if (log_output_callback_) {
                log_output_callback_(log_message);
            }
        }
    }
}

std::string LoggingControl::getCurrentTimestamp() const {
    std::time_t now = std::time(nullptr);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buffer);
}

const LogFormat* LoggingControl::getLogFormat(const std::string& log_format_id) const {
    auto format_it = log_formats_.find(log_format_id);
    if (format_it != log_formats_.end()) {
        return &format_it->second;
    }
    return nullptr;
}

const UserData* LoggingControl::getUserData(const std::string& userid) const {
    auto user_it = users_.find(userid);
    if (user_it != users_.end()) {
        return &user_it->second;
    }
    return nullptr;
}

#endif // LOGGING_CONTROL_HPP
