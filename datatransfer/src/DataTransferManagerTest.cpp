
#include <gtest/gtest.h>
#include "DataTransferManager.h"

TEST(DataTransferManagerTest, ImmediateTransfer) {
    DataTransferManager::TransferMode mode = DataTransferManager::TransferMode::IMMEDIATE;
    DataTransferManager dataTransfer(mode);

    int testData = 42;
    std::chrono::system_clock::time_point testTimestamp;

    dataTransfer.setInputCallback([&](int data, std::chrono::system_clock::time_point timestamp) {
        testTimestamp = timestamp;
        ASSERT_EQ(data, testData);
    });

    dataTransfer.start();
    dataTransfer.addData(testData);

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait for transfer

    dataTransfer.stop();

    // Ensure that the timestamp is set during the callback
    ASSERT_NE(testTimestamp, std::chrono::system_clock::time_point());
}

TEST(DataTransferManagerTest, PeriodicTransferWithInput) {
    DataTransferManager::TransferMode mode = DataTransferManager::TransferMode::PERIODIC_WITH_INPUT;
    int periodMs = 100;
    DataTransferManager dataTransfer(mode, periodMs);

    int testData1 = 10;
    int testData2 = 20;
    std::chrono::system_clock::time_point testTimestamp1;
    std::chrono::system_clock::time_point testTimestamp2;

    dataTransfer.setInputCallback([&](int data, std::chrono::system_clock::time_point timestamp) {
        testTimestamp1 = timestamp;
        ASSERT_EQ(data, testData2); // Verify that data is always the last added
    });

    dataTransfer.start();
    dataTransfer.addData(testData1);

    std::this_thread::sleep_for(std::chrono::milliseconds(2 * periodMs)); // Wait for periodic transfer

    dataTransfer.addData(testData2);

    std::this_thread::sleep_for(std::chrono::milliseconds(2 * periodMs)); // Wait for periodic transfer

    dataTransfer.stop();

    // Ensure that the timestamp is set during the callback for both data points
    ASSERT_NE(testTimestamp1, std::chrono::system_clock::time_point());
    ASSERT_NE(testTimestamp2, std::chrono::system_clock::time_point());
}

TEST(DataTransferManagerTest, PeriodicTransferWithoutInput) {
    DataTransferManager::TransferMode mode = DataTransferManager::TransferMode::PERIODIC_WITHOUT_INPUT;
    int periodMs = 100;
    DataTransferManager dataTransfer(mode, periodMs);

    int testData = 99;
    std::chrono::system_clock::time_point testTimestamp;

    dataTransfer.setInputCallback([&](int data, std::chrono::system_clock::time_point timestamp) {
        testTimestamp = timestamp;
        ASSERT_EQ(data, testData);
    });

    dataTransfer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(2 * periodMs)); // Wait for periodic transfer

    dataTransfer.stop();

    // Ensure that the timestamp is set during the callback even without adding data
    ASSERT_NE(testTimestamp, std::chrono::system_clock::time_point());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
