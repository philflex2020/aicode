### Request 
write a c++  class  to manage a watchdog object.
 Use a high_resolution_clock 
Use c++17 and dont forget to include thread.
The Watchdog object  has four states Fault, Warning, Recovery and Normal
The watchdog object gets an integer value which is expected to change within a designated time period.
If no change is detected in the warning time the watchdog enters the warning state and executes a warning callback with the watchdog object and a void* as parameters.
If no change is detected in the alarm time the watchdog enters the alarm state and executes a fault  callback with the watchdog object and a void* as parameters.
If the watchdog object is in a fault or warning state and detects an input change it enters the recovery state for the recovery time period and executes a recovery callback with the watchdog object and a void * as parameters.
If the input continues to change during the recovery time  the watchdog will enter an normal state and executes a normal callback with the watchdog object and a void * as parameters.

If the watchdog receives no input at all i wiil automatically enter the warning and fault states.


// value not changed after interval time == problem
   // if problem
   // if (normal)
   // if warning wait == 0
   //   set warningwait to now + warningTime 
   // if faultwait == 0
   //   set faultwait to now + faultTime 
   // if now > warningwait
   //  set state to warning  run warning callback
   //   set recoverywait to now + recovery time 
   // if now > faultwait
   //  set state to fault  run fault callback
   //   set recoverywait to now + recovery time 
   // 
   // if (warning or fault or recovery)
   //   set recoverywait to now + recovery time 


   // if (!problem)
   // if (alarm or warning )
   //  run recovery callback
   //   set state recovery
   // if (recovery)
   //    if now > recoverywait
   //      set state normal
  //       run normal callback
  //       set recoverywait to 0
   //   


### Watchdog include code

```
#pragma once

#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>

enum class WatchdogState {
    Fault,
    Warning,
    Recovery,
    Normal
};

class Watchdog {
public:
    Watchdog(int intervalTime, int warningTime, int faultTime, int recoveryTime,
             std::function<void(Watchdog&, void*)> warningCallback,
             std::function<void(Watchdog&, void*)> faultCallback,
             std::function<void(Watchdog&, void*)> recoveryCallback,
             std::function<void(Watchdog&, void*)> normalCallback,
             void* callbackParam)
        : intervalTime(intervalTime),
          warningTime(warningTime),
          faultTime(faultTime),
          recoveryTime(recoveryTime),
          warningCallback(warningCallback),
          faultCallback(faultCallback),
          recoveryCallback(recoveryCallback),
          normalCallback(normalCallback),
          callbackParam(callbackParam),
          currentState(WatchdogState::Normal),
          lastChangeTime(std::chrono::high_resolution_clock::now()),
          warningWait(std::chrono::high_resolution_clock::now()),
          faultWait(std::chrono::high_resolution_clock::now()),
          recoveryWait(std::chrono::high_resolution_clock::now()) {}
   

   // value not changed after interval time == problem
   // if problem
   // if (normal)
   // if warning wait == 0
   //   set warningwait to now + warningTime 
   // if faultwait == 0
   //   set faultwait to now + faultTime 
   // if now > warningwait
   //  set state to warning  run warning callback
   //   set recoverywait to now + recovery time 
   // if now > faultwait
   //  set state to fault  run fault callback
   //   set recoverywait to now + recovery time 
   // 
   // if (warning or fault or recovery)
   //   set recoverywait to now + recovery time 


   // if (!problem)
   // if (alarm or warning )
   //  run recovery callback
   //   set state recovery
   // if (recovery)
   //    if now > recoverywait
   //      set state normal
  //       run normal callback
  //       set recoverywait to 0
   //   

    void setInputValue(int value) {
        std::lock_guard<std::mutex> lock(mymutex);
        auto currentTime = std::chrono::high_resolution_clock::now();
        bool problem = true;
        if (value != inputValue) 
        {
            problem = false;
            lastChangeTime = currentTime;
            std::cout << " Value changed  : no Problem" << std::endl;
            inputValue = value;
        }    
        else
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastChangeTime).count();
            if (elapsed < intervalTime)
            {
                std::cout << " Value not changed but within interval : no Problem" << std::endl;
                return;
            }
            std::cout << " Value not changed possible problem, Elapsed time :" << elapsed << std::endl;

        }

        if (!problem)
        {
           if (currentState == WatchdogState::Normal && warningWait<currentTime) {
                warningWait = currentTime + std::chrono::milliseconds(warningTime);
           }
           if (currentState == WatchdogState::Normal && faultWait<currentTime) {
                faultWait = currentTime + std::chrono::milliseconds(faultTime);
           }
 
           if (currentState == WatchdogState::Normal) {
                if (currentTime > warningWait) 
                {
                    currentState = WatchdogState::Warning;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (warningCallback) 
                    {
                        warningCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached warning : no Problem yet" << std::endl;
                }                  
            }
            if ((currentState == WatchdogState::Normal || currentState == WatchdogState::Warning) ) {
                if (currentTime > faultWait) { 
                    currentState = WatchdogState::Fault;
                    if (faultCallback) {
                        faultCallback(*this, callbackParam);
                    }
                }
                else
                {
                    std::cout << " Not reached fault : no Problem yet" << std::endl;
                }                  

            }
            if ((currentState == WatchdogState::Fault || currentState == WatchdogState::Warning) ) {
                recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                warningWait = currentTime + std::chrono::milliseconds(warningTime);
                faultWait = currentTime + std::chrono::milliseconds(faultTime);

                currentState = WatchdogState::Recovery;
                if (recoveryCallback) {
                    recoveryCallback(*this, callbackParam);
                }
                
                std::cout << " Started Recovery" << std::endl;
            }
            if (currentState == WatchdogState::Recovery)  {
                //recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);

                if (currentTime > recoveryWait) { 
                    currentState = WatchdogState::Normal;
                    warningWait = currentTime + std::chrono::milliseconds(warningTime);
                    faultWait = currentTime + std::chrono::milliseconds(faultTime);

                    if (normalCallback) {
                        normalCallback(*this, callbackParam);
                    }
                    std::cout << " Back to normal" << std::endl;
                }
            }

        }
        else  // we got a problem
        {
           if (currentState == WatchdogState::Normal) {
                if (currentTime > warningWait) 
                {
                    currentState = WatchdogState::Warning;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (warningCallback)   
                    {
                        warningCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached warning : no Problem yet" << std::endl;
                }                  
            }

            if (currentState == WatchdogState::Warning) {
                if (currentTime > faultWait) 
                {
                    currentState = WatchdogState::Fault;
                    recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                    if (faultCallback) 
                    {
                        faultCallback(*this, callbackParam);
                    }
                } 
                else
                {
                    std::cout << " Not reached fault : still in warning" << std::endl;
                }                  
            }
            if (currentState == WatchdogState::Recovery) {
                recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
                std::cout << " extending recovert wait" << std::endl;
            }                  
            //}

            //     currentState = WatchdogState::Recovery;
            //     if (recoveryCallback) {
            //         recoveryCallback(*this, callbackParam);
            //     }
            //     recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);
            // }
            // if (currentState == WatchdogState::Fault) {
            //     currentState = WatchdogState::Recovery;
            //     if (recoveryCallback) {
            //         recoveryCallback(*this, callbackParam);
            //     }
            //     recoveryWait = currentTime + std::chrono::milliseconds(recoveryTime);

            // }
            // if (currentState == WatchdogState::Recovery && currentTime > recoveryWait) {
            //     currentState = WatchdogState::Normal;
            //     if (normalCallback) {
            //         normalCallback(*this, callbackParam);
            //     }
            // }
        }

        // if (value != inputValue) {
        //     lastChangeTime = currentTime;
        //     inputValue = value;

        //     if (currentState == WatchdogState::Normal && elapsed > warningTime) {
        //         currentState = WatchdogState::Warning;
        //         if (warningCallback) {
        //             warningCallback(*this, callbackParam);
        //         }
        //     }
        // }
    }

    WatchdogState getState() const {
        std::lock_guard<std::mutex> lock(mymutex);
        return currentState;
    }

private:
    int warningTime;
    int faultTime;
    int recoveryTime;
    int intervalTime;
    std::function<void(Watchdog&, void*)> warningCallback;
    std::function<void(Watchdog&, void*)> faultCallback;
    std::function<void(Watchdog&, void*)> recoveryCallback;
    std::function<void(Watchdog&, void*)> normalCallback;
    void* callbackParam;
    WatchdogState currentState;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastChangeTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> warningWait;
    std::chrono::time_point<std::chrono::high_resolution_clock> faultWait;
    std::chrono::time_point<std::chrono::high_resolution_clock> recoveryWait;
    int inputValue = 0;
//public:
    mutable std::mutex mymutex;
};
```
### Watchdog Example
```
# include "Watchdog.h"

// Test functions
void warningCallback(Watchdog& watchdog, void* param) {
    std::cout << "Warning state triggered." << std::endl;
}

void faultCallback(Watchdog& watchdog, void* param) {
    std::cout << "Fault state triggered." << std::endl;
}

void recoveryCallback(Watchdog& watchdog, void* param) {
    std::cout << "Recovery state triggered." << std::endl;
}

void normalCallback(Watchdog& watchdog, void* param) {
    std::cout << "Normal state triggered." << std::endl;
}

int main() {
    // Create a Watchdog instance
    Watchdog watchdog(100, 500, 1000, 2000, warningCallback, faultCallback, recoveryCallback, normalCallback, nullptr);

    // Simulate input changes
    watchdog.setInputValue(1);
    std::cout << " wait 50ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    watchdog.setInputValue(1);
    std::cout << " wait 40ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    watchdog.setInputValue(2);

    std::cout << " wait 140ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(140));
    watchdog.setInputValue(2);
    
    std::cout << " wait 540ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(540));
    watchdog.setInputValue(2);
    
    std::cout << " wait 540ms"<< std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(540));
    watchdog.setInputValue(2);
    std::cout << " finished"<< std::endl;
    // std::cout << " wait 1000ms"<< std::endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // watchdog.setInputValue(3);
    // std::cout << " wait 1500ms"<< std::endl;

    // std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    // watchdog.setInputValue(4);

    return 0;
}
```

### Watchdog Test

```
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
```

### Makefile

```
CFLAGS = -std=c++17 -pthread -lgtest -lgmock 

SRC = src/Watchdog.cpp
TEST_SRC = test/WatchdogTest.cpp

OBJ = build/Watchdog
TEST_OBJ = build/WatchdogTest

all: build $(OBJ) $(TEST_OBJ)


build:
	mkdir -p build

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -I./include  -o $@ $<

$(TEST_OBJ): $(TEST_SRC)
	$(CC) $(CFLAGS) -I./include  -o $@ $<


clean:
	rm -f $(OBJ)
	rm -f $(TEST_OBJ)



test: build $(TEST_OBJ)
	./$(TEST_OBJ)
```