#include "MemoryAnalyzer.h"
#include "AlertManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

MemoryAnalyzer::MemoryAnalyzer(double thresholdMB)
    : thresholdMB_(thresholdMB) {}

void MemoryAnalyzer::setThreshold(double thresholdMB) {
    thresholdMB_ = thresholdMB;
}

double MemoryAnalyzer::getThreshold() const {
    return thresholdMB_;
}

std::vector<MemoryViolation> MemoryAnalyzer::analyze(const std::vector<ProcessInfo>& processes, bool triggerAlerts) {
    std::vector<MemoryViolation> violations;

    for (const auto& proc : processes) {
        if (proc.rssMB >= thresholdMB_) {
            MemoryViolation violation{proc.pid, proc.name, proc.rssMB, thresholdMB_};
            violations.push_back(violation);

            if (triggerAlerts) {
                std::ostringstream msg;
                msg << "High Memory usage detected: " << proc.name << " (PID " << proc.pid 
                    << ") using " << std::fixed << std::setprecision(1) << proc.rssMB 
                    << " MB (Threshold: " << thresholdMB_ << " MB)";

                std::ostringstream details;
                details << "PID: " << proc.pid << ", Memory: " << std::fixed << std::setprecision(1) 
                        << proc.rssMB << " MB, Threshold: " << thresholdMB_ << " MB";

                AlertManager::getInstance().addAlert(
                    AlertLevel::WARNING,
                    "Memory Monitor",
                    msg.str(),
                    details.str()
                );
            }
        }
    }

    return violations;
}

void MemoryAnalyzer::printReport(const std::vector<ProcessInfo>& processes, size_t topN) {
    std::cout << "\n============================================================\n";
    std::cout << "MEMORY MONITOR (Configured Threshold: " << std::fixed << std::setprecision(1) << thresholdMB_ << " MB)\n";
    std::cout << "============================================================\n";
    std::cout << std::left << std::setw(10) << "PID"
              << std::setw(24) << "PROCESS"
              << std::setw(14) << "MEMORY (MB)"
              << "STATUS" << "\n";
    std::cout << "------------------------------------------------------------\n";

    std::vector<ProcessInfo> sorted = processes;
    std::sort(sorted.begin(), sorted.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.rssMB > b.rssMB;
    });

    size_t count = std::min(topN, sorted.size());
    std::vector<MemoryViolation> violations;

    for (size_t i = 0; i < count; ++i) {
        const auto& p = sorted[i];
        bool isExceeded = (p.rssMB >= thresholdMB_);
        if (isExceeded) {
            violations.push_back({p.pid, p.name, p.rssMB, thresholdMB_});
        }

        std::string memStr;
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << p.rssMB << " MB";
            memStr = oss.str();
        }

        std::cout << std::left << std::setw(10) << p.pid
                  << std::setw(24) << (p.name.length() > 22 ? p.name.substr(0, 19) + "..." : p.name)
                  << std::setw(14) << memStr
                  << (isExceeded ? "[!] THRESHOLD EXCEEDED" : "NORMAL")
                  << "\n";
    }

    if (!violations.empty()) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "WARNING: HIGH MEMORY USAGE DETECTED (" << violations.size() << " process(es))\n";
        for (const auto& v : violations) {
            std::cout << "  * Process:   " << v.processName << "\n";
            std::cout << "    PID:       " << v.pid << "\n";
            std::cout << "    Memory:    " << std::fixed << std::setprecision(1) << v.memoryMB << " MB\n";
            std::cout << "    Threshold: " << v.thresholdMB << " MB\n";
        }
        std::cout << "------------------------------------------------------------\n";
    } else {
        std::cout << "\nNo processes exceeding Memory threshold (" << thresholdMB_ << " MB).\n";
    }
}
