#include <iostream>
#include <cassert>
#include "../src/WaitForGraph.h"
#include "../src/DeadlockSimulator.h"
#include "../src/BankersAlgorithm.h"

int main() {
    std::cout << "============================================================\n";
    std::cout << "       ProcessShield - Automated Algorithmic Test Suite     \n";
    std::cout << "============================================================\n";

    // 1. Test Wait-For Graph Cycle Detection (Positive Deadlock Case)
    std::cout << "[Test 1] Wait-For Graph: 2-Process Cycle Detection (P1 <-> P2)...\n";
    {
        WaitForGraph wfg;
        wfg.addWaitEdge("P1", "P2");
        wfg.addWaitEdge("P2", "P1");

        CycleInfo info = wfg.detectDeadlock();
        assert(info.hasCycle == true);
        assert(info.involvedProcesses.size() == 2);
        std::cout << "  --> PASS: Detected cycle correctly.\n";
    }

    // 2. Test Wait-For Graph (Negative Case - No Deadlock)
    std::cout << "[Test 2] Wait-For Graph: Acyclic Graph (P1 -> P2 -> P3)...\n";
    {
        WaitForGraph wfg;
        wfg.addWaitEdge("P1", "P2");
        wfg.addWaitEdge("P2", "P3");

        CycleInfo info = wfg.detectDeadlock();
        assert(info.hasCycle == false);
        std::cout << "  --> PASS: Correctly verified no deadlock.\n";
    }

    // 3. Test Deadlock Simulator RAG to WFG conversion
    std::cout << "[Test 3] Deadlock Simulator: Classic RAG Simulation...\n";
    {
        DeadlockSimulator sim;
        sim.loadClassicDeadlockScenario();
        DeadlockResult res = sim.runSimulation(false);
        assert(res.isDeadlocked == true);
        assert(!res.cyclePathDescription.empty());
        std::cout << "  --> PASS: RAG correctly generated WFG cycle: " << res.cyclePathDescription << "\n";
    }

    // 4. Test Banker's Algorithm Safe State
    std::cout << "[Test 4] Banker's Algorithm: Classic Safe Case...\n";
    {
        BankersAlgorithm bankers;
        bankers.loadClassicSafeScenario();
        BankersResult res = bankers.checkSafety(false);
        assert(res.isSafe == true);
        assert(res.safeSequence.size() == 5);
        std::cout << "  --> PASS: System in SAFE state. Safe sequence length: " << res.safeSequence.size() << "\n";
    }

    // 5. Test Banker's Algorithm Unsafe State
    std::cout << "[Test 5] Banker's Algorithm: Unsafe Case...\n";
    {
        BankersAlgorithm bankers;
        bankers.loadClassicUnsafeScenario();
        BankersResult res = bankers.checkSafety(false);
        assert(res.isSafe == false);
        assert(!res.unsatisfiedProcesses.empty());
        std::cout << "  --> PASS: Correctly recognized UNSAFE state.\n";
    }

    std::cout << "\n============================================================\n";
    std::cout << "       ALL ALGORITHMIC TESTS PASSED SUCCESSFULLY! (5/5)     \n";
    std::cout << "============================================================\n";
    return 0;
}
