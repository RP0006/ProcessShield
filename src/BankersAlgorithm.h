#ifndef BANKERSALGORITHM_H
#define BANKERSALGORITHM_H

#include <vector>
#include <string>

struct BankersResult {
    bool isSafe;
    std::vector<int> safeSequence;
    std::vector<int> unsatisfiedProcesses;
};

class BankersAlgorithm {
public:
    BankersAlgorithm();
    BankersAlgorithm(int numProcesses, int numResources);

    void setDimensions(int numProcesses, int numResources);
    void setAllocation(const std::vector<std::vector<int>>& alloc);
    void setMax(const std::vector<std::vector<int>>& max);
    void setAvailable(const std::vector<int>& avail);

    bool computeNeedMatrix();
    BankersResult checkSafety(bool triggerAlerts = true);
    bool requestResources(int processId, const std::vector<int>& request, bool triggerAlerts = true);

    void printMatrices() const;
    void printSafetyReport(const BankersResult& result) const;

    // Preset test configurations for viva/demos
    void loadClassicSafeScenario();
    void loadClassicUnsafeScenario();

    int getNumProcesses() const { return numProcesses_; }
    int getNumResources() const { return numResources_; }

private:
    int numProcesses_;
    int numResources_;
    std::vector<std::vector<int>> allocation_;
    std::vector<std::vector<int>> max_;
    std::vector<std::vector<int>> need_;
    std::vector<int> available_;
};

#endif // BANKERSALGORITHM_H
