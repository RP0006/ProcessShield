#ifndef RESOURCEMONITOR_H
#define RESOURCEMONITOR_H

#include "ProcessManager.h"
#include "CPUAnalyzer.h"
#include "MemoryAnalyzer.h"

struct SystemStats {
    double totalCPU;
    double totalMemoryMB;
    size_t activeProcesses;
    size_t activeAlerts;
};

class ResourceMonitor {
public:
    ResourceMonitor(double cpuThreshold = 70.0, double memThresholdMB = 500.0, int refreshIntervalSec = 5);

    void setCPUThreshold(double percent);
    double getCPUThreshold() const;

    void setMemoryThresholdMB(double mb);
    double getMemoryThresholdMB() const;

    void setRefreshIntervalSec(int sec);
    int getRefreshIntervalSec() const;

    bool refresh();
    SystemStats getSystemStats() const;

    void runCPUAnalysis(bool triggerAlerts = true);
    void runMemoryAnalysis(bool triggerAlerts = true);
    void displayRunningProcesses(size_t limit = 25) const;

    ProcessManager& getProcessManager();
    const ProcessManager& getProcessManager() const;

private:
    double cpuThreshold_;
    double memThresholdMB_;
    int refreshIntervalSec_;

    ProcessManager processManager_;
    CPUAnalyzer cpuAnalyzer_;
    MemoryAnalyzer memoryAnalyzer_;
};

#endif // RESOURCEMONITOR_H
