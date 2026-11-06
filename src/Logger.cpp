#include "Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : logFilePath_("logs/processshield.log") {
    // Ensure logs directory exists
    mkdir("logs", 0755);
    init(logFilePath_);
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(logMutex_);
    logFilePath_ = logFilePath;
    if (logFile_.is_open()) {
        logFile_.close();
    }
    logFile_.open(logFilePath_, std::ios::out | std::ios::app);
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::string Logger::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&now_c, &tm_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    std::string timeStr = currentTimestamp();
    std::string levelStr = levelToString(level);
    std::string formatted = "[" + timeStr + "] " + levelStr + ": " + message;

    if (logFile_.is_open()) {
        logFile_ << formatted << std::endl;
        logFile_.flush();
    }
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::critical(const std::string& message) {
    log(LogLevel::CRITICAL, message);
}

std::vector<std::string> Logger::getRecentLogs(size_t maxLines) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    std::vector<std::string> lines;
    std::ifstream file(logFilePath_);
    if (!file.is_open()) {
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (lines.size() > maxLines) {
        return std::vector<std::string>(lines.end() - maxLines, lines.end());
    }
    return lines;
}

std::string Logger::getLogFilePath() const {
    return logFilePath_;
}
