#include "ResourceMonitor.h"
#include "AlertManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

ResourceMonitor::ResourceMonitor(double cpuThreshold, double memThresholdMB, int refreshIntervalSec)
    : cpuThreshold_(cpuThreshold),
      memThresholdMB_(memThresholdMB),
      refreshIntervalSec_(refreshIntervalSec),
      cpuAnalyzer_(cpuThreshold),
      memoryAnalyzer_(memThresholdMB) {}

void ResourceMonitor::setCPUThreshold(double percent) {
    cpuThreshold_ = percent;
    cpuAnalyzer_.setThreshold(percent);
}

double ResourceMonitor::getCPUThreshold() const {
    return cpuThreshold_;
}

void ResourceMonitor::setMemoryThresholdMB(double mb) {
    memThresholdMB_ = mb;
    memoryAnalyzer_.setThreshold(mb);
}

double ResourceMonitor::getMemoryThresholdMB() const {
    return memThresholdMB_;
}

void ResourceMonitor::setRefreshIntervalSec(int sec) {
    refreshIntervalSec_ = sec;
}

int ResourceMonitor::getRefreshIntervalSec() const {
    return refreshIntervalSec_;
}

bool ResourceMonitor::refresh() {
    return processManager_.refresh();
}

SystemStats ResourceMonitor::getSystemStats() const {
    SystemStats stats;
    stats.totalCPU = processManager_.getTotalCPUUsage();
    stats.totalMemoryMB = processManager_.getTotalMemoryUsageMB();
    stats.activeProcesses = processManager_.getProcessCount();
    stats.activeAlerts = AlertManager::getInstance().getActiveWarningOrCriticalCount();
    return stats;
}

void ResourceMonitor::runCPUAnalysis(bool triggerAlerts) {
    refresh();
    cpuAnalyzer_.analyze(processManager_.getProcesses(), triggerAlerts);
    cpuAnalyzer_.printReport(processManager_.getProcesses());
}

void ResourceMonitor::runMemoryAnalysis(bool triggerAlerts) {
    refresh();
    memoryAnalyzer_.analyze(processManager_.getProcesses(), triggerAlerts);
    memoryAnalyzer_.printReport(processManager_.getProcesses());
}

void ResourceMonitor::displayRunningProcesses(size_t limit) const {
    const auto& procs = processManager_.getProcesses();
    std::cout << "\n========================================================================================\n";
    std::cout << "RUNNING PROCESSES (Total: " << procs.size() << " | Showing top " << std::min(limit, procs.size()) << ")\n";
    std::cout << "========================================================================================\n";
    std::cout << std::left 
              << std::setw(8)  << "PID"
              << std::setw(8)  << "PPID"
              << std::setw(22) << "NAME"
              << std::setw(14) << "STATE"
              << std::setw(10) << "CPU %"
              << std::setw(12) << "MEMORY"
              << std::setw(10) << "TIME"
              << "USER" << "\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    size_t count = std::min(limit, procs.size());
    for (size_t i = 0; i < count; ++i) {
        const auto& p = procs[i];

        std::string cpuStr;
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << p.cpuPercent << "%";
            cpuStr = oss.str();
        }

        std::string memStr;
        {
            std::ostringstream oss;
            if (p.rssMB >= 1024.0) {
                oss << std::fixed << std::setprecision(1) << (p.rssMB / 1024.0) << " GB";
            } else {
                oss << std::fixed << std::setprecision(1) << p.rssMB << " MB";
            }
            memStr = oss.str();
        }

        std::string nameDisplay = p.name;
        if (nameDisplay.length() > 20) {
            nameDisplay = nameDisplay.substr(0, 17) + "...";
        }

        std::cout << std::left
                  << std::setw(8)  << p.pid
                  << std::setw(8)  << p.ppid
                  << std::setw(22) << nameDisplay
                  << std::setw(14) << p.stateDescription
                  << std::setw(10) << cpuStr
                  << std::setw(12) << memStr
                  << std::setw(10) << p.runTime
                  << p.user << "\n";
    }
    std::cout << "----------------------------------------------------------------------------------------\n";
}

ProcessManager& ResourceMonitor::getProcessManager() {
    return processManager_;
}

const ProcessManager& ResourceMonitor::getProcessManager() const {
    return processManager_;
}
