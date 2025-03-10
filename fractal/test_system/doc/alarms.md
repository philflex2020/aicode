To achieve the flexibility of defining a sensor and then dynamically adding multiple alarm levels for that sensor, each with its own thresholds and other configurations, you will need to make several adjustments to the existing code structure. Here's a revised version of your `AlarmMonitor` class to accommodate these requirements. This version allows you to register multiple alarm levels per sensor with individual configurations and evaluate each level when updating sensor values.

### Revised Code Implementation

```cpp
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <functional>

double ref_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

class AlarmMonitor {
public:
    struct AlarmLevelConfig {
        int threshold;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmLevelState {
        bool isActive = false;
        double lastTriggerTime = 0;
        double lastResetTime = 0;
    };

    struct SensorConfig {
        std::vector<AlarmLevelConfig> levels;
    };

    struct SensorState {
        std::vector<AlarmLevelState> levelStates;
        double lastCheckedTime = 0;
    };

    using AlarmCallback = std::function<void(const std::string&, int)>;

private:
    std::map<std::string, SensorConfig> sensorConfigs;
    std::map<std::string, SensorState> sensorStates;
    std::map<std::string, int> sensorValues;
    std::map<std::string, AlarmCallback> alarmCallbacks;

public:
    void configureSensor(const std::string& sensorId, const SensorConfig& config) {
        sensorConfigs[sensorId] = config;
        SensorState& state = sensorStates[sensorId];
        state.levelStates.resize(config.levels.size());
        state.lastCheckedTime = ref_time_dbl();
    }

    void addAlarmLevel(const std::string& sensorId, const AlarmLevelConfig& levelConfig) {
        sensorConfigs[sensorId].levels.push_back(levelConfig);
        sensorStates[sensorId].levelStates.push_back(AlarmLevelState());
    }

    void updateSensorValue(const std::string& sensorId, int newValue) {
        sensorValues[sensorId] = newValue;
        evaluateSensor(sensorId);
    }

    void registerCallback(const std::string& sensorId, const AlarmCallback& callback) {
        alarmCallbacks[sensorId] = callback;
    }

private:
    void evaluateSensor(const std::string& sensorId) {
        auto& sensorState = sensorStates[sensorId];
        auto& sensorConfig = sensorConfigs[sensorId];
        auto currentValue = sensorValues[sensorId];
        auto now = ref_time_dbl();

        for (size_t i = 0; i < sensorConfig.levels.size(); ++i) {
            evaluateAlarmLevel(sensorId, i, currentValue, now);
        }

        sensorState.lastCheckedTime = now;
    }

    void evaluateAlarmLevel(const std::string& sensorId, size_t levelIndex, int currentValue, double now) {
        auto& state = sensorStates[sensorId].levelStates[levelIndex];
        auto& config = sensorConfigs[sensorId].levels[levelIndex];

        // Trigger alarm
        if (currentValue >= config.threshold) {
            if (!state.isActive && (now - state.lastTriggerTime >= config.onsetDelay)) {
                state.isActive = true;
                state.lastTriggerTime = now;
                triggerCallback(sensorId, levelIndex + 1);
            }
        }
        // Reset alarm
        else if (currentValue < config.threshold - config.hysteresis) {
            if (state.isActive && (now - state.lastResetTime >= config.resetDelay)) {
                state.isActive = false;
                state.lastResetTime = now;
                triggerCallback(sensorId, -(int)(levelIndex + 1));
            }
        }
    }

    void triggerCallback(const std::string& sensorId, int level) {
        if (alarmCallbacks.find(sensorId) != alarmCallbacks.end()) {
            alarmCallbacks[sensorId](sensorId, level);
        }
    }
};

void alarmAction(const std::string& sensorId, int level) {
    std::cout << "Alarm " << (level > 0 ? "triggered" : "reset")
              << " on " << sensorId << " at level " << abs(level) << std::endl;
}

int main() {
    AlarmMonitor monitor;
    AlarmMonitor::SensorConfig config;
    config.levels = {
        {100, 10, 3.0, 5.0, false},
        {150, 10, 3.0, 5.0, false},
        {200, 10, 3.0, 5.0, false}
    };
    monitor.configureSensor("TemperatureSensor1", config);
    monitor.registerCallback("TemperatureSensor1", alarmAction);

    for (int i = 0; i < 50; ++i) {
        int value = 90 + (i < 25 ? i * 10 : (50 - i) * 10);  // Simulate temperature changes
        monitor.updateSensorValue("TemperatureSensor1", value);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}
```

### Key Modifications
1. **Sensor and Alarm Configurations**: Now each sensor can have a dynamic number of alarm levels. Each level can be individually configured and evaluated.
2. **Flexibility in Adding Alarm Levels**: Additional alarm levels can be added dynamically to any sensor.
3. **Generalized Alarm Evaluation**: The alarm evaluation is now capable of handling multiple levels dynamically, checking each level based on its own thresholds and configurations.

This design allows you to adapt the monitoring system to various use cases, accommodating sensors with varying numbers of alarm levels as needed in more complex monitoring scenarios.
========================

To modify your existing `AlarmMonitor` system to support an arbitrary number \( n \) levels of alarms with a generic level handler, you will need to restructure the alarm level checks and handling logic. Below is an updated version of your code that implements this flexibility.

### Step 1: Redesign the `AlarmConfig` and `AlarmState` Structures
You need to change the `AlarmConfig` and `AlarmState` structures to support dynamic levels of alarm thresholds and states.

```cpp
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <functional>

double ref_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

class AlarmMonitor {
public:
    struct AlarmConfig {
        std::vector<int> thresholds;  // List of thresholds for each alarm level
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmState {
        std::vector<bool> isActive;  // States for each alarm level
        double lastTriggerTime = 0;
        double lastResetTime = 0;
        double lastCheckedTime = 0;
        double onsetTime = 0;
        double resetTime = 0;
        bool isLatched = false;
    };

    using AlarmCallback = std::function<void(const std::string&, int)>;

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;
    std::map<std::string, AlarmCallback> alarmCallbacks;  // Callbacks for alarms

public:
    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState();
        states[sensorId].isActive.resize(config.thresholds.size(), false);
        states[sensorId].lastCheckedTime = ref_time_dbl();
    }

    void registerCallback(const std::string& sensorId, const AlarmCallback& callback) {
        alarmCallbacks[sensorId] = callback;
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto value = values[sensorId];
        auto now = ref_time_dbl();

        // Generic handling for any number of alarm levels
        for (size_t i = 0; i < config.thresholds.size(); i++) {
            checkAndUpdateAlarm(sensorId, i, state, config, value, now);
        }
        state.lastCheckedTime = now;
    }

    void checkAndUpdateAlarm(const std::string& sensorId, size_t level, AlarmState& state, const AlarmConfig& config, int value, double now) {
        if (value >= config.thresholds[level]) {
            if (state.lastResetTime != 0 && (now - state.lastResetTime) < config.resetDelay) {
                state.lastResetTime = 0;
            }
            if (!state.isActive[level] && (now - state.lastTriggerTime) >= state.onsetTime) {
                state.isActive[level] = true;
                triggerCallback(sensorId, level + 1);  // Level is 1-based in callbacks
            }
        } else if (value < config.thresholds[level] - config.hysteresis) {
            if (state.isActive[level]) {
                if ((now - state.lastResetTime) >= state.resetTime) {
                    state.isActive[level] = false;
                    triggerCallback(sensorId, -(level + 1));  // Negative level indicates reset
                }
            }
        }
    }

    void triggerCallback(const std::string& sensorId, int level) {
        if (alarmCallbacks.find(sensorId) != alarmCallbacks.end()) {
            alarmCallbacks[sensorId](sensorId, level);
        }
    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status:";
        for (size_t i = 0; i < state.isActive.size(); i++) {
            std::cout << "\nLevel " << (i + 1) << ": " << (state.isActive[i] ? "Active" : "Inactive");
        }
        std::cout << std::endl;
    }
};

void alarmAction(const std::string& sensorId, int level) {
    if (level > 0) {
        std::cout << "Alarm triggered on " << sensorId << " at level " << level << std::endl;
    } else {
        std::cout << "Alarm reset on " << sensorId << " at level " << -level << std::endl;
    }
}

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {{100, 150, 200}, 10, 3.0, 5.0, false});
    monitor.registerCallback("TemperatureSensor1", alarmAction);
    double value = 90;
    for (int i = 0; i < 50; ++i) {
        value += (i < 25) ? 10 : -10;
        monitor.updateSensorValue("TemperatureSensor1", value);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 200ms per loop iteration
    }
    return 0;
}
```

### Key Modifications
1. **Alarm Config and State Redesign**: Both structures now support an arbitrary number of alarm levels using `std::vector`.
2. **Generalized Alarm Handling**: The `evaluateAlarm` and `checkAndUpdateAlarm` methods were modified to loop through each alarm level dynamically, enabling scalability and flexibility in handling multiple alarm levels.
3. **Usage in Main**: The configuration now simply requires listing the thresholds as a vector, allowing for easy addition or removal of levels.

This design provides a flexible and scalable solution to handle multiple alarm levels dynamically, suitable for complex systems needing versatile monitoring and alerting capabilities.


==========================

To extend the functionality of the `AlarmMonitor` class with callback functions, you can introduce mechanisms to register and execute callbacks when alarms are triggered or reset. Callbacks provide a flexible way to define custom actions without modifying the core monitoring logic.

### Adding Callback Support

Here’s how you can integrate callback functions into your `AlarmMonitor` class:

1. **Define Callback Types**: Use `std::function` for defining callback signatures.
2. **Store Callbacks**: Maintain a list of callbacks for different alarm events.
3. **Trigger Callbacks**: Execute callbacks when alarms are triggered or reset.

### Updated AlarmMonitor Class with Callbacks

```cpp
#include <iostream>
#include <map>
#include <string>
#include <chrono>
#include <thread>
#include <functional>

double ref_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

class AlarmMonitor {
public:
    struct AlarmConfig {
        int thresholdLevel1;
        int thresholdLevel2;
        int thresholdLevel3;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        double alarmDelay;  //used in level 3 to trigger 5 second alarm action 
        bool isLatched;
    };

    struct AlarmState {
        bool isAlarmLevel1 = false;
        bool isAlarmLevel2 = false;
        bool isAlarmLevel3 = false;
        double lastTriggerTime = 0;
        double lastResetTime = 0;
        double lastCheckedTime = 0;
        double onsetTime = 0;
        double resetTime = 0;
        bool isLatched = false;
    };

    using AlarmCallback = std::function<void(const std::string&, int)>;

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;
    std::map<std::string, AlarmCallback> alarmCallbacks;  // Callbacks for alarms

public:
    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState();
        states[sensorId].lastCheckedTime = ref_time_dbl(); // Initialize the checked time
    }

    void registerCallback(const std::string& sensorId, const AlarmCallback& callback) {
        alarmCallbacks[sensorId] = callback;
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto& value = values[sensorId];
        auto now = ref_time_dbl();

        // Logic to check alarm levels and trigger callbacks
        checkAndUpdateAlarm(sensorId, state, config, value, now);
    }

    void checkAndUpdateAlarm(const std::string& sensorId, AlarmState& state, const AlarmConfig& config, int value, double now) {
        bool previouslyTriggered = state.isAlarmLevel1;
        if (value >= config.thresholdLevel1) {
            if (state.lastResetTime != 0 && (now - state.lastResetTime) < config.resetDelay) {
                state.lastResetTime = 0;
            }
            if (state.lastTriggerTime == 0) {
                state.lastTriggerTime = now;
            } else if (!state.isAlarmLevel1 && (now - state.lastTriggerTime) >= state.onsetTime) {
                state.isAlarmLevel1 = true;
                triggerCallback(sensorId, 1);  // Trigger callback for alarm level 1
            }
        } else if (value < config.thresholdLevel1 - config.hysteresis) {
            if (state.isAlarmLevel1) {
                if (state.lastResetTime == 0) {
                    state.lastResetTime = now;
                } else if ((now - state.lastResetTime) >= state.resetTime) {
                    state.isAlarmLevel1 = false;
                    triggerCallback(sensorId, -1);  // Trigger callback for resetting alarm level 1
                }
            }
        }
        state.lastCheckedTime = now;
    }

    void triggerCallback(const std::string& sensorId, int level) {
        if (alarmCallbacks.find(sensorId) != alarmCallbacks.end()) {
            alarmCallbacks[sensorId](sensorId, level);
        }
    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status: "
                  << "\nLevel 1: " << (state.isAlarmLevel1 ? "Active" : "Inactive")
                  << "\nLevel 2: " << (state.isAlarmLevel2 ? "Active" : "Inactive")
                  << "\nLevel 3: " << (state.isAlarmLevel3 ? "Active" : "Inactive") << std::endl;
    }
};

void alarmAction(const std::string& sensorId, int level) {
    if (level > 0) {
        std::cout << "Alarm triggered

 on " << sensorId << " at level " << level << std::endl;
    } else {
        std::cout << "Alarm reset on " << sensorId << std::endl;
    }
}

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {100, 150, 200, 10, 3.0, 5.0, false});
    monitor.registerCallback("TemperatureSensor1", alarmAction);
    double value = 90;
    for (int i = 0; i < 50; ++i) {
        value += (i < 25) ? 10 : -10;
        monitor.updateSensorValue("TemperatureSensor1", value);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 200ms per loop iteration
    }
    return 0;
}
```

### Key Changes and Enhancements
- **Callback Registration**: Sensors can have custom callbacks registered that execute specific actions when alarms are triggered or reset.
- **Callback Execution**: When an alarm state changes, the registered callback for the sensor is executed, which can handle logging, notifications, or other actions based on alarm level changes.

This structure makes the alarm monitoring system highly flexible and capable of integrating with complex systems, enabling real-time reactions to critical sensor data changes.



==============
### Code Documentation
The provided C++ code implements an `AlarmMonitor` system designed to monitor sensor values and trigger alarms based on defined thresholds and configurations. Here is a detailed documentation of its components and functionalities:

#### Functions and Classes

- **`ref_time_dbl()`**:
  - **Purpose**: Returns the current time in seconds as a double value since the epoch of `std::chrono::steady_clock`.
  - **Returns**: `double` representing current time in seconds.

- **Class `AlarmMonitor`**:
  - Manages alarms for sensors based on configurable thresholds and hysteresis values.

  **Nested Structs**:
  - **`AlarmConfig`**:
    - Contains configuration parameters for alarms including three threshold levels, hysteresis, onset and reset delays, and a latch feature.
  - **`AlarmState`**:
    - Tracks the state of alarms including flags for each alarm level, timestamps for last trigger, reset, and check times, and onset and reset times.

  **Member Functions**:
  - **`updateSensorValue(const std::string& sensorId, int newValue)`**:
    - Updates the sensor value and evaluates if an alarm condition is met.
  - **`configureSensor(const std::string& sensorId, const AlarmConfig& config)`**:
    - Sets the initial configuration for a sensor and resets its state.
  - **`registerCallback(const std::string& sensorId, const AlarmCallback& callback)`**:
    - Registers a callback function to be called when an alarm is triggered or reset.
  - **`printStatus(const std::string& sensorId)`**:
    - Prints the current alarm status for a sensor.

- **`alarmAction(const std::string& sensorId, int level)`** (global function):
  - **Purpose**: A callback function that logs alarm activations and resets to the console.
  - **Parameters**:
    - `sensorId`: Identifier for the sensor.
    - `level`: Alarm level triggered or reset (-1 indicates reset).

#### Functional Specification




# Alarm Monitor Specification

**Document Title:** Alarm Monitor Design  
**Author:** P. Wilshire  
**Creation Date:** March 7, 2025  
**Revision Date:** March 7, 2025  
**Document Version:** 0.9  

## Change Log

| Date       | Description      | Author      |
|------------|------------------|-------------|
| 03/07/2025 | Initial creation | Phil Wilshire |

## 1. Overview

**Overview**:
- The `AlarmMonitor` class is designed to monitor various sensors, evaluating their readings against predefined thresholds to manage and trigger alarms. It supports multi-level alarms, hysteresis to prevent alarm chatter, delays for onset and reset to ensure alarms are triggered based on sustained conditions, and callbacks to handle alarm events, including extended time  triggers that takes place after the onset trigger.

**Functional Details**:
- **Sensor Configuration**:
  - Sensors are configured with thresholds for triggering different levels of alarms.
  - Each sensor can have its alarm thresholds, hysteresis, delays, and latching behavior customized.
- **Alarm Evaluation**:
  - Continuously checks sensor values against configured thresholds.
  - Uses hysteresis to determine when an alarm should reset, preventing rapid toggling of alarm states.
  - Supports delayed activation and reset to ensure alarms reflect sustained abnormal conditions.
- **Callbacks**:
  - Supports registering functions that execute when alarms initial trigger, extended trigger  or reset, allowing for external actions such as notifications or logging.

**Usage**:
- Sensors are configured with their respective thresholds and behaviors.
- Sensor values are updated periodically, and alarms are evaluated and managed accordingly.
- Callbacks for each sensor can be defined to perform actions based on alarm changes.

**Example Scenario**:
- In the main function, a sensor is configured, and its values are updated in a loop simulating real-time data changes.
- As the sensor value crosses defined thresholds, alarms are triggered or reset, and respective callbacks are executed displaying alarm status.

This system is ideal for applications requiring monitoring of environmental conditions, machinery status, or any parameter that needs to maintain within certain bounds. The use of modern C++ features like `std::map`, `std::function`, and `std::chrono` enhances the flexibility, performance, and accuracy of the system.







=======================


Sure, let's encapsulate the full code including the complete `AlarmMonitor` class with the adjustments we discussed for handling alarm onset and reset with sophisticated rules for temporary dips below threshold levels. I'll also provide a basic testing framework to simulate sensor value changes over time and see how the alarms behave.

### Full C++ Code with AlarmMonitor Class

```cpp
#include <iostream>
#include <map>
#include <string>
#include <chrono>
#include <thread>

// Simulate the current time in seconds since epoch
double ref_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

class AlarmMonitor {
public:
    struct AlarmConfig {
        int thresholdLevel1;
        int thresholdLevel2;
        int thresholdLevel3;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmState {
        bool isAlarmLevel1 = false;
        bool isAlarmLevel2 = false;
        bool isAlarmLevel3 = false;
        double lastTriggerTime = 0;
        double lastResetTime = 0;
        double lastCheckedTime = 0;
        double onsetTime = 0;
        double resetTime = 0;
        bool isLatched = false;
    };

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;

public:
    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState();
        states[sensorId].lastCheckedTime = ref_time_dbl(); // Initialize the checked time
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto& value = values[sensorId];
        auto now = ref_time_dbl();

        // Updating onset and reset handling
        if (value >= config.thresholdLevel1) {
            if (state.lastResetTime != 0 && (now - state.lastResetTime) < config.resetDelay) {
                // Extend the reset time if the value goes above the threshold again
                state.lastResetTime = 0;
            }

            if (state.lastTriggerTime == 0) {
                state.lastTriggerTime = now;
            } else if (!state.isAlarmLevel1 && (now - state.lastTriggerTime) >= state.onsetTime) {
                state.isAlarmLevel1 = true;
                std::cout << "Alarm Level 1 triggered for " << sensorId << std::endl;
            }
        } else if (value < config.thresholdLevel1 - config.hysteresis) {
            if (state.isAlarmLevel1) {
                if (state.lastResetTime == 0) {
                    state.lastResetTime = now;
                } else if ((now - state.lastResetTime) >= state.resetTime) {
                    state.isAlarmLevel1 = false;
                    state.lastResetTime = 0;
                    state.lastTriggerTime = 0;
                    std::cout << "Alarm Level 1 reset for " << sensorId << std::endl;
                }
            } else {
                // Adjust onset time for below-threshold dips
                double interval = now - state.lastCheckedTime;
                state.onsetTime = std::max(0.0, config.onsetDelay - interval);
                state.lastTriggerTime = (state.onsetTime > 0) ? now : 0;
            }
        }

        state.lastCheckedTime = now;
    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status: "
                  << "\nLevel 1: " << (state.isAlarmLevel1 ? "Active" : "Inactive")
                  << "\nLevel 2: " << (state.isAlarmLevel2 ? "Active" : "Inactive")
                  << "\nLevel 3: " << (state.isAlarmLevel3 ? "Active" : "Inactive") << std::endl;
    }
};

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {100, 150, 200, 10, 3.0, 5.0, false});
    double value = 90;
    for (int i = 0; i < 50; ++i) {
        // Simulate rapid increase and then decrease in sensor value
        value += (i < 25) ? 10 : -10;
        monitor.updateSensorValue("TemperatureSensor1", value);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 200ms per loop iteration
    }
    return 0;
}
```

### Basic Testing Setup
The `main()` function in the code provides a basic testing environment where:
- A temperature sensor's value is simulated to increase and then decrease.
- The alarm monitor is set up with specified thresholds and delays.
- Sensor values are updated every 200 milliseconds, and the status of the sensor's alarms is printed out each time.

This code gives you a comprehensive view of how alarms are triggered and reset based on the sophisticated logic you specified, including handling temporary dips and managing complex reset conditions effectively.




To properly handle the reset logic that extends the reset time when measurements temporarily exceed the trigger level after an initial fall below the threshold (with hysteresis), you need to incorporate logic that checks the conditions under which the reset time is adjusted. This involves modifying the reset mechanism to pause or extend the reset time if the value rises above the threshold again before the reset delay completes.

### Adjusted Reset Logic

Below, I'll enhance the `evaluateAlarm` method in the `AlarmMonitor` class to appropriately handle this complex reset scenario:

```cpp
void evaluateAlarm(const std::string& sensorId) {
    auto& state = states[sensorId];
    auto& config = configs[sensorId];
    auto& value = values[sensorId];
    auto now = ref_time_dbl();

    // Check for alarm level 1
    if (value >= config.thresholdLevel1) {
        // Reset the reset time if the value goes back above the threshold
        if (state.isAlarmLevel1 && state.lastResetTime != 0) {
            state.lastResetTime = 0; // Reset the reset timer
        }

        if (state.lastTriggerTime == 0) {
            state.lastTriggerTime = now;
        } else if (!state.isAlarmLevel1 && (now - state.lastTriggerTime) >= config.onsetDelay) {
            state.isAlarmLevel1 = true;
            std::cout << "Alarm Level 1 triggered for " << sensorId << std::endl;
        }
    } else if (value < config.thresholdLevel1 - config.hysteresis) {
        if (!state.isAlarmLevel1) {
            // Adjust the onset time if below the threshold but within the hysteresis range
            adjustOnsetTime(state, now, config.onsetDelay);
        } else {
            // If currently in an alarm state, manage the reset logic
            if (state.lastResetTime == 0) {
                state.lastResetTime = now; // Start reset timer
            } else if ((now - state.lastResetTime) >= config.resetDelay) {
                state.isAlarmLevel1 = false;
                state.lastResetTime = 0;
                state.lastTriggerTime = 0;
                std::cout << "Alarm Level 1 reset for " << sensorId << std::endl;
            }
        }
    } else {
        // Value is below the trigger level but not enough to start resetting (within hysteresis)
        // Extend the reset delay if value temporarily goes back above the lower threshold
        if (state.isAlarmLevel1 && state.lastResetTime != 0) {
            double interval = now - state.lastCheckedTime;
            state.resetTime -= interval;
            if (state.resetTime > 0) {
                state.lastResetTime += interval; // Extend the reset timer
            } else {
                state.lastResetTime = 0;
            }
        }
    }

    state.lastCheckedTime = now;
}

void adjustOnsetTime(AlarmState& state, double now, double onsetDelay) {
    if (state.lastTriggerTime > 0) {
        double timeBelowThreshold = now - state.lastCheckedTime;
        state.onsetTime -= timeBelowThreshold;
        if (state.onsetTime <= 0) {
            state.lastTriggerTime = 0; // Reset the trigger time as the condition was not met continuously
        } else {
            state.lastTriggerTime += timeBelowThreshold; // Extend the trigger time
        }
    }
}
```

### Explanation:
- **Reset Logic Handling**: When the value rises back above the threshold after starting a reset, the reset timer (`lastResetTime`) is reset to delay the alarm reset. This change ensures that the reset time extends if the value fluctuates above the threshold before the reset delay elapses.
- **Onset Time Adjustment**: Similar logic applies to adjusting the onset time, which ensures that the alarm is only triggered after the value has consistently been above the threshold for the specified delay.
- **Flexibility in Configuration**: This logic provides a flexible framework for configuring different alarm levels with their respective thresholds, hysteresis, onset, and reset delays, accommodating more sophisticated industrial or technical requirements.

This approach helps ensure the alarm system behaves predictably in environments with fluctuating measurements, preventing premature alarm resets and ensuring alarms are only deactivated after stable conditions are confirmed.
The provided C++ code aims to implement a sophisticated alarm monitoring system that handles temporary dips below threshold levels intelligently by adjusting the onset and reset timers. Here's an analysis and some refinements for your implementation to ensure functionality and readability:

### Key Changes Needed
1. **Fix Initialization**: Initialize all `double` members in `AlarmState` to ensure they start from a known state.
2. **Clear Alarm Handling Logic**: Simplify and clearly define the alarm handling logic, especially around the reset and onset adjustments.
3. **Bug Fixes**: Address any potential logical issues where states might conflict or where transitions might not be handled correctly.

### Refined C++ Code
Here's your code with some corrections and enhancements for clarity and function:

```cpp
#include <iostream>
#include <map>
#include <string>
#include <chrono>
#include <thread>

double ref_time_dbl() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

class AlarmMonitor {
public:
    struct AlarmConfig {
        int thresholdLevel1;
        int thresholdLevel2;
        int thresholdLevel3;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmState {
        bool isAlarmLevel1 = false;
        bool isAlarmLevel2 = false;
        bool isAlarmLevel3 = false;
        double lastTriggerTime = 0;
        double lastResetTime = 0;
        double lastCheckedTime = 0;
        double onsetTime = 0;
        double resetTime = 0;
        bool isLatched = false;
    };

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;

public:
    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState(); // Resets state upon configuration
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto& value = values[sensorId];
        auto now = ref_time_dbl();

        // Updating onset and reset handling
        if (value >= config.thresholdLevel1) {
            if (state.lastTriggerTime == 0) {
                state.lastTriggerTime = now;
            } else if (!state.isAlarmLevel1 && (now - state.lastTriggerTime) >= config.onsetDelay) {
                state.isAlarmLevel1 = true;
                std::cout << "Alarm Level 1 triggered for " << sensorId << std::endl;
            }
        } else if (state.isAlarmLevel1 && value < (config.thresholdLevel1 - config.hysteresis)) {
            if (state.lastResetTime == 0) {
                state.lastResetTime = now;
            } else if ((now - state.lastResetTime) >= config.resetDelay) {
                state.isAlarmLevel1 = false;
                state.lastResetTime = 0;
                state.lastTriggerTime = 0;
                std::cout << "Alarm Level 1 reset for " << sensorId << std::endl;
            }
        } else {
            adjustOnsetTime(state, now, config.onsetDelay);
        }

        state.lastCheckedTime = now;
    }

    void adjustOnsetTime(AlarmState& state, double now, double onsetDelay) {
        if (state.lastTriggerTime > 0) {
            double timeBelowThreshold = now - state.lastCheckedTime;
            state.onsetTime -= timeBelowThreshold;
            if (state.onsetTime <= 0) {
                state.lastTriggerTime = 0;
            } else {
                state.lastTriggerTime += timeBelowThreshold;
            }
        }
    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status: "
                  << "\nLevel 1: " << (state.isAlarmLevel1 ? "Active" : "Inactive")
                  << "\nLevel 2: " << (state.isAlarmLevel2 ? "Active" : "Inactive")
                  << "\nLevel 3: " << (state.isAlarmLevel3 ? "Active" : "Inactive") << std::endl;
    }
};

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {100, 150, 200, 10, 3.0, 5.0, false});
    for (int i = 0; i <= 400; i += 10) {
        monitor.updateSensorValue("TemperatureSensor1", i);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
```

### Key Adjustments:
1. **Initialization**: Ensures all `AlarmState` fields start from zero or false.
2. **Logic Simplification**: Clarifies the control flow and ensures all transitions are logically sound.
3. **Efficiency**: Updates are processed only as needed without redundant checks.
4. **Debugging and Testing**: The code is structured to allow easy tracking of state transitions and alarm status.

This refined implementation should effectively meet your requirement of handling alarms with advanced conditions based on thresholds and time intervals.




```cpp
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>

class AlarmMonitor {
public:
    struct AlarmConfig {
        int thresholdLevel1;
        int thresholdLevel2;
        int thresholdLevel3;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmState {
        bool isAlarmLevel1;
        bool isAlarmLevel2;
        bool isAlarmLevel3;
        double lastTriggerTime;
        double lastResetTime;
        double lastCheckedTime;
        double onsetTime;
        double resetTime;

        bool isLatched;
    };

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;

public:
    AlarmMonitor() {}

    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState{false, false, false, ref_time_dbl(), false};
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto& value = values[sensorId];
        auto now = ref_time_dbl();
     
        // Check for alarm level 1
        if (value >= config.thresholdLevel1) {
            if( state.lastTriggerTime == 0)
            {
                state.lastTriggerTime = now;
                state.onsetTime = config.onsetDelay;
            }
            if (!state.isAlarmLevel1 && (now - state.lastTriggerTime) >= config.onsetDelay) {
                state.isAlarmLevel1 = true;
                std::cout << "Alarm Level 1 triggered for " << sensorId << std::endl;
            }
            // todo pause reset time if are in alarmlevel1 and we measure above the config.threshold1
        } 
        else
        {
            if(state.isAlarmLevel1)
            {
                if((value < config.thresholdLevel1 - config.hysteresis) && (state.lastResetTime == 0))
                {
                    state.lastResetTime = now;
                    state.resetTime = config.resetDelay;
                }

                if (value < config.thresholdLevel1 - config.hysteresis) 
                {
                    if ((now - state.lastResetTime) >= config.resetDelay) {
                        state.isAlarmLevel1 = false;
                        state.lastResetTime = now;
                        state.lastTriggerTime = 0;

                        std::cout << "Alarm Level 1 reset for " << sensorId << std::endl;
                    }
                }
            }
            else
            // we are not above the trigger threshold but have seen a transition 
            // if we are still in the onset delay period then modify the last triggertime so that we get a total of onset delay above the threshold
            // if we have been below the threshold (onsetTiem == 0) then restart the onset period.
            {
                if(state.lastTriggerTime > 0)
                {
                    double interval = (now - state.lastCheckedTime);
                    state.onsetTime -= interval;
                    if (state.onsetTime < 0) 
                        state.onsetTime = 0;

                    if (state.onsetTime > 0) 
                        state.lastTriggerTime += interval;
                    else
                    state.lastTriggerTime = 0;
                }

            }
        else
        {
            // we had started to be  triggered  so move up the triger time unless the trigger adjustment == onsetDeay
        
        }

        // Similar checks can be duplicated for level 2 and level 3 with appropriate logic adjustments
        state.lastCheckedTime = now;

    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status: "
                  << "\nLevel 1: " << (state.isAlarmLevel1 ? "Active" : "Inactive")
                  << "\nLevel 2: " << (state.isAlarmLevel2 ? "Active" : "Inactive")
                  << "\nLevel 3: " << (state.isAlarmLevel3 ? "Active" : "Inactive") << std::endl;
    }
};

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {100, 150, 200, 10, 3.0, 5.0, false});
    for (int i = 0; i < 400; i += 10) {
        monitor.updateSensorValue("TemperatureSensor1", i);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}


To implement an alarm monitoring system with three levels of alarms and configurable time thresholds for alarm activation and reset, we can structure a C++ solution around an `AlarmMonitor` class. This class will handle the monitoring, checking of thresholds, and timing necessary to determine when to trigger or reset alarms based on the provided specifications.

### Key Features and Functions
1. **Alarm Levels** - Three alarm levels with different actions for each level.
2. **Alarm Duration Thresholds** - Time durations for which values must exceed or fall below certain thresholds before actions are taken.
3. **Hysteresis** - A buffer around the alarm thresholds to prevent frequent toggling of alarm states.
4. **Configurable Onset and Reset Durations** - For triggering and resetting alarms.
5. **Latched Alarms** - Once an alarm is triggered, it must be manually reset.

### C++ Class Design

We will design a class `AlarmMonitor` with methods to update sensor readings and to check and update alarm statuses.

```cpp
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>

class AlarmMonitor {
public:
    struct AlarmConfig {
        int thresholdLevel1;
        int thresholdLevel2;
        int thresholdLevel3;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        bool isLatched;
    };

    struct AlarmState {
        bool isAlarmLevel1;
        bool isAlarmLevel2;
        bool isAlarmLevel3;
        std::chrono::steady_clock::time_point lastTriggerTime;
        std::chrono::steady_clock::time_point lastResetTime;
        bool isLatched;
    };

private:
    std::map<std::string, AlarmConfig> configs;
    std::map<std::string, AlarmState> states;
    std::map<std::string, int> values;

public:
    AlarmMonitor() {}

    void updateSensorValue(const std::string& sensorId, int newValue) {
        values[sensorId] = newValue;
        evaluateAlarm(sensorId);
    }

    void configureSensor(const std::string& sensorId, const AlarmConfig& config) {
        configs[sensorId] = config;
        states[sensorId] = AlarmState{false, false, false, std::chrono::steady_clock::now(), std::chrono::steady_clock::now(), false};
    }

private:
    void evaluateAlarm(const std::string& sensorId) {
        auto& state = states[sensorId];
        auto& config = configs[sensorId];
        auto& value = values[sensorId];
        auto now = std::chrono::steady_clock::now();

        // Check for alarm level 1
        if (value >= config.thresholdLevel1) {
            if (!state.isAlarmLevel1 && std::chrono::duration_cast<std::chrono::seconds>(now - state.lastTriggerTime).count() >= config.onsetDelay) {
                state.isAlarmLevel1 = true;
                state.lastTriggerTime = now;
                std::cout << "Alarm Level 1 triggered for " << sensorId << std::endl;
            }
        } else if (state.isAlarmLevel1 && value < config.thresholdLevel1 - config.hysteresis) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - state.lastResetTime).count() >= config.resetDelay) {
                state.isAlarmLevel1 = false;
                state.lastResetTime = now;
                std::cout << "Alarm Level 1 reset for " << sensorId << std::endl;
            }
        }

        // Similar checks can be duplicated for level 2 and level 3 with appropriate logic adjustments
    }

public:
    void printStatus(const std::string& sensorId) {
        auto& state = states[sensorId];
        std::cout << "Sensor " << sensorId << " Status: "
                  << "\nLevel 1: " << (state.isAlarmLevel1 ? "Active" : "Inactive")
                  << "\nLevel 2: " << (state.isAlarmLevel2 ? "Active" : "Inactive")
                  << "\nLevel 3: " << (state.isAlarmLevel3 ? "Active" : "Inactive") << std::endl;
    }
};

int main() {
    AlarmMonitor monitor;
    monitor.configureSensor("TemperatureSensor1", {100, 150, 200, 10, 3.0, 5.0, false});
    for (int i = 0; i < 400; i += 10) {
        monitor.updateSensorValue("TemperatureSensor1", i);
        monitor.printStatus("TemperatureSensor1");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
```

### Explanation

- **Alarm Levels and Thresholds**: Each sensor has configurable thresholds for three levels of alarms and hysteresis to prevent toggling.
- **Timing**: Alarms will only trigger after the configured delay has been surpassed with the condition met continuously.
- **Alarm State**: Maintains state for each alarm level, including