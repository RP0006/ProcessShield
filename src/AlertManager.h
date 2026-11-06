#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include "Logger.h"

enum class AlertLevel {
    INFO,
    WARNING,
    CRITICAL
};

struct Alert {
    std::string timestamp;
    AlertLevel level;
    std::string source;
    std::string message;
    std::string details;
};

class AlertManager {
public:
    static AlertManager& getInstance();

    void addAlert(AlertLevel level, const std::string& source, const std::string& message, const std::string& details = "");
    std::vector<Alert> getRecentAlerts(size_t maxCount = 20) const;
    std::vector<Alert> getAllAlerts() const;
    size_t getAlertCount() const;
    size_t getActiveWarningOrCriticalCount() const;
    void clearAlerts();

    static std::string levelToString(AlertLevel level);

private:
    AlertManager();
    ~AlertManager() = default;
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;

    std::string currentTimestamp() const;

    mutable std::mutex alertMutex_;
    std::vector<Alert> alerts_;
};

#endif // ALERTMANAGER_H
