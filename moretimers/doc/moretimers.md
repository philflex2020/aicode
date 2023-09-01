Certainly, your source code would typically be structured as below:

```
project_directory/
|-- include/
|   |-- Timer.h
|-- src/
|   |-- Timer.cpp
|   |-- main.cpp
|-- test/
|   |-- test_Timer.cpp
|-- build/
|-- Makefile
```

1. **Timer.h**: This file contains the declaration of the Timer class.
2. **Timer.cpp**: This file contains the implementation of the Timer class.
3. **main.cpp**: This file contains the `main()` function, which is the entry point of the program.
4. **test_Timer.cpp**: This file contains the Google Test (gtest) unit tests for the Timer class.
5. **build/**: This directory contains all the object files and the final executable.
6. **Makefile**: This file contains the rules to build the project.

Your files would look something like this:

**include/Timer.h**
```cpp
#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <atomic>
#include <thread>
#include <functional>

class Timer {
public:
    Timer(std::chrono::milliseconds interval, std::chrono::milliseconds offset, 
            std::function<void(Timer*, void*)> callback, void* varPtr);

    void start();
    void stop();
    void sync();
    
private:
    std::chrono::milliseconds interval;
    std::chrono::milliseconds offset;
    std::chrono::steady_clock::time_point baseTime;
    std::chrono::steady_clock::time_point runtime;
    int synctime;
    std::atomic<bool> run;
    std::function<void(Timer*, void*)> callback;
    void* varPtr;
};

#endif // TIMER_H
```

**src/Timer.cpp**
```cpp
#include "Timer.h"

Timer::Timer(std::chrono::milliseconds interval, std::chrono::milliseconds offset, 
             std::function<void(Timer*, void*)> callback, void* varPtr) 
    : interval(interval), offset(offset), baseTime(std::chrono::steady_clock::now()),
      synctime(0), run(false), callback(callback), varPtr(varPtr) {}

void Timer::start() {
    run = true;
    auto now = std::chrono::steady_clock::now();
    auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - baseTime).count();
    int initialDelay = offset.count() - timeSinceStart;
    if(initialDelay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(initialDelay));
    }
    while(run) {
        std::this_thread::sleep_for(interval);
        if (synctime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(synctime));
            synctime = 0;
        }
        runtime = std::chrono::steady_clock::now();
        callback(this, varPtr);
    }
}

void Timer::stop(){
    run = false;
}

void Timer::sync () {  
    auto now = std::chrono::steady_clock::now();
    auto timeSinceRun = std::chrono::duration_cast<std::chrono::milliseconds>(now - runtime);
    auto remainingTime = interval - timeSinceRun;
    if (remainingTime.count() > interval.count() / 2) {
        synctime = remainingTime.count();
    } else {
        synctime = 0;
    }
}
```

**src/main.cpp**
```cpp
#include "Timer.h"
#include <iostream>

void callbackFunction(Timer* timer, void* varPtr) {
    std::cout << "Callback function called!" << std::endl;
}

int main() {
    Timer timer(std::chrono::milliseconds(1000), std::chrono::milliseconds(500), callbackFunction, nullptr);
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    timer.stop();
    return 0;
}
```

**test/test_Timer.cpp**
```cpp
#include "Timer.h"
#include <gtest/gtest.h>

// You can write test cases here.

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

**Makefile**
```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Iinclude
LDFLAGS = -lpthread -lgtest -lgtest_main

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
TEST_DIR = test

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: $(BUILD_DIR)/main

$(BUILD_DIR)/main: $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(BUILD_DIR)/test

$(BUILD_DIR)/test: $(BUILD_DIR)/Timer.o $(TEST_DIR)/test_Timer.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(BUILD_DIR)/Timer.o $(TEST_DIR)/test_Timer.cpp -o $@

clean:
	rm -f $(BUILD_DIR)/*
```

In the `Makefile`, the `all` target builds the main executable, and the `test` target builds the test executable. The `clean` target removes all the files in the `build` directory. The `CXXFLAGS` variable includes the `include` directory, and the `LDFLAGS` variable includes the necessary libraries for threading and Google Test.

To build the project, run `make` in the `project_directory`. To build and run the tests, run `make test` and then `./build/test`. To clean the `build` directory, run `make clean`.