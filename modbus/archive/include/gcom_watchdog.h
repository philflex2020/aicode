#pragma once

#include <chrono>
#include <iostream>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <mutex>


//#include "tmwscl/utils/tmwsim.h"
//#include "gcom_fims.h"

struct GcomSystem;

struct GcomSystem 
{
    std::string id;
};

struct GCOM_POINT 
{
    std::string id;
};


enum class WatchdogState
{
    Fault,
    Warning,
    Recovery,
    Normal
};

// Mock callback functions for testing
void warningCallback(const std::string &name, int timeSinceLastSeen, GcomSystem &sys);
void faultCallback(const std::string &name, int timeSinceLastSeen, GcomSystem &sys);
void recoveryCallback(const std::string &name, void *data, GcomSystem &sys);
void normalCallback(const std::string &name, void *data, GcomSystem &sys);
typedef void (*WATCHDOG_CALLBACK)(
    void *pCallbackParam);

struct VariableState
{
    std::string name;
    double previousValue;
    GCOM_POINT *dbPoint;
    WatchdogState state;
    std::chrono::high_resolution_clock::time_point lastChangeTime;
    std::chrono::high_resolution_clock::time_point secondChangeTime;
    int64_t warningTimeout;
    int64_t faultTimeout;
    int64_t recoveryTimeout;
};

class Watchdog
{
public:
    struct WatchdogParams
    {
        std::string name;
        int warningTime;  // in milliseconds
        int faultTime;    // in milliseconds
        int recoveryTime; // in milliseconds
        std::function<void(const std::string &, int, GcomSystem &)> warningCallback;
        std::function<void(const std::string &, int, GcomSystem &)> faultCallback;
        std::function<void(const std::string &, void *, GcomSystem &)> recoveryCallback;
        std::function<void(const std::string &, void *, GcomSystem &)> normalCallback;
    };

    Watchdog();

    VariableState *addVariable(const WatchdogParams &params, std::string name, GCOM_POINT *dbPoint);
    void setTime(WatchdogParams &params, int warning, int fault, int recovery);
    void setName(WatchdogParams &params, std::string name);
    void setCallbacks(WatchdogParams &params, std::function<void(const std::string &, int, GcomSystem &)>warning, std::function<void(const std::string &, int, GcomSystem &)>fault, std::function<void(const std::string &, void *, GcomSystem &)>recovery);
    void start(GcomSystem &sys);
    void stop();

    void setInput(const std::string &name, double value);

public:
    std::vector<VariableState> variables;
    std::mutex lock;
    std::thread watchdogThread;
    std::thread publishThread;
    VariableState varState;

    bool monitorLoop(GcomSystem &sys);
    bool publishState(GcomSystem &sys);
};

template<>
struct fmt::formatter<WatchdogState>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const WatchdogState& state, FormatContext& ctx) const
    {
        switch (state)
        {
        case WatchdogState::Fault: 
            return fmt::format_to(ctx.out(), "Fault");

        case WatchdogState::Recovery:
            return fmt::format_to(ctx.out(), "Recovery");

        case WatchdogState::Warning:
            return fmt::format_to(ctx.out(), "Warning");

        case WatchdogState::Normal:
            return fmt::format_to(ctx.out(), "Normal");
        }
        return fmt::format_to(ctx.out(), "");
    }
};

