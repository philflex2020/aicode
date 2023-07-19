#pragma once

#include <functional>
#include <vector>
#include <chrono>
#include <string>
#include <thread>

enum class TransferMode {
    IMMEDIATE,
    PERIODIC_WITH_INPUT,
    PERIODIC_WITHOUT_INPUT
};

struct Data {
        int intData;
        float floatData;
        bool boolData;
        std::string stringData;

        Data() {}
        ~Data() {}
    };

class DataTransferManager {
public:
    // union Data {
    //     int intData;
    //     float floatData;
    //     bool boolData;
    //     std::string stringData;

    //     Data() {}
    //     ~Data() {}
    // };

    using DataCallback = std::function<void(Data, std::chrono::system_clock::time_point)>;



DataTransferManager(TransferMode mode, int periodMs)
    : mode_(mode), periodMs_(periodMs), running_(false) {
    lastDataInputs_.reserve(1024);
    timestamps_.reserve(1024);
};

~DataTransferManager() {
    stop();
};

// void setInputCallback(DataCallback callback) {
//     inputCallback_ = callback;
// }

void addData(int data) {
    Data newData;
    newData.intData = data;
    addData(newData);
}

void addData(float data) {
    Data newData;
    newData.floatData = data;
    addData(newData);
}

void addData(bool data) {
    Data newData;
    newData.boolData = data;
    addData(newData);
}

void addData(const std::string& data) {
    Data newData;
    newData.stringData = data;
    addData(newData);
}

void addData(Data data) {
    if (lastDataInputs_.size() >= 1024) {
        lastDataInputs_.erase(lastDataInputs_.begin());
        timestamps_.erase(timestamps_.begin());
    }

    lastDataInputs_.push_back(data);
    timestamps_.push_back(std::chrono::system_clock::now());
}

void start() {
    if (running_)
        return;

    running_ = true;
    transferThread_ = std::thread(&DataTransferManager::transferData, this);
}

void stop() {
    if (!running_)
        return;

    running_ = false;
    if (transferThread_.joinable()) {
        transferThread_.join();
    }
}
private:
void transferData() {
    while (running_) {
        switch (mode_) {
            case TransferMode::IMMEDIATE: {
                if (inputCallback_ && !lastDataInputs_.empty()) {
                    inputCallback_(lastDataInputs_.back(), timestamps_.back());
                }
                break;
            }
            case TransferMode::PERIODIC_WITH_INPUT: {
                if (inputCallback_ && !lastDataInputs_.empty()) {
                    inputCallback_(lastDataInputs_.back(), timestamps_.back());
                }
                break;
            }
            case TransferMode::PERIODIC_WITHOUT_INPUT: {
                if (inputCallback_) {
                    if (!lastDataInputs_.empty()) {
                        inputCallback_(lastDataInputs_.back(), timestamps_.back());
                    } else {
                        Data emptyData;
                        inputCallback_(emptyData, std::chrono::system_clock::now());
                    }
                }
                break;
            }
        }

        if (inputCallback_ && mode_ != TransferMode::IMMEDIATE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(periodMs_));
        }
    }
}


private:
    TransferMode mode_;
    int periodMs_;
    DataCallback inputCallback_;
    std::vector<Data> lastDataInputs_;
    std::vector<std::chrono::system_clock::time_point> timestamps_;
    std::thread transferThread_;
    bool running_;

};
