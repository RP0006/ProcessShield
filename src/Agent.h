#ifndef AGENT_H
#define AGENT_H

#include <string>
#include <vector>
#include <chrono>
#include "ProcessManager.h"

struct NodeTelemetry {
    std::string nodeId;
    std::string hostname;
    std::string ip;
    bool isSimulated = false;
    std::string systemType;
    double cpuPercent = 0.0;
    double ramUsedGB = 0.0;
    double ramTotalGB = 0.0;
    double ramPercent = 0.0;
    double diskUsedGB = 0.0;
    double diskTotalGB = 0.0;
    double diskPercent = 0.0;
    double netSentKB = 0.0;
    double netRecvKB = 0.0;
    std::string interfaceName = "en0";
    size_t processCount = 0;
    std::string health = "HEALTHY";
    std::vector<std::string> healthReasons;
    std::vector<ProcessInfo> topProcesses;
};

class Agent {
public:
    Agent(const std::string& masterHost = "127.0.0.1", int masterPort = 8080, int intervalSec = 5, const std::string& authKey = "ProcessShield-OS-2026");
    ~Agent() = default;

    void run();
    bool collectAndSendOnce();

    static std::string detectLocalIP();
    static std::string detectHostname();
    static bool getDiskStats(double& totalGB, double& usedGB, double& percent);
    static bool getRAMStats(double& totalGB, double& usedGB, double& percent);
    static std::string evaluateHealth(double cpuPct, double ramPct, double diskPct, std::vector<std::string>& reasons);

private:
    std::string masterHost_;
    int masterPort_;
    int intervalSec_;
    std::string authKey_;
    ProcessManager processManager_;

    std::string escapeJson(const std::string& s) const;
    std::string buildJsonPayload(const NodeTelemetry& tele) const;
    bool sendHttpReport(const std::string& jsonPayload) const;
};

#endif // AGENT_H
