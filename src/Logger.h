#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <vector>

enum class LogLevel {
    INFO,
    WARNING,
    CRITICAL
};

class Logger {
public:
    static Logger& getInstance();

    void init(const std::string& logFilePath = "logs/processshield.log");
    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void critical(const std::string& message);

    std::vector<std::string> getRecentLogs(size_t maxLines = 25) const;
    std::string getLogFilePath() const;

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) const;
    std::string currentTimestamp() const;

    std::string logFilePath_;
    mutable std::mutex logMutex_;
    std::ofstream logFile_;
};

#endif // LOGGER_H
