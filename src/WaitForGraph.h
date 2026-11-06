#ifndef WAITFORGRAPH_H
#define WAITFORGRAPH_H

#include <string>
#include <vector>
#include <map>
#include <set>

struct CycleInfo {
    bool hasCycle = false;
    std::vector<std::string> cyclePath;
    std::vector<std::string> involvedProcesses;
};

class WaitForGraph {
public:
    WaitForGraph() = default;

    void addProcess(const std::string& process);
    void addWaitEdge(const std::string& fromProcess, const std::string& toProcess);
    void clear();

    CycleInfo detectDeadlock() const;
    void printGraph() const;
    void printDeadlockReport(const CycleInfo& cycle) const;

    const std::map<std::string, std::set<std::string>>& getAdjacencyList() const;
    const std::set<std::string>& getProcesses() const;

private:
    bool dfsVisit(const std::string& current,
                  std::map<std::string, int>& color,
                  std::map<std::string, std::string>& parent,
                  std::vector<std::string>& path) const;

    std::set<std::string> processes_;
    std::map<std::string, std::set<std::string>> adj_;
};

#endif // WAITFORGRAPH_H
