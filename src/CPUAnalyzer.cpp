#include "CPUAnalyzer.h"
#include "AlertManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

CPUAnalyzer::CPUAnalyzer(double thresholdPercent)
    : thresholdPercent_(thresholdPercent) {}

void CPUAnalyzer::setThreshold(double thresholdPercent) {
    thresholdPercent_ = thresholdPercent;
}

double CPUAnalyzer::getThreshold() const {
    return thresholdPercent_;
}

std::vector<CPUViolation> CPUAnalyzer::analyze(const std::vector<ProcessInfo>& processes, bool triggerAlerts) {
    std::vector<CPUViolation> violations;

    for (const auto& proc : processes) {
        if (proc.cpuPercent >= thresholdPercent_) {
            CPUViolation violation{proc.pid, proc.name, proc.cpuPercent, thresholdPercent_};
            violations.push_back(violation);

            if (triggerAlerts) {
                std::ostringstream msg;
                msg << "High CPU usage detected: " << proc.name << " (PID " << proc.pid 
                    << ") using " << std::fixed << std::setprecision(1) << proc.cpuPercent 
                    << "% (Threshold: " << thresholdPercent_ << "%)";

                std::ostringstream details;
                details << "PID: " << proc.pid << ", CPU: " << std::fixed << std::setprecision(1) 
                        << proc.cpuPercent << "%, Threshold: " << thresholdPercent_ << "%";

                AlertManager::getInstance().addAlert(
                    AlertLevel::WARNING,
                    "CPU Monitor",
                    msg.str(),
                    details.str()
                );
            }
        }
    }

    return violations;
}

void CPUAnalyzer::printReport(const std::vector<ProcessInfo>& processes, size_t topN) {
    std::cout << "\n============================================================\n";
    std::cout << "CPU MONITOR (Configured Threshold: " << std::fixed << std::setprecision(1) << thresholdPercent_ << "%)\n";
    std::cout << "============================================================\n";
    std::cout << std::left << std::setw(10) << "PID"
              << std::setw(24) << "PROCESS"
              << std::setw(12) << "CPU %"
              << "STATUS" << "\n";
    std::cout << "------------------------------------------------------------\n";

    std::vector<ProcessInfo> sorted = processes;
    std::sort(sorted.begin(), sorted.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.cpuPercent > b.cpuPercent;
    });

    size_t count = std::min(topN, sorted.size());
    std::vector<CPUViolation> violations;

    for (size_t i = 0; i < count; ++i) {
        const auto& p = sorted[i];
        bool isExceeded = (p.cpuPercent >= thresholdPercent_);
        if (isExceeded) {
            violations.push_back({p.pid, p.name, p.cpuPercent, thresholdPercent_});
        }

        std::string cpuStr;
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << p.cpuPercent << "%";
            cpuStr = oss.str();
        }

        std::cout << std::left << std::setw(10) << p.pid
                  << std::setw(24) << (p.name.length() > 22 ? p.name.substr(0, 19) + "..." : p.name)
                  << std::setw(12) << cpuStr
                  << (isExceeded ? "[!] THRESHOLD EXCEEDED" : "NORMAL")
                  << "\n";
    }

    if (!violations.empty()) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "WARNING: HIGH CPU USAGE DETECTED (" << violations.size() << " process(es))\n";
        for (const auto& v : violations) {
            std::cout << "  * Process:   " << v.processName << "\n";
            std::cout << "    PID:       " << v.pid << "\n";
            std::cout << "    CPU:       " << std::fixed << std::setprecision(1) << v.cpuPercent << "%\n";
            std::cout << "    Threshold: " << v.threshold << "%\n";
        }
        std::cout << "------------------------------------------------------------\n";
    } else {
        std::cout << "\nNo processes exceeding CPU threshold (" << thresholdPercent_ << "%).\n";
    }
}
