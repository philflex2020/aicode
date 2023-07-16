#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Include the Watchdog header
#include "watchdog.h"



// Define the test fixture
class WatchdogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up the test environment
    }

    void TearDown() override {
        // Tear down the test environment
    }
    // Test functions
    static void warningCallback(Watchdog& watchdog, void* param) {
        std::cout << ">>>>>>>>>>>Warning state triggered." << std::endl;
    }

    static void faultCallback(Watchdog& watchdog, void* param) {
        std::cout << ">>>>>>>>>Fault state triggered." << std::endl;
    }

    static void recoveryCallback(Watchdog& watchdog, void* param) {
        std::cout << ">>>>>>>>>Recovery state triggered." << std::endl;
    }

    static void normalCallback(Watchdog& watchdog, void* param) {
        std::cout << ">>>>>>>>>Normal state triggered." << std::endl;
    }
    // Declare any additional helper functions or variables
};

// Define test cases and tests
TEST_F(WatchdogTest, TestInitialState) {
    // Create a Watchdog instance
    //Watchdog watchdog(100, 500, 1000, 2000, warningCallback, faultCallback, recoveryCallback, normalCallback, nullptr);
    //Watchdog watchdog(100, 500, 1000, 2000, nullptr, nullptr, nullptr, nullptr, nullptr);
    Watchdog watchdog(100, 500, 1000, 2000, WatchdogTest::warningCallback, nullptr, nullptr, nullptr, nullptr);

    watchdog.setInputValue(1);
    // Test the initial state of the Watchdog
    EXPECT_EQ(watchdog.getState(), WatchdogState::Normal);
}

TEST_F(WatchdogTest, TestWarningState) {

    Watchdog watchdog(100, 500, 1000, 2000, WatchdogTest::warningCallback, WatchdogTest::faultCallback, WatchdogTest::recoveryCallback, WatchdogTest::normalCallback, nullptr);
    //Watchdog watchdog(100, 500, 1000, 2000, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Simulate input changes that trigger the warning state
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    watchdog.setInputValue(1);

    // Test the state of the Watchdog after entering the warning state
    EXPECT_EQ(watchdog.getState(), WatchdogState::Warning);
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Recovery);
}

TEST_F(WatchdogTest, TestFaultState) {

    Watchdog watchdog(100, 500, 1000, 2000, WatchdogTest::warningCallback, WatchdogTest::faultCallback, WatchdogTest::recoveryCallback, WatchdogTest::normalCallback, nullptr);
    //Watchdog watchdog(100, 500, 1000, 2000, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Simulate input changes that trigger the warning state
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    watchdog.setInputValue(1);

    // Test the state of the Watchdog after entering the warning state
    EXPECT_EQ(watchdog.getState(), WatchdogState::Fault);
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Recovery);
}
TEST_F(WatchdogTest, TestRecoveryState) {

    Watchdog watchdog(100, 500, 1000, 500, WatchdogTest::warningCallback, WatchdogTest::faultCallback, WatchdogTest::recoveryCallback, WatchdogTest::normalCallback, nullptr);
    //Watchdog watchdog(100, 500, 1000, 2000, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Simulate input changes that trigger the warning state
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(3);

    // Test the state of the Watchdog after entering the warning state
    EXPECT_EQ(watchdog.getState(), WatchdogState::Recovery);
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Normal);
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Fault);
}
TEST_F(WatchdogTest, TestRecoveryNormalState) {

    Watchdog watchdog(100, 500, 1000, 300, WatchdogTest::warningCallback, WatchdogTest::faultCallback, WatchdogTest::recoveryCallback, WatchdogTest::normalCallback, nullptr);
    //Watchdog watchdog(100, 500, 1000, 2000, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Simulate input changes that trigger the warning state
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1800));
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(3);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(3);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(90));
    watchdog.setInputValue(3);

    // Test the state of the Watchdog after entering the warning state
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Recovery);
    EXPECT_EQ(watchdog.getState(), WatchdogState::Normal);
    //EXPECT_EQ(watchdog.getState(), WatchdogState::Fault);
}
// ... Add more tests

// Run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}