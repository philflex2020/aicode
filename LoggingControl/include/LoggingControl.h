#pragma once 

#include <iostream>
#include <vector>
#include <map>
#include <functional>

class LoggingControlObject {
public:
    struct User {
        std::string name;
        int id;
    };

    struct FormatString {
        std::string name;
        int id;
    };

    struct LogMessage {
        std::string message;
        int userId;
        int formatId;
    };

    typedef std::function<void(const LogMessage&)> LogOutputCallback;

    LoggingControlObject() {
        lastUserMessageCounts_ = std::map<int, int>();
        logOutputCallback_ = nullptr;
    }

    void addUser(const User& user) {
        users_.push_back(user);
    }

    void addFormatString(const FormatString& formatString) {
        formatStrings_.push_back(formatString);
    }

    void setLogOutputCallback(const LogOutputCallback& callback) {
        logOutputCallback_ = callback;
    }

    void generateLogMessage(int userId, int formatId) {
        User* user = getUserById(userId);
        FormatString* formatString = getFormatStringById(formatId);

        if (user && formatString) {
            std::string message = formatString->name;  // Replace with actual message generation using formatString

            LogMessage logMessage;
            logMessage.message = message;
            logMessage.userId = userId;
            logMessage.formatId = formatId;

            processLogMessage(logMessage);
        }
    }

    void resetMessageCounts() {
        lastUserMessageCounts_.clear();
    }

    void recallLogMessages(const std::function<void(const std::vector<LogMessage>&)>& callback) {
        callback(logMessages_);
    }

private:
    std::vector<User> users_;
    std::vector<FormatString> formatStrings_;
    std::map<int, int> lastUserMessageCounts_;
    std::vector<LogMessage> logMessages_;
    LogOutputCallback logOutputCallback_;

    User* getUserById(int userId) {
        for (auto& user : users_) {
            if (user.id == userId) {
                return &user;
            }
        }
        return nullptr;
    }

    FormatString* getFormatStringById(int formatId) {
        for (auto& formatString : formatStrings_) {
            if (formatString.id == formatId) {
                return &formatString;
            }
        }
        return nullptr;
    }

    void processLogMessage(const LogMessage& logMessage) {
        logMessages_.push_back(logMessage);

        auto countIterator = lastUserMessageCounts_.find(logMessage.userId);
        int count = (countIterator != lastUserMessageCounts_.end()) ? countIterator->second + 1 : 1;
        lastUserMessageCounts_[logMessage.userId] = count;

        if (count == 1 || count == 10 || count % 100 == 0) {
            if (logOutputCallback_) {
                logOutputCallback_(logMessage);
            }
        }
    }
};