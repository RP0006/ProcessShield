#ifndef MEMORYANALYZER_H
#define MEMORYANALYZER_H

#include <vector>
#include <string>
#include "ProcessManager.h"

struct MemoryViolation {
    pid_t pid;
    std::string processName;
    double memoryMB;
    double thresholdMB;
};

class MemoryAnalyzer {
public:
    explicit MemoryAnalyzer(double thresholdMB = 500.0);

    void setThreshold(double thresholdMB);
    double getThreshold() const;

    std::vector<MemoryViolation> analyze(const std::vector<ProcessInfo>& processes, bool triggerAlerts = true);
    void printReport(const std::vector<ProcessInfo>& processes, size_t topN = 10);

private:
    double thresholdMB_;
};

#endif // MEMORYANALYZER_H
