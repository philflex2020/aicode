#ifndef DATATRANSFERMANAGER_H
#define DATATRANSFERMANAGER_H

#include <functional>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <thread>
#include <memory> // Include for smart pointers

enum class TransferMode {
    IMMEDIATE,
    PERIODIC_WITH_INPUT,
    PERIODIC_WITHOUT_INPUT
};

class DataTransferObject {
public:
    using DataCallback = std::function<void(int, std::chrono::system_clock::time_point)>;

    DataTransferObject(TransferMode mode, int periodMs = 1000)
        : mode_(mode), periodMs_(periodMs), running_(false) {
        lastDataInputs_.reserve(1024);
        timestamps_.reserve(1024);
    }

    ~DataTransferObject() {
        stop();
    }

    void setInputCallback(DataCallback callback) {
        inputCallback_ = callback;
    }

    void addData(int data) {
        // Create a new data object containing the data and timestamp
        // We are using an int as an example, but this could be any data type
        // with its respective transfer mode.
        Data newData;
        newData.data = data;
        newData.timestamp = std::chrono::system_clock::now();

        if (lastDataInputs_.size() >= 1024) {
            lastDataInputs_.erase(lastDataInputs_.begin());
            timestamps_.erase(timestamps_.begin());
        }

        lastDataInputs_.push_back(newData);
        timestamps_.push_back(newData.timestamp);
    }

    void start() {
        if (running_)
            return;

        running_ = true;
        transferThread_ = std::thread(&DataTransferObject::transferData, this);
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
    struct Data {
        int data;
        std::chrono::system_clock::time_point timestamp;
    };

    TransferMode mode_;
    int periodMs_;
    DataCallback inputCallback_;
    std::vector<Data> lastDataInputs_;
    std::vector<std::chrono::system_clock::time_point> timestamps_;
    std::thread transferThread_;
    bool running_;

    void transferData() {
        while (running_) {
            switch (mode_) {
                case TransferMode::IMMEDIATE: {
                    if (inputCallback_ && !lastDataInputs_.empty()) {
                        auto newData = lastDataInputs_.back();
                        inputCallback_(newData.data, newData.timestamp);
                    }
                    break;
                }
                case TransferMode::PERIODIC_WITH_INPUT: {
                    if (inputCallback_ && !lastDataInputs_.empty()) {
                        auto newData = lastDataInputs_.back();
                        inputCallback_(newData.data, newData.timestamp);
                    }
                    break;
                }
                case TransferMode::PERIODIC_WITHOUT_INPUT: {
                    if (inputCallback_) {
                        if (!lastDataInputs_.empty()) {
                            auto newData = lastDataInputs_.back();
                            inputCallback_(newData.data, newData.timestamp);
                        } else {
                            inputCallback_(0, std::chrono::system_clock::now());
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
};

class DataTransferManager {
public:
    DataTransferManager() : running_(false) {}

    ~DataTransferManager() {
        stop();
    }

    void addDataObject(const std::string& dataId, TransferMode mode, int periodMs = 1000) {
        dataObjects_[dataId] = std::make_shared<DataTransferObject>(mode, periodMs);
    }

    void setInputCallback(const std::string& dataId, DataTransferObject::DataCallback callback) {
        if (dataObjects_.count(dataId) > 0) {
            dataObjects_[dataId]->setInputCallback(callback);
        }
    }

    void addData(const std::string& dataId, int data) {
        if (dataObjects_.count(dataId) > 0) {
            dataObjects_[dataId]->addData(data);
        }
    }

    void start() {
        if (running_)
            return;

        running_ = true;
        for (auto& [dataId, dataObject] : dataObjects_) {
            dataObject->start();
        }
    }

    void stop() {
        if (!running_)
            return;

        running_ = false;
        for (auto& [dataId, dataObject] : dataObjects_) {
            dataObject->stop();
        }
    }

private:
    std::map<std::string, std::shared_ptr<DataTransferObject>> dataObjects_;
    bool running_;
};
#endif