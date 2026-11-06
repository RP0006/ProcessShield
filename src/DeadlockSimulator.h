#ifndef DEADLOCKSIMULATOR_H
#define DEADLOCKSIMULATOR_H

#include <string>
#include <vector>
#include <map>
#include "WaitForGraph.h"

struct DeadlockResult {
    bool isDeadlocked;
    std::vector<std::string> involvedProcesses;
    std::vector<std::string> involvedResources;
    std::string cyclePathDescription;
};

class DeadlockSimulator {
public:
    DeadlockSimulator();

    void clear();
    void addAllocation(const std::string& process, const std::string& resource);
    void addRequest(const std::string& process, const std::string& resource);

    WaitForGraph buildWaitForGraph() const;
    DeadlockResult runSimulation(bool triggerAlerts = true);

    // Preset scenarios for demonstration
    void loadClassicDeadlockScenario();
    void loadThreeProcessDeadlockScenario();
    void loadSafeScenario();

    void displayRAG() const;

private:
    // Process -> Resources it holds
    std::map<std::string, std::vector<std::string>> processHolds_;
    // Process -> Resources it is requesting
    std::map<std::string, std::vector<std::string>> processRequests_;
    // Resource -> Process holding it (single instance model)
    std::map<std::string, std::string> resourceHolder_;
};

#endif // DEADLOCKSIMULATOR_H
