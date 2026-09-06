#pragma once
/// @file logger.h
/// @brief Thread-safe logger with severity levels and timestamped output.

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

#ifdef ERROR
#undef ERROR
#endif

namespace emg {

enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERR   = 3,
    FATAL = 4
};

class Logger {
public:
    static Logger& instance();

    /// Initialize logger with a minimum level and optional file output.
    void init(LogLevel min_level = LogLevel::INFO,
              const std::string& log_file = "",
              bool console_output = true);

    /// Log a message at the given level.
    void log(LogLevel level, const std::string& component, const std::string& message);

    // Convenience methods
    void debug(const std::string& component, const std::string& msg);
    void info(const std::string& component, const std::string& msg);
    void warn(const std::string& component, const std::string& msg);
    void error(const std::string& component, const std::string& msg);
    void fatal(const std::string& component, const std::string& msg);

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) const;
    std::string timestamp() const;

    std::mutex      mutex_;
    LogLevel        min_level_ = LogLevel::INFO;
    bool            console_ = true;
    std::ofstream   file_;
};

// Macros for convenient logging with automatic component name
#define LOG_DEBUG(component, msg) ::emg::Logger::instance().debug(component, msg)
#define LOG_INFO(component, msg)  ::emg::Logger::instance().info(component, msg)
#define LOG_WARN(component, msg)  ::emg::Logger::instance().warn(component, msg)
#define LOG_ERROR(component, msg) ::emg::Logger::instance().error(component, msg)
#define LOG_FATAL(component, msg) ::emg::Logger::instance().fatal(component, msg)

} // namespace emg
