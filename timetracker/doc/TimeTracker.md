
### Spec

Create a timetracker object that creates a list of named objects.
Each object is given a time interval (in nanoseconds) to record.
The TimeTracker will record the max , min, avg of each sample given to each object.
THe list is stored in a strcture using a timestore thread and the time tracker sends the collected time samples to the thread for storage.

The Timetracker will construct a Json string for one or more named objects  on request, a key string for the json output  is placed in to the request 

Additionally, please provide detailed  gtest code, sample code  and a Makefile.
Put the cpp code in the src directory , the include files in the include directory and the test objects in the test directory.


### TimeTracker Include

```cpp
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;
using namespace std::chrono;

class TimeTracker {
private:
    string name;
    vector<long long> intervals;
    long long minInterval;
    long long maxInterval;
    long long totalInterval;
    int sampleCount;

public:
    TimeTracker(const string &name) : name(name) {
        minInterval = LONG_LONG_MAX;
        maxInterval = 0;
        totalInterval = 0;
        sampleCount = 0;
    }

    void addInterval(const chrono::nanoseconds &interval) {
        long long duration = interval.count();
        intervals.push_back(duration);
        totalInterval += duration;
        sampleCount++;
        minInterval = min(minInterval, duration);
        maxInterval = max(maxInterval, duration);
    }

    void clearIntervals() {
        intervals.clear();
        minInterval = LONG_LONG_MAX;
        maxInterval = 0;
        totalInterval = 0;
        sampleCount = 0;
    }

    double getAverageInterval() {
        if (sampleCount == 0) {
            return 0;
        }
        return static_cast<double>(totalInterval) / sampleCount;
    }

    long long getMinInterval() {
        return minInterval;
    }

    long long getMaxInterval() {
        return maxInterval;
    }

    int getSampleCount() {
        return sampleCount;
    }

    json toJson() {
        json jsonObject;
        jsonObject["name"] = name;
        jsonObject["min"] = minInterval;
        jsonObject["max"] = maxInterval;
        jsonObject["avg"] = getAverageInterval();
        jsonObject["count"] = sampleCount;
        return jsonObject;
    }
};
```
### TimeTracker Example

```cpp
int main() {
    TimeTracker tracker("sample");
    tracker.addInterval(chrono::nanoseconds(100));
    tracker.addInterval(chrono::nanoseconds(200));
    tracker.addInterval(chrono::nanoseconds(150));
    tracker.addInterval(chrono::nanoseconds(300));

    std::string result = tracker.toJsonStr("myres");
    cout << result << endl;

    tracker.clearIntervals();

    return 0;
}
```

### TimeTracker Makefile Example

To compile and run the code, youll need to have the simdjson and spdlog libraries installed. 
You can use the following Makefile:

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

SRC = src/TimeTracker.cpp
INC = include/TimeTracker.h
TEST_SRC = test/TimeTrackerTest.cpp
BUILD_DIR = build
TARGET = $(BUILD_DIR)/TimeTracker 
TEST_TARGET = $(BUILD_DIR)/TimeTrackerTest 

LIB_PATH = /usr/local/lib
INCLUDE_PATH = ./include

all: build $(TEST_TARGET) $(TARGET)

build:
	mkdir -p $(BUILD_DIR)

$(TEST_TARGET): $(TEST_SRC) $(INC) 
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_PATH) $< -o $@ -L$(LIB_PATH) -lsimdjson -lgtest -lspdlog -lpthread

$(TARGET): $(SRC) $(INC)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_PATH) $< -o $@ -L$(LIB_PATH) -lsimdjson -lspdlog

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET)
	rm -f $(TEST_TARGET)
```

### TimeTracker Test 

```cpp

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
```

# TimeTracker

TimeTracker is a C++ class that allows you to track and analyze time intervals. It supports tracking the maximum, minimum, average, and count of samples, and provides a function to generate a JSON object containing these statistics.

## Requirements

- C++ compiler that supports C++17
- simdjson library (for JSON parsing)
- spdlog library (for logging)
- Google Test framework (for running the tests)

## How to Use

1. Copy the `TimeTracker.h` and `TimeTracker.cpp` files into your project directory.
2. Include the `TimeTracker.h` file in your code.
3. Create an instance of the `TimeTracker` class, providing a name for the tracker.
4. Use the `addInterval` function to add time intervals to the tracker.
5. Use the provided functions (`getMinInterval`, `getMaxInterval`, `getAverageInterval`, `getSampleCount`) to access the tracked statistics.
6. Use the `toJson` function to generate a JSON object containing the statistics.

Example usage:

```cpp
#include "TimeTracker.h"

int main() {
    TimeTracker tracker("sample");
    tracker.addInterval(chrono::nanoseconds(100));
    tracker.addInterval(chrono::nanoseconds(200));
    tracker.addInterval(chrono::nanoseconds(150));
    tracker.addInterval(chrono::nanoseconds(300));

    auto  result = tracker.toJsonStr("sample");
    cout << result << endl;

    tracker.clearIntervals();

    return 0;
}
```
