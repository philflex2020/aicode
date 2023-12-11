#include "gcom_watchdog.h"

// extern "C" {
//     #include "tmwscl/utils/tmwsesn.h"
// }
// #include "system_structs.h"
#include "logger/logger.h"

// Client event source constant:

void warningCallback(const std::string &name, int timeSinceLastSeen, GcomSystem &sys)
{
    if (sys.debug > 0)
        FPS_WARNING_LOG("[%s] has entered warning state. Value last changed [%d ms] ago.\n", name.c_str(), timeSinceLastSeen);
    else
    {
        fmt::memory_buffer str_buf;
        emit_event(str_buf, sys.fims_dependencies->fims_gateway, sys.watch.status_uri, FimsEventSeverity::Info, R"(INFO: \"{}\")", name);
    }
}
void faultCallback(const std::string &name, int timeSinceLastSeen, GcomSystem &sys)
{
    if (sys.debug > 0)
        FPS_ERROR_LOG("[%s] has entered fault state. Value last changed [%d ms] ago.\n", name.c_str(), timeSinceLastSeen);
    else
    {
        fmt::memory_buffer str_buf;
        emit_event(str_buf, sys.fims_dependencies->fims_gateway, sys.watch.status_uri, FimsEventSeverity::Info, R"(INFO: \"{}\")", name);
    }
}
void recoveryCallback(const std::string &name, void *data, GcomSystem &sys)
{
    if (sys.debug > 0)
        FPS_INFO_LOG("[%s] has entered recovery state\n", name.c_str());
    else
    {
        fmt::memory_buffer str_buf;
        emit_event(str_buf, sys.fims_dependencies->fims_gateway, sys.watch.status_uri, FimsEventSeverity::Info, R"(INFO: \"{}\")", name);
    }
}
void normalCallback(const std::string &name, void *data, GcomSystem &sys)
{
    if (sys.debug > 0)
        FPS_INFO_LOG("[%s] has entered online state\n", name.c_str());
    else
    {
        fmt::memory_buffer str_buf;
        emit_event(str_buf, sys.fims_dependencies->fims_gateway, sys.watch.status_uri, FimsEventSeverity::Info, R"(INFO: \"{}\")", name);
    }
}

// used when initializing a watchdog variable
void Watchdog::setTime(WatchdogParams &param, int warningTime, int faultTime, int recoveryTime)
{
    param.warningTime = warningTime;
    param.faultTime = faultTime;
    param.recoveryTime = recoveryTime;
}

// used when initializing a watchdog variable
void Watchdog::setName(WatchdogParams &param, std::string name)
{
    param.name = name;
}

Watchdog::Watchdog() {}

VariableState *Watchdog::addVariable(const WatchdogParams &params, std::string name, TMWSIM_POINT *dbPoint)
{
    VariableState *varState = new VariableState;
    varState->name = name;
    varState->dbPoint = dbPoint;
    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
        varState->previousValue = dbPoint->data.analog.value;
    else if (dbPoint->type == TMWSIM_TYPE_BINARY)
        varState->previousValue = static_cast<double>(dbPoint->data.binary.value);
    varState->state = WatchdogState::Normal;

    // default timeouts
    varState->warningTimeout = params.warningTime;
    varState->faultTimeout = params.faultTime;
    varState->recoveryTimeout = params.recoveryTime;

    variables.push_back(*varState);
    return varState;
}

void Watchdog::start(GcomSystem &sys)
{
    for (auto &var : variables)
    {
        var.lastChangeTime = std::chrono::high_resolution_clock::now();
    }
    watchdogThread = std::thread(&Watchdog::monitorLoop, this, std::ref(sys));
    publishThread = std::thread(&Watchdog::publishState, this, std::ref(sys));
}

void Watchdog::stop()
{
    watchdogThread.join();
    publishThread.join();
}

void Watchdog::setInput(const std::string &name, double value)
{
    for (auto &var : variables)
    {
        if (var.name.compare(name))
        {
            if (var.previousValue != value)
            {
                lock.lock();
                var.previousValue = value;
                var.secondChangeTime = var.lastChangeTime;
                var.lastChangeTime = std::chrono::high_resolution_clock::now();
                lock.unlock();
            }
            break;
        }
    }
}

bool Watchdog::publishState(GcomSystem &sys)
{
    while (((TMWSESN *)sys.protocol_dependencies->dnp3.pSession)->online == TMWSESN_STAT_ONLINE)
    {
        fmt::memory_buffer send_buf;
        send_buf.push_back('{');
        for (auto &var : variables)
        {
            FORMAT_TO_BUF(send_buf, R"("{}": "{}",)", var.name, var.state);
        }
        const auto timestamp = std::chrono::system_clock::now();
        const auto timestamp_micro = time_fraction<std::chrono::microseconds>(timestamp);
        FORMAT_TO_BUF_NO_COMPILE(send_buf, R"("Timestamp":"{:%Y-%m-%d %T}.{:06d}"}})", timestamp, timestamp_micro.count());
        if (!send_pub(sys.fims_dependencies->fims_gateway, sys.watch.status_uri, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("{} cannot publish onto fims, exiting\n", sys.fims_dependencies->name.c_str());
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sys.watch.freq));
    }
    return true;
}

bool Watchdog::monitorLoop(GcomSystem &sys)
{
        while (((TMWSESN *)sys.protocol_dependencies->dnp3.pSession)->online == TMWSESN_STAT_ONLINE)
        {
            for (auto &var : variables)
            {
                lock.lock();
                auto now = std::chrono::high_resolution_clock::now();
                auto timeSinceLastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(now - var.lastChangeTime).count();
                auto timeBetweenChanges = std::chrono::duration_cast<std::chrono::milliseconds>(var.lastChangeTime - var.secondChangeTime).count();
                lock.unlock();

                if ((var.state == WatchdogState::Normal || var.state == WatchdogState::Recovery) && timeSinceLastSeen >= var.warningTimeout)
                {
                    var.state = WatchdogState::Warning;
                    warningCallback(var.name, timeSinceLastSeen, std::ref(sys));
                }
                else if (var.state == WatchdogState::Warning && timeSinceLastSeen >= var.faultTimeout + var.warningTimeout) // NOTE: Do we want to change this timout scheme?
                {
                    var.state = WatchdogState::Fault;
                    faultCallback(var.name, timeSinceLastSeen, std::ref(sys));
                }
                else if ((var.state == WatchdogState::Warning || var.state == WatchdogState::Fault) && timeBetweenChanges <= var.recoveryTimeout)
                {
                    var.state = WatchdogState::Recovery;
                    recoveryCallback(var.name, nullptr, std::ref(sys));
                }
                else if (var.state == WatchdogState::Recovery && timeBetweenChanges <= var.recoveryTimeout)
                {
                    var.state = WatchdogState::Normal;
                    normalCallback(var.name, nullptr, std::ref(sys));
                }
            }
            // Sleep for a short interval to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return true;
}