/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_LOGGER_H
#define EPICS_DIODE_LOGGER_H

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <string>

#include <epicsTime.h>


namespace epics_diode {

struct LogLevel {
    enum Level : uint32_t {
        Trace = 0,
        Debug,
        Config,
        Info,
        Warning,
        Error
    };

    static Level from_verbosity(int debug_level) {
        return Level(std::max(int(Trace), int(Info) - debug_level));
    }
};

struct Logger {

    explicit Logger(const std::string &name) :
        name(name),
        log_level(default_log_level) {
    }
    
    inline void set_log_level(LogLevel::Level level) {
        log_level = level;
    }

    inline bool is_loggable(LogLevel::Level level) {
        return level >= log_level;
    }

    void log(epics_diode::LogLevel::Level level, const char* msg)
    {
        if (Logger::is_loggable(level))
        {
            std::array<char, 32> timeText{};
            epicsTimeStamp tsNow;

            epicsTimeGetCurrent(&tsNow);
            epicsTimeToStrftime(timeText.begin(), timeText.size(), "%Y-%m-%dT%H:%M:%S.%03f", &tsNow);

            printf("%s [%s] %s\n", timeText.begin(), name.c_str(), msg);
        }
    }    

    template<typename... Ts>
    void log(epics_diode::LogLevel::Level level, const char* format, Ts... args)
    {
        if (Logger::is_loggable(level))
        {
            std::array<char, 32> timeText{};
            epicsTimeStamp tsNow;

            epicsTimeGetCurrent(&tsNow);
            epicsTimeToStrftime(timeText.begin(), timeText.size(), "%Y-%m-%dT%H:%M:%S.%03f", &tsNow);

            printf("%s [%s] ", timeText.begin(), name.c_str());
            printf(format, args...);
            printf("\n");
        }
    }    

    static inline void set_default_log_level(LogLevel::Level level) {
        default_log_level = level;
    }

protected:
    std::string name;
    LogLevel::Level log_level;

    static LogLevel::Level default_log_level;
};

}

#endif