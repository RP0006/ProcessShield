#include "AlertManager.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

AlertManager& AlertManager::getInstance() {
    static AlertManager instance;
    return instance;
}

AlertManager::AlertManager() = default;

std::string AlertManager::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&now_c, &tm_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string AlertManager::levelToString(AlertLevel level) {
    switch (level) {
        case AlertLevel::INFO: return "INFO";
        case AlertLevel::WARNING: return "WARNING";
        case AlertLevel::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

void AlertManager::addAlert(AlertLevel level, const std::string& source, const std::string& message, const std::string& details) {
    std::lock_guard<std::mutex> lock(alertMutex_);
    std::string ts = currentTimestamp();
    Alert alert{ts, level, source, message, details};
    alerts_.push_back(alert);

    // Forward to Logger
    LogLevel logLvl = LogLevel::INFO;
    if (level == AlertLevel::WARNING) logLvl = LogLevel::WARNING;
    else if (level == AlertLevel::CRITICAL) logLvl = LogLevel::CRITICAL;

    std::string logMsg = "[" + source + "] " + message;
    if (!details.empty()) {
        logMsg += " (" + details + ")";
    }
    Logger::getInstance().log(logLvl, logMsg);
}

std::vector<Alert> AlertManager::getRecentAlerts(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(alertMutex_);
    if (alerts_.size() <= maxCount) {
        return alerts_;
    }
    return std::vector<Alert>(alerts_.end() - maxCount, alerts_.end());
}

std::vector<Alert> AlertManager::getAllAlerts() const {
    std::lock_guard<std::mutex> lock(alertMutex_);
    return alerts_;
}

size_t AlertManager::getAlertCount() const {
    std::lock_guard<std::mutex> lock(alertMutex_);
    return alerts_.size();
}

size_t AlertManager::getActiveWarningOrCriticalCount() const {
    std::lock_guard<std::mutex> lock(alertMutex_);
    size_t count = 0;
    for (const auto& alert : alerts_) {
        if (alert.level == AlertLevel::WARNING || alert.level == AlertLevel::CRITICAL) {
            count++;
        }
    }
    return count;
}

void AlertManager::clearAlerts() {
    std::lock_guard<std::mutex> lock(alertMutex_);
    alerts_.clear();
    Logger::getInstance().info("AlertManager: All active alerts cleared");
}
