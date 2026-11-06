#include "ProcessManager.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <array>
#include <memory>

ProcessManager::ProcessManager() {
    refresh();
}

std::string ProcessManager::extractBaseName(const std::string& path) {
    if (path.empty()) return "unknown";
    std::string cleanPath = path;
    while (cleanPath.length() > 1 && (cleanPath.back() == '/' || cleanPath.back() == '\\')) {
        cleanPath.pop_back();
    }
    size_t lastSlash = cleanPath.find_last_of("/\\");
    if (lastSlash != std::string::npos && lastSlash + 1 < cleanPath.size()) {
        return cleanPath.substr(lastSlash + 1);
    }
    return cleanPath;
}

std::string ProcessManager::interpretState(const std::string& rawState) {
    if (rawState.empty()) return "UNKNOWN";
    char primary = rawState[0];
    switch (primary) {
        case 'R': return "RUNNING";
        case 'S': return "SLEEPING";
        case 'I': return "IDLE";
        case 'T': return "STOPPED";
        case 'Z': return "ZOMBIE";
        case 'U': return "UNINTERRUPTIBLE";
        default:  return "ACTIVE";
    }
}

bool ProcessManager::refresh() {
    processes_.clear();

    // macOS compatible command
    const char* cmd = "ps -axo pid,ppid,comm,state,%cpu,rss,time,user 2>/dev/null";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return false;
    }

    std::array<char, 1024> buffer;
    bool isFirstLine = true;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string line(buffer.data());
        // Remove trailing newline / carriage return
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty()) continue;

        if (isFirstLine) {
            isFirstLine = false;
            // Skip the header: PID PPID COMM STAT %CPU RSS TIME USER
            continue;
        }

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        // Expected minimum: PID, PPID, COMM, STAT, %CPU, RSS, TIME, USER -> 8 tokens
        if (tokens.size() < 7) {
            continue;
        }

        ProcessInfo info;
        try {
            info.pid = static_cast<pid_t>(std::stol(tokens[0]));
            info.ppid = static_cast<pid_t>(std::stol(tokens[1]));
        } catch (...) {
            continue;
        }

        size_t n = tokens.size();
        info.user = tokens[n - 1];
        info.runTime = tokens[n - 2];

        try {
            info.rssKB = std::stol(tokens[n - 3]);
            info.rssMB = static_cast<double>(info.rssKB) / 1024.0;
        } catch (...) {
            info.rssKB = 0;
            info.rssMB = 0.0;
        }

        try {
            info.cpuPercent = std::stod(tokens[n - 4]);
        } catch (...) {
            info.cpuPercent = 0.0;
        }

        if (n >= 7) {
            info.rawState = tokens[n - 5];
            info.stateDescription = interpretState(info.rawState);

            std::string comm;
            for (size_t i = 2; i <= n - 6; ++i) {
                if (i > 2) comm += " ";
                comm += tokens[i];
            }
            info.fullPath = comm;
            info.name = extractBaseName(comm);
        } else {
            info.rawState = "?";
            info.stateDescription = "UNKNOWN";
            info.name = "unknown";
        }

        processes_.push_back(info);
    }

    return true;
}

const std::vector<ProcessInfo>& ProcessManager::getProcesses() const {
    return processes_;
}

std::optional<ProcessInfo> ProcessManager::getProcessByPid(pid_t pid) const {
    for (const auto& proc : processes_) {
        if (proc.pid == pid) {
            return proc;
        }
    }
    return std::nullopt;
}

std::vector<ProcessInfo> ProcessManager::getTopProcessesByCPU(size_t limit) const {
    std::vector<ProcessInfo> sorted = processes_;
    std::sort(sorted.begin(), sorted.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.cpuPercent > b.cpuPercent;
    });
    if (sorted.size() > limit) {
        sorted.resize(limit);
    }
    return sorted;
}

std::vector<ProcessInfo> ProcessManager::getTopProcessesByMemory(size_t limit) const {
    std::vector<ProcessInfo> sorted = processes_;
    std::sort(sorted.begin(), sorted.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.rssMB > b.rssMB;
    });
    if (sorted.size() > limit) {
        sorted.resize(limit);
    }
    return sorted;
}

size_t ProcessManager::getProcessCount() const {
    return processes_.size();
}

double ProcessManager::getTotalCPUUsage() const {
    double total = 0.0;
    for (const auto& proc : processes_) {
        total += proc.cpuPercent;
    }
    return total;
}

double ProcessManager::getTotalMemoryUsageMB() const {
    double total = 0.0;
    for (const auto& proc : processes_) {
        total += proc.rssMB;
    }
    return total;
}
