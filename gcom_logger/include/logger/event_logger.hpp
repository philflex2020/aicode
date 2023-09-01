#ifndef EVENT_LOGGER_HPP
#define EVENT_LOGGER_HPP

#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/details/file_helper.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/details/synchronous_factory.h"
#include "spdlog/sinks/ringbuffer_sink.h"

#include "spdlog/common.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/chrono.h"

#include "logger/circular_msg_q.hpp" // important, do NOT remove

#include <chrono>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <ratio>

// todo: get event type for this calc as well?:
struct event_time_filename_calculator
{
    static spdlog::filename_t calc_filename(const spdlog::filename_t& filename, bool add_timestamp)
    {
        auto now = spdlog::log_clock::now();
        spdlog::filename_t folder_path, basename, ext;
        std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(filename);
        auto index = basename.find_last_of("/");
        if (index == basename.npos)
        {
            if (!add_timestamp)
            {
                return fmt::format(SPDLOG_FILENAME_T("{}{}"), basename, ext);
                
            }
            else // we want to add a timestamp to it:
            {
                return fmt::format(
                SPDLOG_FILENAME_T("{:%F}_{:%H:%M:%S}_{}{}"), 
                    now, now.time_since_epoch() + std::chrono::hours(20), basename, ext);
            }
        }
        folder_path = basename.substr(0, index + 1);
        basename = basename.substr(index + 1);
        if (!add_timestamp)
        {
            return fmt::format(SPDLOG_FILENAME_T("{}{}{}"), folder_path, basename, ext);
        }
        else
        {
            return fmt::format(
                SPDLOG_FILENAME_T("{}{:%F}_{:%H:%M:%S}_{}{}"), 
                    folder_path, now, now.time_since_epoch() + std::chrono::hours(20), basename, ext);
        }
    }
};

//
// Rotating event sink based on date (possibly event type as well -> will look into this)
// todo: put "bucket" (actual backtrace) here so any logger can just sink_it to here auto magically.
//
template<typename Mutex, typename FileNameCalc = event_time_filename_calculator>
class rotating_event_sink final : public spdlog::sinks::base_sink<Mutex>
{
    spdlog::details::file_helper file_helper_;
    circular_msg_q buf_;

public:
    rotating_event_sink(std::size_t bufSize)
        : buf_(bufSize)
    {}

    void set_backtrace(std::size_t size)
    {
        buf_.reSize(size);
    }

    // actually dumps current messages to file
    void log_it_(const spdlog::filename_t& fileName, bool add_timestamp = false)
    {
        rotate_(fileName, add_timestamp);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf_.newest());
    }

    // flushing just consists of dumping the backtrace logs into the file.
    // todo: default file name is not setup right now, maybe maybe will include this feature.
    void flush_() override
    {
        // rotate_();
        return;
    }

private:
    // Rotate file:
    // log.txt -> name_(date)(.ext -> optional)
    void rotate_(const spdlog::filename_t& fileName, bool add_timestamp = false)
    {
        auto newFile = FileNameCalc::calc_filename(fileName, add_timestamp);
        file_helper_.open(newFile);

//         spdlog::memory_buf_t begin_brace, end_brace;
//         begin_brace.append(std::string(R"({
//     "logs": [
// )"));
//         end_brace.append(std::string("    ]\n}"));

        // file_helper_.write(begin_brace);
        buf_.dumpToFile(file_helper_);
        // file_helper_.write(end_brace);
        file_helper_.close(); // Must be called before file has been officially written to.
    }
};

// aliases:
using rotating_event_sink_mt = rotating_event_sink<std::mutex>;
using rotating_event_sink_st = rotating_event_sink<spdlog::details::null_mutex>;

class EventLogger final
{
    std::shared_ptr<rotating_event_sink_st> mSink;
    spdlog::logger mLogger;

public:
    EventLogger(const char* name, std::size_t size)
        : mSink(std::make_shared<rotating_event_sink_st>(size)), mLogger(name, mSink)
    {
        mLogger.set_level(spdlog::level::debug); // so we can log debug messages
    }

    void setBacktrace(std::size_t size)
    {
        mSink->set_backtrace(size);
    }

    // neat trick for flushing the previous logs and starting fresh, almost no performance cost.
    // NOTE: it is your responsibility to set size > 0 and not flush other people's messages unecessarily.
    void resetBacktrace(std::size_t size)
    {
        mSink->set_backtrace(0); // flush all previous messages
        mSink->set_backtrace(size); // set to new size. start fresh
    }

    // call this function yourself to log the stored messages when needed.
    void logIt(const spdlog::filename_t& fileName, bool add_timestamp = false)
    {
        mSink->log_it_(fileName, add_timestamp);
    }

    // Might change this to have set pattern take a template parameter?
    // might improve efficiency and allow for string_views?
    // probably needs to own it though, so this should be good enough.
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    void setPattern(std::string pattern, spdlog::pattern_time_type time_type = spdlog::pattern_time_type::local)
    {
        mLogger.set_pattern(std::move(pattern), time_type);
    }

    void set_level(spdlog::level::level_enum severity)
    {
        mLogger.set_level(severity);
    }

    void flush_on(spdlog::level::level_enum severity)
    {
        mLogger.flush_on(severity);
    }

    // for if you want your own custom formatting flags
    void setFormatter(std::unique_ptr<spdlog::formatter> formatter)
    {
        mLogger.set_formatter(std::move(formatter));
    }

    template<typename... Args>
    void log(spdlog::level::level_enum severity, spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.log(severity, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        mLogger.critical(fmt, std::forward<Args>(args)...);
    }
};

#endif