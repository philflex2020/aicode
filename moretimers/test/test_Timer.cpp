#include <gtest/gtest.h>
#include <iostream>
#include "Timer.h"

// Define a callback function for the timer
// void onTestTimer(Timer* timer, void* varPtr) {
//     int* var = (int*) varPtr;
//     *var = *var + 1;
// }
void onTestTimer(Timer* timer, void* varPtr) {
    auto now = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - Timer::baseTime).count();
    std::cout << "Timer " << timer->name << " callback called. Elapsed time since base time: " << elapsedTime << " ms." << std::endl;
    int* var = (int*) varPtr;
    *var = *var + 1;
}
// Define a test case
TEST(TimerTest, BasicFunctionality) {
    Timer::baseTime = std::chrono::steady_clock::now();
    int var = 0;
    // Start a timer with an interval of 100 milliseconds, offset of 50 milliseconds
    startTimer("testTimer", 100, 50, onTestTimer, &var);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Stop the timer
    stopTimer("testTimer");
    // Check that the variable was incremented by the timer
    ASSERT_GE(var, 4);
    ASSERT_LE(var, 6);
}

// Define another test case
TEST(TimerTest, SyncFunctionality) {
    Timer::baseTime = std::chrono::steady_clock::now();
    int var = 0;
    // Start a timer with an interval of 100 milliseconds, offset of 50 milliseconds
    startTimer("testTimer", 100, 50, onTestTimer, &var);
    std::this_thread::sleep_for(std::chrono::milliseconds(310));
    timers["testTimer"]->sync();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    // Stop the timer
    stopTimer("testTimer");
    // Check that the variable was incremented by the timer
    ASSERT_GE(var, 5);
    ASSERT_LE(var, 8);
}

// Run all the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}