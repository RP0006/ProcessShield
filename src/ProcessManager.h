#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <string>
#include <vector>
#include <optional>
#include <sys/types.h>

struct ProcessInfo {
    pid_t pid = 0;
    pid_t ppid = 0;
    std::string name;
    std::string fullPath;
    std::string rawState;
    std::string stateDescription;
    double cpuPercent = 0.0;
    long rssKB = 0;
    double rssMB = 0.0;
    std::string runTime;
    std::string user;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager() = default;

    bool refresh();
    const std::vector<ProcessInfo>& getProcesses() const;
    std::optional<ProcessInfo> getProcessByPid(pid_t pid) const;

    std::vector<ProcessInfo> getTopProcessesByCPU(size_t limit = 15) const;
    std::vector<ProcessInfo> getTopProcessesByMemory(size_t limit = 15) const;

    size_t getProcessCount() const;
    double getTotalCPUUsage() const;
    double getTotalMemoryUsageMB() const;

    static std::string interpretState(const std::string& rawState);
    static std::string extractBaseName(const std::string& path);

private:
    std::vector<ProcessInfo> processes_;
};

#endif // PROCESSMANAGER_H
