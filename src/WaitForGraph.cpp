#include "WaitForGraph.h"
#include <iostream>
#include <algorithm>

void WaitForGraph::addProcess(const std::string& process) {
    processes_.insert(process);
    if (adj_.find(process) == adj_.end()) {
        adj_[process] = std::set<std::string>();
    }
}

void WaitForGraph::addWaitEdge(const std::string& fromProcess, const std::string& toProcess) {
    addProcess(fromProcess);
    addProcess(toProcess);
    adj_[fromProcess].insert(toProcess);
}

void WaitForGraph::clear() {
    processes_.clear();
    adj_.clear();
}

bool WaitForGraph::dfsVisit(const std::string& current,
                           std::map<std::string, int>& color,
                           std::map<std::string, std::string>& parent,
                           std::vector<std::string>& path) const {
    color[current] = 1; // 1 = Gray (currently exploring in recursion stack)

    auto it = adj_.find(current);
    if (it != adj_.end()) {
        for (const auto& neighbor : it->second) {
            if (color[neighbor] == 1) {
                // Back-edge found! We have detected a cycle in the Wait-For Graph.
                path.clear();
                path.push_back(neighbor);
                std::string curr = current;
                while (curr != neighbor && !curr.empty()) {
                    path.push_back(curr);
                    curr = parent[curr];
                }
                path.push_back(neighbor);
                std::reverse(path.begin(), path.end());
                return true;
            }

            if (color[neighbor] == 0) {
                parent[neighbor] = current;
                if (dfsVisit(neighbor, color, parent, path)) {
                    return true;
                }
            }
        }
    }

    color[current] = 2; // 2 = Black (fully explored)
    return false;
}

CycleInfo WaitForGraph::detectDeadlock() const {
    CycleInfo info;
    std::map<std::string, int> color; // 0 = White, 1 = Gray, 2 = Black
    std::map<std::string, std::string> parent;
    std::vector<std::string> path;

    for (const auto& proc : processes_) {
        color[proc] = 0;
    }

    for (const auto& proc : processes_) {
        if (color[proc] == 0) {
            if (dfsVisit(proc, color, parent, path)) {
                info.hasCycle = true;
                info.cyclePath = path;

                // Extract unique processes involved in the cycle
                std::set<std::string> uniqueProcs;
                for (size_t i = 0; i + 1 < path.size(); ++i) {
                    uniqueProcs.insert(path[i]);
                }
                info.involvedProcesses.assign(uniqueProcs.begin(), uniqueProcs.end());
                return info;
            }
        }
    }

    return info;
}

void WaitForGraph::printGraph() const {
    std::cout << "\nWait-For Graph (WFG) Edges:\n";
    if (adj_.empty()) {
        std::cout << "  (Graph is empty)\n";
        return;
    }
    bool hasEdges = false;
    for (const auto& pair : adj_) {
        for (const auto& target : pair.second) {
            std::cout << "  " << pair.first << " -> " << target << "  (" << pair.first 
                      << " is waiting for a resource held by " << target << ")\n";
            hasEdges = true;
        }
    }
    if (!hasEdges) {
        std::cout << "  No wait-for dependencies exist among processes.\n";
    }
}

void WaitForGraph::printDeadlockReport(const CycleInfo& cycle) const {
    std::cout << "\n============================================================\n";
    std::cout << "DEADLOCK DETECTION & ANALYSIS\n";
    std::cout << "============================================================\n";

    printGraph();

    std::cout << "\nAnalysis Result:\n";
    if (cycle.hasCycle) {
        std::cout << "  STATUS: CRITICAL — DEADLOCK DETECTED!\n";
        std::cout << "  Cycle Path: ";
        for (size_t i = 0; i < cycle.cyclePath.size(); ++i) {
            std::cout << cycle.cyclePath[i];
            if (i + 1 < cycle.cyclePath.size()) {
                std::cout << " -> ";
            }
        }
        std::cout << "\n\n  Processes Involved in Deadlock:\n";
        for (const auto& proc : cycle.involvedProcesses) {
            std::cout << "    * " << proc << "\n";
        }
        std::cout << "\n  OS Principle Explanation:\n";
        std::cout << "    1. In a single-unit resource allocation model, a cycle in\n";
        std::cout << "       the Wait-For Graph (WFG) is necessary & sufficient for deadlock.\n";
        std::cout << "    2. The 4 Coffman conditions are fully satisfied:\n";
        std::cout << "       - Mutual Exclusion: Resources are exclusively held.\n";
        std::cout << "       - Hold and Wait: Processes hold resources while waiting for more.\n";
        std::cout << "       - No Preemption: Resources cannot be forcibly revoked.\n";
        std::cout << "       - Circular Wait: A closed loop of processes waiting on each other.\n";
    } else {
        std::cout << "  STATUS: SAFE — NO DEADLOCK DETECTED.\n";
        std::cout << "  No cycle exists in the Wait-For Graph.\n";
        std::cout << "  All processes can theoretically proceed to completion without circular blocking.\n";
    }
    std::cout << "============================================================\n";
}

const std::map<std::string, std::set<std::string>>& WaitForGraph::getAdjacencyList() const {
    return adj_;
}

const std::set<std::string>& WaitForGraph::getProcesses() const {
    return processes_;
}
