#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace vecu {

enum class LogLevel { DEBUG, INFO, WARNING, CRITICAL };

std::string logLevelToString(LogLevel level);

/**
 * @brief Thread-safe structured logger.
 *
 * Writes structured log lines to both stdout and an optional log file.
 * All methods are safe to call from multiple threads simultaneously.
 *
 * Log format:
 *   [2024-01-15T10:23:45.123] [INFO   ] [EngineECU] Engine started at idle
 *
 * RAII: the file is opened in the constructor and closed in the destructor.
 */
class Logger {
public:
    /// Get the singleton logger instance
    static Logger& instance();

    /// Initialize with a log file path (call once at startup)
    void init(const std::string& log_file_path, LogLevel min_level = LogLevel::INFO);

    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warning(const std::string& component, const std::string& message);
    void critical(const std::string& component, const std::string& message);

    void log(LogLevel level, const std::string& component, const std::string& message);

    void setMinLevel(LogLevel level) { min_level_.store(level); }

private:
    Logger() = default;
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string formatTimestamp() const;
    std::string formatLine(LogLevel level, const std::string& component, const std::string& message) const;

    mutable std::mutex file_mutex_;
    std::ofstream      log_file_;
    std::atomic<int>   min_level_{static_cast<int>(LogLevel::INFO)};
    bool               initialized_{false};
};

// Convenience macros
#define LOG_INFO(component, msg)     vecu::Logger::instance().info(component, msg)
#define LOG_WARN(component, msg)     vecu::Logger::instance().warning(component, msg)
#define LOG_CRITICAL(component, msg) vecu::Logger::instance().critical(component, msg)
#define LOG_DEBUG(component, msg)    vecu::Logger::instance().debug(component, msg)

} // namespace vecu
