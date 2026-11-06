#include "BankersAlgorithm.h"
#include "AlertManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

BankersAlgorithm::BankersAlgorithm() : numProcesses_(0), numResources_(0) {
    loadClassicSafeScenario();
}

BankersAlgorithm::BankersAlgorithm(int numProcesses, int numResources)
    : numProcesses_(numProcesses), numResources_(numResources) {
    allocation_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    max_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    need_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    available_.assign(numResources_, 0);
}

void BankersAlgorithm::setDimensions(int numProcesses, int numResources) {
    numProcesses_ = numProcesses;
    numResources_ = numResources;
    allocation_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    max_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    need_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    available_.assign(numResources_, 0);
}

void BankersAlgorithm::setAllocation(const std::vector<std::vector<int>>& alloc) {
    allocation_ = alloc;
}

void BankersAlgorithm::setMax(const std::vector<std::vector<int>>& max) {
    max_ = max;
}

void BankersAlgorithm::setAvailable(const std::vector<int>& avail) {
    available_ = avail;
}

bool BankersAlgorithm::computeNeedMatrix() {
    if (static_cast<int>(allocation_.size()) != numProcesses_ || static_cast<int>(max_.size()) != numProcesses_) {
        return false;
    }

    need_.assign(numProcesses_, std::vector<int>(numResources_, 0));
    for (int i = 0; i < numProcesses_; ++i) {
        for (int j = 0; j < numResources_; ++j) {
            if (max_[i][j] < allocation_[i][j]) {
                // Max cannot be less than Allocation
                return false;
            }
            need_[i][j] = max_[i][j] - allocation_[i][j];
        }
    }
    return true;
}

BankersResult BankersAlgorithm::checkSafety(bool triggerAlerts) {
    BankersResult result;
    result.isSafe = false;

    if (!computeNeedMatrix()) {
        return result;
    }

    std::vector<int> work = available_;
    std::vector<bool> finish(numProcesses_, false);
    std::vector<int> safeSeq;

    int count = 0;
    while (count < numProcesses_) {
        bool found = false;
        for (int p = 0; p < numProcesses_; ++p) {
            if (!finish[p]) {
                bool canAllocate = true;
                for (int r = 0; r < numResources_; ++r) {
                    if (need_[p][r] > work[r]) {
                        canAllocate = false;
                        break;
                    }
                }

                if (canAllocate) {
                    for (int r = 0; r < numResources_; ++r) {
                        work[r] += allocation_[p][r];
                    }
                    safeSeq.push_back(p);
                    finish[p] = true;
                    found = true;
                    count++;
                }
            }
        }

        if (!found) {
            break; // No process can finish with current available resources
        }
    }

    if (count == numProcesses_) {
        result.isSafe = true;
        result.safeSequence = safeSeq;
    } else {
        result.isSafe = false;
        for (int p = 0; p < numProcesses_; ++p) {
            if (!finish[p]) {
                result.unsatisfiedProcesses.push_back(p);
            }
        }

        if (triggerAlerts) {
            std::ostringstream msg;
            msg << "Banker's Algorithm: System entered UNSAFE state (Deadlock Risk). "
                << result.unsatisfiedProcesses.size() << " process(es) cannot complete.";

            std::ostringstream det;
            det << "Blocked processes: ";
            for (size_t i = 0; i < result.unsatisfiedProcesses.size(); ++i) {
                det << "P" << result.unsatisfiedProcesses[i] << (i + 1 < result.unsatisfiedProcesses.size() ? ", " : "");
            }

            AlertManager::getInstance().addAlert(
                AlertLevel::WARNING,
                "Banker's Algorithm",
                msg.str(),
                det.str()
            );
        }
    }

    return result;
}

bool BankersAlgorithm::requestResources(int processId, const std::vector<int>& request, bool triggerAlerts) {
    if (processId < 0 || processId >= numProcesses_) {
        return false;
    }
    if (static_cast<int>(request.size()) != numResources_) {
        return false;
    }

    // 1. Check if Request <= Need
    for (int j = 0; j < numResources_; ++j) {
        if (request[j] > need_[processId][j]) {
            std::cout << "[!] Error: Process P" << processId << " has exceeded its maximum claim (Request > Need)!\n";
            return false;
        }
    }

    // 2. Check if Request <= Available
    for (int j = 0; j < numResources_; ++j) {
        if (request[j] > available_[j]) {
            std::cout << "[!] Notice: Process P" << processId << " must wait. Resources are not available currently.\n";
            return false;
        }
    }

    // 3. Pretend to allocate
    for (int j = 0; j < numResources_; ++j) {
        available_[j] -= request[j];
        allocation_[processId][j] += request[j];
        need_[processId][j] -= request[j];
    }

    // 4. Test safety
    BankersResult safety = checkSafety(triggerAlerts);

    if (safety.isSafe) {
        std::cout << "[+] Request GRANTED to P" << processId << ". System remains in a SAFE state.\n";
        return true;
    } else {
        std::cout << "[-] Request REJECTED! Granting this request leads to an UNSAFE state.\n";
        std::cout << "    Rolling back resource allocation...\n";
        // Rollback
        for (int j = 0; j < numResources_; ++j) {
            available_[j] += request[j];
            allocation_[processId][j] -= request[j];
            need_[processId][j] += request[j];
        }
        return false;
    }
}

void BankersAlgorithm::loadClassicSafeScenario() {
    numProcesses_ = 5;
    numResources_ = 3;

    allocation_ = {
        {0, 1, 0}, // P0
        {2, 0, 0}, // P1
        {3, 0, 2}, // P2
        {2, 1, 1}, // P3
        {0, 0, 2}  // P4
    };

    max_ = {
        {7, 5, 3}, // P0
        {3, 2, 2}, // P1
        {9, 0, 2}, // P2
        {2, 2, 2}, // P3
        {4, 3, 3}  // P4
    };

    available_ = {3, 3, 2};
    computeNeedMatrix();
}

void BankersAlgorithm::loadClassicUnsafeScenario() {
    numProcesses_ = 4;
    numResources_ = 3;

    allocation_ = {
        {2, 1, 1}, // P0
        {1, 2, 1}, // P1
        {2, 1, 1}, // P2
        {1, 1, 1}  // P3
    };

    max_ = {
        {5, 4, 3}, // P0
        {4, 4, 4}, // P1
        {5, 3, 3}, // P2
        {3, 3, 3}  // P3
    };

    // Very low available vector that cannot satisfy ANY process's remaining need
    available_ = {0, 0, 0};
    computeNeedMatrix();
}

void BankersAlgorithm::printMatrices() const {
    std::cout << "\n========================================================================\n";
    std::cout << "BANKER'S ALGORITHM RESOURCE ALLOCATION MATRICES\n";
    std::cout << "========================================================================\n";

    std::cout << std::left << std::setw(10) << "PROCESS"
              << std::setw(18) << "ALLOCATION"
              << std::setw(18) << "MAX"
              << std::setw(18) << "NEED (Max - Alloc)"
              << "AVAILABLE\n";
    std::cout << "------------------------------------------------------------------------\n";

    for (int i = 0; i < numProcesses_; ++i) {
        std::cout << std::left << std::setw(10) << ("P" + std::to_string(i));

        // Alloc
        std::ostringstream allocStr;
        for (int j = 0; j < numResources_; ++j) allocStr << allocation_[i][j] << " ";
        std::cout << std::setw(18) << allocStr.str();

        // Max
        std::ostringstream maxStr;
        for (int j = 0; j < numResources_; ++j) maxStr << max_[i][j] << " ";
        std::cout << std::setw(18) << maxStr.str();

        // Need
        std::ostringstream needStr;
        for (int j = 0; j < numResources_; ++j) needStr << need_[i][j] << " ";
        std::cout << std::setw(18) << needStr.str();

        // Avail (only on first row)
        if (i == 0) {
            std::ostringstream availStr;
            for (int j = 0; j < numResources_; ++j) availStr << available_[j] << " ";
            std::cout << availStr.str();
        }
        std::cout << "\n";
    }
    std::cout << "------------------------------------------------------------------------\n";
}

void BankersAlgorithm::printSafetyReport(const BankersResult& result) const {
    printMatrices();

    std::cout << "\nSystem Safety Assessment:\n";
    if (result.isSafe) {
        std::cout << "  System State: SAFE\n\n";
        std::cout << "  Safe Sequence Found:\n  ";
        for (size_t i = 0; i < result.safeSequence.size(); ++i) {
            std::cout << "P" << result.safeSequence[i];
            if (i + 1 < result.safeSequence.size()) {
                std::cout << " -> ";
            }
        }
        std::cout << "\n\n  Explanation:\n";
        std::cout << "  A safe sequence guarantees that every process can eventually obtain its\n";
        std::cout << "  maximum declared resources, execute to completion, and return all held\n";
        std::cout << "  resources back to the system without entering a deadlock state.\n";
    } else {
        std::cout << "  System State: UNSAFE (Deadlock Risk!)\n\n";
        std::cout << "  No safe sequence exists!\n";
        std::cout << "  The following processes cannot obtain their required resources:\n";
        for (int p : result.unsatisfiedProcesses) {
            std::cout << "    * P" << p << "\n";
        }
        std::cout << "\n  Explanation:\n";
        std::cout << "  An unsafe state is NOT synonymous with an active deadlock, but it represents\n";
        std::cout << "  a state where the OS cannot prevent processes from deadlocking if each requests\n";
        std::cout << "  its maximum remaining claim.\n";
    }
    std::cout << "========================================================================\n";
}
