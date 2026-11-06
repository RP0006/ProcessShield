#ifndef CPUANALYZER_H
#define CPUANALYZER_H

#include <vector>
#include <string>
#include "ProcessManager.h"

struct CPUViolation {
    pid_t pid;
    std::string processName;
    double cpuPercent;
    double threshold;
};

class CPUAnalyzer {
public:
    explicit CPUAnalyzer(double thresholdPercent = 70.0);

    void setThreshold(double thresholdPercent);
    double getThreshold() const;

    std::vector<CPUViolation> analyze(const std::vector<ProcessInfo>& processes, bool triggerAlerts = true);
    void printReport(const std::vector<ProcessInfo>& processes, size_t topN = 10);

private:
    double thresholdPercent_;
};

#endif // CPUANALYZER_H
