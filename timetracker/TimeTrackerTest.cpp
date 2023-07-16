#include <gtest/gtest.h>

#include "TimeTracker.h"

using namespace std;

TEST(TimeTrackerTest, AddInterval) {
    TimeTracker tracker("test");
    tracker.addInterval(chrono::nanoseconds(100));

    EXPECT_EQ(1, tracker.getSampleCount());
    EXPECT_EQ(100, tracker.getMinInterval());
    EXPECT_EQ(100, tracker.getMaxInterval());
    EXPECT_EQ(100, tracker.getAverageInterval());
}

TEST(TimeTrackerTest, ClearIntervals) {
    TimeTracker tracker("test");
    tracker.addInterval(chrono::nanoseconds(100));

    tracker.clearIntervals();

    EXPECT_EQ(0, tracker.getSampleCount());
    EXPECT_EQ(LONG_LONG_MAX, tracker.getMinInterval());
    EXPECT_EQ(0, tracker.getMaxInterval());
    EXPECT_EQ(0, tracker.getAverageInterval());
}

TEST(TimeTrackerTest, ToJson) {
    TimeTracker tracker("test");
    tracker.addInterval(chrono::nanoseconds(100));

    json expectedResult = {
        "test": {
            "min", 100,
            "max", 100,
            "avg", 100,
            "count", 4
            }
    };

    EXPECT_EQ(expectedResult, tracker.toJson());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}