#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/printf.h>
#include <iostream>
#include <iterator>
#include <chrono>
#include <unordered_map>
#include <map>
#include <tuple>
extern "C" {
//#include "tmwscl/utils/tmwdiag.h"
//#include "tmwscl/utils/tmwtypes.h"
}
#include "logger/event_logger.hpp"


#define MIN_LOG_LEVEL -1
#define MAX_LOG_LEVEL 5

class EventLogger;

namespace Logging
{
    // Entry in the map storing a record of messages that have been logged
    struct RecordEntry
    {
        // Timestamp when the message was logged
        std::chrono::_V2::steady_clock::time_point timestamp;
        // Number of duplicate messages that have been captured
        uint count;
    };

    //smart pointer no mem leak
    extern EventLogger logger;
    extern std::shared_ptr<spdlog::logger> console;
    extern std::unordered_map<std::string, RecordEntry> records;

    extern bool to_file;
    extern bool to_console;
    extern std::string log_dir;
    extern std::map<spdlog::level::level_enum, std::string> severity_names; // Translation of spdlog levels to their string representations, used for logging
    extern spdlog::level::level_enum severity_threshold;                    // Log level at or above which messages will be logged
    extern std::chrono::seconds redundant_rate;                             // Minimum amount of time before logging the same message again
    extern std::chrono::minutes clear_rate;                                 // Minimum amount of time before clearing the map of messages
    extern std::chrono::_V2::steady_clock::time_point last_records_clear;   // Last time the records map was cleared

    extern std::string parse_config_path(int argc, char** argv);
    extern void Init(std::string module, int argc, const char** argv);
    extern std::vector<std::string> read_config(std::string file_path);
    extern void log_msg(spdlog::level::level_enum severity, std::string pre, std::string msg_stripped, std::string original_msg, std::string post);
//    extern void log_TMW_message(const TMWDIAG_ANLZ_ID *pAnlzId, const TMWTYPES_CHAR *pString);
    extern void log_it(std::string fileName);
    extern void log_test(spdlog::level::level_enum severity, std::string pre, std::string msg, std::string post);
    extern bool update_records(std::string msg, std::string& redundant_msg);
    extern void clear_records();

    /**
     * @brief This function removes newlines and tabs from user messages to allow for valid json and 
     * generates a single string from a variable number or arguments. Uses c-style formatting
     * 
     * @tparam Types Variable arguments that correspond with c-style formatting
     * @param format c-style format <"%s etc. %d">
     * @param args const reference to types
     * @return std::string <- single string with newlines and tabs removed.
     */
    template <class ... Types>
    std::string msg_string(std::string format, const Types&... args)
    {
        // removing newlines and tabs
        std::string formatted_str; 
        std::copy_if(format.begin(), format.end(), back_inserter(formatted_str), [](char c){return c!='\n' && c!='\t';});

        // sprintf a string
        return fmt::sprintf(formatted_str, args...);
    }

    extern std::string pre_string(const char * file, const char* func, const int line); 
    extern std::string post_string();
    extern spdlog::level::level_enum severity_to_level(int severity);
}

#define FPS_INFO_LOG(...)      ::Logging::log_msg(spdlog::level::info,  ::Logging::pre_string(__FILE__, __FUNCTION__, __LINE__), ::Logging::msg_string(__VA_ARGS__), fmt::sprintf(__VA_ARGS__), ::Logging::post_string())
#define FPS_DEBUG_LOG(...)     ::Logging::log_msg(spdlog::level::debug, ::Logging::pre_string(__FILE__, __FUNCTION__, __LINE__), ::Logging::msg_string(__VA_ARGS__), fmt::sprintf(__VA_ARGS__), ::Logging::post_string())
#define FPS_WARNING_LOG(...)   ::Logging::log_msg(spdlog::level::warn,  ::Logging::pre_string(__FILE__, __FUNCTION__, __LINE__), ::Logging::msg_string(__VA_ARGS__), fmt::sprintf(__VA_ARGS__), ::Logging::post_string())
#define FPS_ERROR_LOG(...)     ::Logging::log_msg(spdlog::level::err,   ::Logging::pre_string(__FILE__, __FUNCTION__, __LINE__), ::Logging::msg_string(__VA_ARGS__), fmt::sprintf(__VA_ARGS__), ::Logging::post_string())
#define FPS_TEST_LOG(...)      ::Logging::log_test(spdlog::level::err,  ::Logging::pre_string(__FILE__, __FUNCTION__, __LINE__), ::Logging::msg_string(__VA_ARGS__), fmt::sprintf(__VA_ARGS__), ::Logging::post_string())
#define FPS_LOG_IT(fileName)   ::Logging::log_it(fileName)