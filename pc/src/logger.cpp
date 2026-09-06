#include "logger.h"

namespace emg {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::init(LogLevel min_level, const std::string& log_file, bool console_output) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = min_level;
    console_ = console_output;

    if (!log_file.empty()) {
        file_.open(log_file, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "[Logger] WARNING: Could not open log file: " << log_file << "\n";
        }
    }
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (level < min_level_) return;

    std::string ts = timestamp();
    std::string lvl = levelToString(level);

    std::ostringstream line;
    line << "[" << ts << "] [" << lvl << "] [" << component << "] " << message << "\n";
    std::string formatted = line.str();

    std::lock_guard<std::mutex> lock(mutex_);
    if (console_) {
        std::cout << formatted;
        std::cout.flush();
    }
    if (file_.is_open()) {
        file_ << formatted;
        file_.flush();
    }
}

void Logger::debug(const std::string& component, const std::string& msg) {
    log(LogLevel::DEBUG, component, msg);
}

void Logger::info(const std::string& component, const std::string& msg) {
    log(LogLevel::INFO, component, msg);
}

void Logger::warn(const std::string& component, const std::string& msg) {
    log(LogLevel::WARN, component, msg);
}

void Logger::error(const std::string& component, const std::string& msg) {
    log(LogLevel::ERR, component, msg);
}

void Logger::fatal(const std::string& component, const std::string& msg) {
    log(LogLevel::FATAL, component, msg);
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "?????";
    }
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::tm bt{};
#ifdef _WIN32
    localtime_s(&bt, &timer);
#else
    localtime_r(&timer, &bt);
#endif

    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

} // namespace emg
