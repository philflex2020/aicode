#include <gtest/gtest.h>

#include <sstream>
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

// TEST(TimeTrackerTest, ToJson) {
//     TimeTracker tracker("test");
//     tracker.addInterval(chrono::nanoseconds(100));
//     json result = tracker.toJson();
//     stringstream ssout;
//     ssout << result.dump() << endl;
//     //std::cout << result.dump() << endl;
//     std::string sres, sout;

//     ssout >> sout;
//     sres="{\"test\":{\"avg\":100.0,\"count\":1,\"max\":100,\"min\":100}}";

//     EXPECT_EQ(sres, sout);
// }

TEST(TimeTrackerTest, ToJsonStr) {
    TimeTracker tracker;
    tracker.setName("test");
    tracker.addInterval(chrono::nanoseconds(100));
    std::string sout = tracker.toJsonStr();
    std::string sres;

    sres="\"test\":{\"min\":100,\"max\":100,\"avg\":100,\"count\":1}";

    EXPECT_EQ(sres, sout);
}
TEST(TimeTrackerTest, ToJsonStrName) {
    TimeTracker tracker;
    tracker.setName("test");
    tracker.addInterval(chrono::nanoseconds(100));
    std::string sout = tracker.toJsonStr("myName");
    std::string sres;

    sres="\"myName\":{\"min\":0.0001,\"max\":0.0001,\"avg\":0.0001,\"count\":1}";

    EXPECT_EQ(sres, sout);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}