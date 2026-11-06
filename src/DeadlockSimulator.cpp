#include "DeadlockSimulator.h"
#include "AlertManager.h"
#include <iostream>
#include <sstream>
#include <set>

DeadlockSimulator::DeadlockSimulator() {
    loadClassicDeadlockScenario();
}

void DeadlockSimulator::clear() {
    processHolds_.clear();
    processRequests_.clear();
    resourceHolder_.clear();
}

void DeadlockSimulator::addAllocation(const std::string& process, const std::string& resource) {
    processHolds_[process].push_back(resource);
    resourceHolder_[resource] = process;
}

void DeadlockSimulator::addRequest(const std::string& process, const std::string& resource) {
    processRequests_[process].push_back(resource);
}

WaitForGraph DeadlockSimulator::buildWaitForGraph() const {
    WaitForGraph wfg;

    // Ensure all processes are known in the graph
    for (const auto& pair : processHolds_) {
        wfg.addProcess(pair.first);
    }
    for (const auto& pair : processRequests_) {
        wfg.addProcess(pair.first);
    }

    // A process Pi waits for Pj if Pi requests resource Rk, and Pj currently holds Rk
    for (const auto& reqPair : processRequests_) {
        const std::string& requestingProc = reqPair.first;
        for (const std::string& requestedRes : reqPair.second) {
            auto holderIt = resourceHolder_.find(requestedRes);
            if (holderIt != resourceHolder_.end()) {
                const std::string& holdingProc = holderIt->second;
                if (requestingProc != holdingProc) {
                    wfg.addWaitEdge(requestingProc, holdingProc);
                }
            }
        }
    }

    return wfg;
}

void DeadlockSimulator::loadClassicDeadlockScenario() {
    clear();
    // P1 holds R1 and requests R2
    addAllocation("P1", "R1");
    addRequest("P1", "R2");

    // P2 holds R2 and requests R1
    addAllocation("P2", "R2");
    addRequest("P2", "R1");
}

void DeadlockSimulator::loadThreeProcessDeadlockScenario() {
    clear();
    // P1 holds R1 and requests R2
    addAllocation("P1", "R1");
    addRequest("P1", "R2");

    // P2 holds R2 and requests R3
    addAllocation("P2", "R2");
    addRequest("P2", "R3");

    // P3 holds R3 and requests R1
    addAllocation("P3", "R3");
    addRequest("P3", "R1");
}

void DeadlockSimulator::loadSafeScenario() {
    clear();
    // P1 holds R1 and requests R2
    addAllocation("P1", "R1");
    addRequest("P1", "R2");

    // P2 holds R2, and does not request any blocked resources (R3 is free)
    addAllocation("P2", "R2");
    // P2 requests nothing held by P1 -> can finish and release R2
}

void DeadlockSimulator::displayRAG() const {
    std::cout << "\nResource Allocation State (RAG):\n";
    std::cout << "--------------------------------\n";
    std::cout << "Allocations (Process holds Resource):\n";
    for (const auto& pair : processHolds_) {
        for (const auto& res : pair.second) {
            std::cout << "  " << pair.first << " holds " << res << "\n";
        }
    }

    std::cout << "\nRequests (Process requests Resource):\n";
    for (const auto& pair : processRequests_) {
        for (const auto& res : pair.second) {
            std::cout << "  " << pair.first << " requests " << res;
            auto it = resourceHolder_.find(res);
            if (it != resourceHolder_.end()) {
                std::cout << " (Currently held by " << it->second << ")";
            } else {
                std::cout << " (Resource is AVAILABLE)";
            }
            std::cout << "\n";
        }
    }
}

DeadlockResult DeadlockSimulator::runSimulation(bool triggerAlerts) {
    displayRAG();

    WaitForGraph wfg = buildWaitForGraph();
    CycleInfo cycle = wfg.detectDeadlock();
    wfg.printDeadlockReport(cycle);

    DeadlockResult result;
    result.isDeadlocked = cycle.hasCycle;
    result.involvedProcesses = cycle.involvedProcesses;

    if (cycle.hasCycle) {
        std::ostringstream pathOss;
        for (size_t i = 0; i < cycle.cyclePath.size(); ++i) {
            pathOss << cycle.cyclePath[i];
            if (i + 1 < cycle.cyclePath.size()) pathOss << " -> ";
        }
        result.cyclePathDescription = pathOss.str();

        // Identify resources held or requested by the deadlocked processes
        std::set<std::string> resSet;
        for (const auto& proc : cycle.involvedProcesses) {
            auto hIt = processHolds_.find(proc);
            if (hIt != processHolds_.end()) {
                for (const auto& r : hIt->second) resSet.insert(r);
            }
            auto rIt = processRequests_.find(proc);
            if (rIt != processRequests_.end()) {
                for (const auto& r : rIt->second) resSet.insert(r);
            }
        }
        result.involvedResources.assign(resSet.begin(), resSet.end());

        if (triggerAlerts) {
            std::ostringstream msg;
            msg << "Deadlock detected in simulated environment! Cycle: " << result.cyclePathDescription;

            std::ostringstream det;
            det << "Processes: ";
            for (size_t i = 0; i < result.involvedProcesses.size(); ++i) {
                det << result.involvedProcesses[i] << (i + 1 < result.involvedProcesses.size() ? ", " : "");
            }
            det << " | Resources: ";
            for (size_t i = 0; i < result.involvedResources.size(); ++i) {
                det << result.involvedResources[i] << (i + 1 < result.involvedResources.size() ? ", " : "");
            }

            AlertManager::getInstance().addAlert(
                AlertLevel::CRITICAL,
                "Deadlock Simulator",
                msg.str(),
                det.str()
            );
        }
    }

    return result;
}
