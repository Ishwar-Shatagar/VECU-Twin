#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace vecu {

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG   ";
        case LogLevel::INFO:     return "INFO    ";
        case LogLevel::WARNING:  return "WARNING ";
        case LogLevel::CRITICAL: return "CRITICAL";
    }
    return "INFO    ";
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& log_file_path, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(file_mutex_);
    min_level_.store(static_cast<int>(min_level));
    if (!log_file_path.empty()) {
        log_file_.open(log_file_path, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "[Logger] WARNING: Could not open log file: " << log_file_path << "\n";
        }
    }
    initialized_ = true;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) log_file_.close();
}

std::string Logger::formatTimestamp() const {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::formatLine(LogLevel level,
                                const std::string& component,
                                const std::string& message) const {
    return "[" + formatTimestamp() + "] [" + logLevelToString(level) + "] [" +
           component + "] " + message;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (static_cast<int>(level) < min_level_.load()) return;
    std::string line = formatLine(level, component, message);

    std::lock_guard<std::mutex> lock(file_mutex_);
    std::cout << line << "\n";
    if (log_file_.is_open()) {
        log_file_ << line << "\n";
        log_file_.flush();
    }
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}
void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}
void Logger::warning(const std::string& component, const std::string& message) {
    log(LogLevel::WARNING, component, message);
}
void Logger::critical(const std::string& component, const std::string& message) {
    log(LogLevel::CRITICAL, component, message);
}

} // namespace vecu
