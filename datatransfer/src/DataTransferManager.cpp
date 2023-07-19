
#include "DataTransferManager.h"

DataTransferManager::DataTransferManager(TransferMode mode, int periodMs)
    : mode_(mode), periodMs_(periodMs), running_(false) {
    lastDataInputs_.reserve(1024);
    timestamps_.reserve(1024);
}

DataTransferManager::~DataTransferManager() {
    stop();
}

void DataTransferManager::setInputCallback(DataCallback callback) {
    inputCallback_ = callback;
}

void DataTransferManager::addData(int data) {
    Data newData;
    newData.intData = data;
    addData(newData);
}

void DataTransferManager::addData(float data) {
    Data newData;
    newData.floatData = data;
    addData(newData);
}

void DataTransferManager::addData(bool data) {
    Data newData;
    newData.boolData = data;
    addData(newData);
}

void DataTransferManager::addData(const std::string& data) {
    Data newData;
    newData.stringData = data;
    addData(newData);
}

void DataTransferManager::addData(Data data) {
    if (lastDataInputs_.size() >= 1024) {
        lastDataInputs_.erase(lastDataInputs_.begin());
        timestamps_.erase(timestamps_.begin());
    }

    lastDataInputs_.push_back(data);
    timestamps_.push_back(std::chrono::system_clock::now());
}

void DataTransferManager::start() {
    if (running_)
        return;

    running_ = true;
    transferThread_ = std::thread(&DataTransferManager::transferData, this);
}

void DataTransferManager::stop() {
    if (!running_)
        return;

    running_ = false;
    if (transferThread_.joinable()) {
        transferThread_.join();
    }
}

void DataTransferManager::transferData() {
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
