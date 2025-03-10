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
using AlarmCallback = std::function<void(const std::string&, int)>;

public:
    struct AlarmLevelConfig {
        int threshold;
        int hysteresis;
        double onsetDelay;
        double resetDelay;
        double actionDelay;
        bool isLatched;
        AlarmCallback onsetCallback;
        AlarmCallback resetCallback;
        AlarmCallback actionCallback;

    };

    struct AlarmLevelState {
        bool isTriggering = false;
        bool isResetting = false;
        bool isActive = false;
        bool actionTriggered = false;
        double lastTriggerTime = 0;
        double lastResetTime = 0;
        double lastTriggerDelay = 0;
        double lastResetDelay = 0;
        double lastSeenTime = 0;
    };

    struct SensorConfig {
        std::vector<AlarmLevelConfig> levels;
    };

    struct SensorState {
        std::vector<AlarmLevelState> levelStates;
        double lastCheckedTime = 0;
    };


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
        //std::cout << " levelIndex " << levelIndex << " currentValue :" << currentValue << " Threshold :" << config.threshold << std::endl;   
        if (currentValue >= config.threshold) {
            // pause reset
            if (state.isResetting) {
                state.lastResetTime += (now - state.lastSeenTime);
            }
            else
            {
                // start trigger timing
                if (!state.isTriggering ) {
                    if(!state.isActive)
                    {
                        std::cout << "         set state to triggering " << std::endl;
                        state.isTriggering = true;
                        state.lastTriggerTime = now;
                        state.lastTriggerDelay = config.onsetDelay;
                    }
                }
                // see if trigger past threshold]
                if (!state.isActive && (now - state.lastTriggerTime >= state.lastTriggerDelay)) {
                    std::cout << "         set state to active " << std::endl;
                    state.isActive = true;
                    state.isResetting = false;
                    state.isTriggering = false;
                    state.actionTriggered  = false;
                    config.onsetCallback(sensorId, levelIndex + 1);
                }
                if (state.isActive && !state.actionTriggered && (now - state.lastTriggerTime >= config.actionDelay)) {
                    state.actionTriggered  = true;
                    config.actionCallback(sensorId, levelIndex + 1);
                }

            }
        }
        // Reset alarm
        else if (currentValue < config.threshold - config.hysteresis) {
            //pause trigger
            if (state.isTriggering ) {
                state.lastTriggerTime += (now - state.lastSeenTime);
            }
            else
            { 
                if (!state.isResetting) {
                    if(state.isActive)
                    {
                        std::cout << "         set state to resetting " << std::endl;

                        state.isResetting = true;
                        state.lastResetTime = now;
                        state.lastResetDelay = config.resetDelay;
                    }
                }
                if (state.isActive && (now - state.lastResetTime >= state.lastResetDelay)) {
                    std::cout << "         set state to inactive " << std::endl;
                    state.isActive = false;
                    state.isTriggering = false;
                    state.isResetting = false;
                    config.resetCallback(sensorId, -(int)(levelIndex + 1));
                }
            }
        }
        state.lastSeenTime = now;

    }

    void triggerCallback(const std::string& sensorId, int level) {
        if (alarmCallbacks.find(sensorId) != alarmCallbacks.end()) {
            alarmCallbacks[sensorId](sensorId, level);
        }
    }
};

void alarmAction(const std::string& sensorId, int level) {
    std::cout << "Alarm Action " 
              << " on " << sensorId << " at level " << abs(level) << std::endl;
}

void onsetAction(const std::string& sensorId, int level) {
    std::cout << "Alarm Set " 
              << " on " << sensorId << " at level " << abs(level) << std::endl;
}

void resetAction(const std::string& sensorId, int level) {
    std::cout << "Alarm reset " 
              << " on " << sensorId << " at level " << abs(level) << std::endl;
}

int main() {
    AlarmMonitor monitor;
    AlarmMonitor::SensorConfig config;
            int threshold; int hysteresis; double onsetDelay; double resetDelay; bool isLatched;

    config.levels = {
        {100, 10, 1.0, 1.0, 5.0, false, onsetAction, resetAction, alarmAction},
        {150, 10, 1.0, 1.0, 5.0, false, onsetAction, resetAction, alarmAction},
        {200, 10, 1.0, 1.0, 5.0, false, onsetAction, resetAction, alarmAction}
    };
    monitor.configureSensor("TemperatureSensor1", config);
    monitor.registerCallback("TemperatureSensor1", alarmAction);

    double start = ref_time_dbl();
    for (int i = 0; i < 300; i+=10) {
        int value = i;
        std::cout << " time "<< ref_time_dbl() - start << "  value " << value << std::endl;
        // + (i < 25 ? i * 10 : (50 - i) * 10);  // Simulate temperature changes
        monitor.updateSensorValue("TemperatureSensor1", value);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    for (int i = 300; i > 0;  i -=10) {
        int value = i;
        std::cout << " time "<< ref_time_dbl() - start << "  value " << value << std::endl;
        // + (i < 25 ? i * 10 : (50 - i) * 10);  // Simulate temperature changes
        monitor.updateSensorValue("TemperatureSensor1", value);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return 0;
}