// CommandArgs
// p wilshire
// 07_16_2023

#include <gtest/gtest.h>
#include "CommandArgs.h"

// Test fixture for CommandLineArgs
class CommandLineArgsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up the test environment
    }

    void TearDown() override {
        // Tear down the test environment
    }

    // Declare any additional helper functions or variables
};

// Test case for retrieving argument values
TEST_F(CommandLineArgsTest, GetValueTest) {
    int argc = 5;
    char* argv[] = { const_cast<char*>("test"), const_cast<char*>("--name"), const_cast<char*>("John"), const_cast<char*>("--age"), const_cast<char*>("30"), const_cast<char *>("someFile") };

    CommandLineArgs cmdArgs;
    cmdArgs.parse(argc, argv);

    ASSERT_EQ(cmdArgs.getValue("name"), "John");
    ASSERT_EQ(cmdArgs.getValue("age"), "30");
    ASSERT_EQ(cmdArgs.getValue("city"), "");
    ASSERT_EQ(cmdArgs.getValue("country"), "");
}

// Test case for checking if an argument is present
TEST_F(CommandLineArgsTest, HasValueTest) {
    int argc = 3;
    char* argv[] = { const_cast<char*>("test"), const_cast<char*>("--name"), const_cast<char*>("John") };

    CommandLineArgs cmdArgs;
    cmdArgs.parse(argc, argv);

    ASSERT_TRUE(cmdArgs.hasValue("name"));
    ASSERT_FALSE(cmdArgs.hasValue("age"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}