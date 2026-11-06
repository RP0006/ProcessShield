#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include "ResourceMonitor.h"
#include "ProcessController.h"
#include "DeadlockSimulator.h"
#include "BankersAlgorithm.h"
#include "AlertManager.h"
#include "Logger.h"
#include "Agent.h"

void clearScreen() {
    // Portable ANSI clear
    std::cout << "\033[2J\033[1;1H";
}

void pauseForUser() {
    std::cout << "\nPress Enter to return to menu...";
    std::string dummy;
    std::getline(std::cin, dummy);
}

void handleAgentMenu() {
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "DISTRIBUTED AGENT CONFIGURATION\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Enter Master Server IP/Hostname [default: 127.0.0.1]: ";
    std::string host;
    std::getline(std::cin, host);
    if (host.empty()) host = "127.0.0.1";

    std::cout << "Enter Master Server Port [default: 8080]: ";
    std::string portStr;
    std::getline(std::cin, portStr);
    int port = 8080;
    if (!portStr.empty()) {
        try {
            port = std::stoi(portStr);
        } catch (...) {
            port = 8080;
        }
    }

    std::cout << "Enter Security Key [default: ProcessShield-OS-2026]: ";
    std::string key;
    std::getline(std::cin, key);
    if (key.empty()) key = "ProcessShield-OS-2026";

    std::cout << "[*] Starting ProcessShield Agent connecting to " << host << ":" << port << "...\n";
    Agent agent(host, port, 5, key);
    agent.run();
}

void printHeader(const ResourceMonitor& monitor) {
    SystemStats stats = monitor.getSystemStats();
    std::cout << "========================================================================================\n";
    std::cout << "                         PROCESS SHIELD - RESOURCE GUARDIAN                             \n";
    std::cout << "                             Intelligent Process Monitor                                \n";
    std::cout << "========================================================================================\n";
    std::cout << " System CPU: " << std::fixed << std::setprecision(1) << stats.totalCPU << "% | "
              << "System Memory: " << std::fixed << std::setprecision(1) << stats.totalMemoryMB << " MB | "
              << "Active Processes: " << stats.activeProcesses << " | "
              << "Alerts: " << stats.activeAlerts << "\n";
    std::cout << " Thresholds: CPU >= " << monitor.getCPUThreshold() << "%, Memory >= " 
              << monitor.getMemoryThresholdMB() << " MB | Log: " << Logger::getInstance().getLogFilePath() << "\n";
    std::cout << "========================================================================================\n";
}


void handleProcessManagement(ResourceMonitor& monitor, ProcessController& controller) {
    while (true) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "PROCESS MANAGEMENT SUBMENU\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "1. View Process Details\n";
        std::cout << "2. Pause Process (SIGSTOP)\n";
        std::cout << "3. Resume Process (SIGCONT)\n";
        std::cout << "4. Terminate Process (SIGTERM)\n";
        std::cout << "5. Return to Main Menu\n";
        std::cout << "Enter choice (1-5): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid choice.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 5) break;

        if (choice >= 1 && choice <= 4) {
            std::cout << "Enter Target Process PID: ";
            pid_t pid;
            if (!(std::cin >> pid)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[!] Invalid PID.\n";
                continue;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            monitor.refresh();

            if (choice == 1) {
                controller.displayProcessDetails(pid, monitor.getProcessManager());
            } else if (choice == 2) {
                ControlResult res = controller.pauseProcess(pid);
                std::cout << (res.success ? "[+] " : "[-] ") << res.message << "\n";
            } else if (choice == 3) {
                ControlResult res = controller.resumeProcess(pid);
                std::cout << (res.success ? "[+] " : "[-] ") << res.message << "\n";
            } else if (choice == 4) {
                std::cout << "[!] CAUTION: Are you sure you want to terminate PID " << pid << "? (y/n): ";
                std::string confirm;
                std::getline(std::cin, confirm);
                if (confirm == "y" || confirm == "Y" || confirm == "yes" || confirm == "YES") {
                    ControlResult res = controller.terminateProcess(pid, true);
                    std::cout << (res.success ? "[+] " : "[-] ") << res.message << "\n";
                } else {
                    std::cout << "[*] Termination cancelled by user.\n";
                }
            }
        } else {
            std::cout << "[!] Invalid selection. Please enter 1-5.\n";
        }
    }
}

void handleDeadlockMenu(DeadlockSimulator& deadlockSim) {
    while (true) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "DEADLOCK DETECTION & SIMULATION SUBMENU\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "1. Run Classic Deadlock Scenario (P1 holds R1 requests R2; P2 holds R2 requests R1)\n";
        std::cout << "2. Run 3-Process Circular Deadlock (P1 -> P2 -> P3 -> P1)\n";
        std::cout << "3. Run Deadlock-Free Safe Scenario\n";
        std::cout << "4. Build Custom Resource Allocation Graph (RAG)\n";
        std::cout << "5. Return to Main Menu\n";
        std::cout << "Enter choice (1-5): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid choice.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 5) break;

        if (choice == 1) {
            std::cout << "\n[+] Loading Classic 2-Process Deadlock Scenario...\n";
            deadlockSim.loadClassicDeadlockScenario();
            deadlockSim.runSimulation(true);
        } else if (choice == 2) {
            std::cout << "\n[+] Loading 3-Process Circular Deadlock Scenario...\n";
            deadlockSim.loadThreeProcessDeadlockScenario();
            deadlockSim.runSimulation(true);
        } else if (choice == 3) {
            std::cout << "\n[+] Loading Safe / Deadlock-Free Scenario...\n";
            deadlockSim.loadSafeScenario();
            deadlockSim.runSimulation(true);
        } else if (choice == 4) {
            deadlockSim.clear();
            std::cout << "\nEnter number of resource allocations (Process holds Resource): ";
            int numAllocs = 0;
            if (std::cin >> numAllocs && numAllocs > 0) {
                for (int i = 0; i < numAllocs; ++i) {
                    std::string p, r;
                    std::cout << "  Allocation " << (i + 1) << " (format: <ProcessName> <ResourceName>, e.g. P1 R1): ";
                    std::cin >> p >> r;
                    deadlockSim.addAllocation(p, r);
                }
            }
            std::cout << "Enter number of resource requests (Process requests Resource): ";
            int numReqs = 0;
            if (std::cin >> numReqs && numReqs > 0) {
                for (int i = 0; i < numReqs; ++i) {
                    std::string p, r;
                    std::cout << "  Request " << (i + 1) << " (format: <ProcessName> <ResourceName>, e.g. P1 R2): ";
                    std::cin >> p >> r;
                    deadlockSim.addRequest(p, r);
                }
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            deadlockSim.runSimulation(true);
        } else {
            std::cout << "[!] Invalid option.\n";
        }
    }
}

void handleBankersMenu(BankersAlgorithm& bankers) {
    while (true) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "BANKER'S ALGORITHM (DEADLOCK AVOIDANCE) SUBMENU\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "1. Run Classic SAFE Scenario (5 Processes, 3 Resource Types)\n";
        std::cout << "2. Run Classic UNSAFE Scenario (Deadlock Risk Demonstration)\n";
        std::cout << "3. Request Resource Allocation for a Process\n";
        std::cout << "4. Custom Matrix Configuration (Enter custom system state)\n";
        std::cout << "5. Return to Main Menu\n";
        std::cout << "Enter choice (1-5): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid choice.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 5) break;

        if (choice == 1) {
            std::cout << "\n[+] Loading Classic Safe Banker's Scenario...\n";
            bankers.loadClassicSafeScenario();
            BankersResult res = bankers.checkSafety(true);
            bankers.printSafetyReport(res);
        } else if (choice == 2) {
            std::cout << "\n[+] Loading Classic Unsafe Banker's Scenario...\n";
            bankers.loadClassicUnsafeScenario();
            BankersResult res = bankers.checkSafety(true);
            bankers.printSafetyReport(res);
        } else if (choice == 3) {
            bankers.printMatrices();
            std::cout << "Enter Process ID to request for (e.g. 1 for P1): ";
            int pid = 0;
            if (!(std::cin >> pid) || pid < 0 || pid >= bankers.getNumProcesses()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[!] Invalid Process ID.\n";
                continue;
            }

            std::vector<int> req(bankers.getNumResources(), 0);
            std::cout << "Enter requested units for " << bankers.getNumResources() << " resources (separated by space): ";
            for (int r = 0; r < bankers.getNumResources(); ++r) {
                std::cin >> req[r];
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            bankers.requestResources(pid, req, true);
        } else if (choice == 4) {
            int n = 0, m = 0;
            std::cout << "Enter number of processes: ";
            if (!(std::cin >> n) || n <= 0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[!] Invalid process count.\n";
                continue;
            }
            std::cout << "Enter number of resource types: ";
            if (!(std::cin >> m) || m <= 0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "[!] Invalid resource count.\n";
                continue;
            }

            bankers.setDimensions(n, m);
            std::vector<std::vector<int>> alloc(n, std::vector<int>(m, 0));
            std::vector<std::vector<int>> max(n, std::vector<int>(m, 0));
            std::vector<int> avail(m, 0);

            std::cout << "\nEnter Allocation Matrix (" << n << "x" << m << "):\n";
            for (int i = 0; i < n; ++i) {
                std::cout << "  Process P" << i << ": ";
                for (int j = 0; j < m; ++j) std::cin >> alloc[i][j];
            }

            std::cout << "\nEnter Max Matrix (" << n << "x" << m << "):\n";
            for (int i = 0; i < n; ++i) {
                std::cout << "  Process P" << i << ": ";
                for (int j = 0; j < m; ++j) std::cin >> max[i][j];
            }

            std::cout << "\nEnter Available Vector (" << m << " values): ";
            for (int j = 0; j < m; ++j) std::cin >> avail[j];
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            bankers.setAllocation(alloc);
            bankers.setMax(max);
            bankers.setAvailable(avail);

            BankersResult res = bankers.checkSafety(true);
            bankers.printSafetyReport(res);
        } else {
            std::cout << "[!] Invalid selection.\n";
        }
    }
}

void handleAlertsMenu() {
    std::cout << "\n============================================================\n";
    std::cout << "ACTIVE SYSTEM ALERTS\n";
    std::cout << "============================================================\n";

    auto alerts = AlertManager::getInstance().getAllAlerts();
    if (alerts.empty()) {
        std::cout << "  (No active alerts recorded)\n";
    } else {
        std::cout << std::left << std::setw(22) << "TIMESTAMP"
                  << std::setw(12) << "LEVEL"
                  << std::setw(20) << "SOURCE"
                  << "MESSAGE\n";
        std::cout << "----------------------------------------------------------------------------------------\n";
        for (const auto& a : alerts) {
            std::cout << std::left << std::setw(22) << a.timestamp
                      << std::setw(12) << AlertManager::levelToString(a.level)
                      << std::setw(20) << a.source
                      << a.message << "\n";
            if (!a.details.empty()) {
                std::cout << "    Details: " << a.details << "\n";
            }
        }
    }

    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << "Clear all alerts? (y/n): ";
    std::string clearChoice;
    std::getline(std::cin, clearChoice);
    if (clearChoice == "y" || clearChoice == "Y") {
        AlertManager::getInstance().clearAlerts();
        std::cout << "[+] Alerts cleared successfully.\n";
    }
}

void handleLogsMenu() {
    std::cout << "\n============================================================\n";
    std::cout << "SYSTEM EVENT LOGS (File: " << Logger::getInstance().getLogFilePath() << ")\n";
    std::cout << "============================================================\n";

    auto logs = Logger::getInstance().getRecentLogs(25);
    if (logs.empty()) {
        std::cout << "  (Log file is currently empty)\n";
    } else {
        for (const auto& line : logs) {
            std::cout << line << "\n";
        }
    }
    std::cout << "============================================================\n";
}

void handleConfigMenu(ResourceMonitor& monitor) {
    while (true) {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "CONFIGURATION SUBMENU\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "Current Settings:\n";
        std::cout << "  1. CPU Threshold:     " << monitor.getCPUThreshold() << "%\n";
        std::cout << "  2. Memory Threshold:  " << monitor.getMemoryThresholdMB() << " MB\n";
        std::cout << "  3. Refresh Interval:  " << monitor.getRefreshIntervalSec() << " seconds\n";
        std::cout << "  4. Return to Main Menu\n";
        std::cout << "Select setting to modify (1-4): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid choice.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 4) break;

        if (choice == 1) {
            std::cout << "Enter new CPU threshold percentage (1 - 100): ";
            double val = 0.0;
            if (std::cin >> val && val >= 1.0 && val <= 100.0) {
                monitor.setCPUThreshold(val);
                std::cout << "[+] CPU threshold updated to " << val << "%.\n";
                Logger::getInstance().info("Configuration: CPU threshold updated to " + std::to_string(val) + "%");
            } else {
                std::cin.clear();
                std::cout << "[!] Invalid value. CPU threshold must be between 1 and 100.\n";
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else if (choice == 2) {
            std::cout << "Enter new Memory threshold in MB (> 0): ";
            double val = 0.0;
            if (std::cin >> val && val > 0.0) {
                monitor.setMemoryThresholdMB(val);
                std::cout << "[+] Memory threshold updated to " << val << " MB.\n";
                Logger::getInstance().info("Configuration: Memory threshold updated to " + std::to_string(val) + " MB");
            } else {
                std::cin.clear();
                std::cout << "[!] Invalid value. Memory threshold must be greater than 0.\n";
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else if (choice == 3) {
            std::cout << "Enter new Refresh interval in seconds (> 0): ";
            int val = 0;
            if (std::cin >> val && val > 0) {
                monitor.setRefreshIntervalSec(val);
                std::cout << "[+] Refresh interval updated to " << val << " seconds.\n";
                Logger::getInstance().info("Configuration: Refresh interval updated to " + std::to_string(val) + "s");
            } else {
                std::cin.clear();
                std::cout << "[!] Invalid value. Refresh interval must be greater than 0.\n";
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            std::cout << "[!] Invalid selection.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Check command line arguments for quick agent mode
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--agent" || arg1 == "-a") {
            std::string masterHost = (argc > 2) ? argv[2] : "127.0.0.1";
            int masterPort = (argc > 3) ? std::stoi(argv[3]) : 8080;
            std::string authKey = (argc > 4) ? argv[4] : "ProcessShield-OS-2026";
            Agent agent(masterHost, masterPort, 5, authKey);
            agent.run();
            return 0;
        }
    }

    Logger::getInstance().info("ProcessShield: Application session started");

    ResourceMonitor monitor(70.0, 500.0, 5);
    ProcessController controller;
    DeadlockSimulator deadlockSim;
    BankersAlgorithm bankers;

    bool running = true;
    while (running) {
        monitor.refresh();
        printHeader(monitor);

        std::cout << "\nMAIN MENU:\n";
        std::cout << " 1. View Running Processes\n";
        std::cout << " 2. Monitor CPU Usage\n";
        std::cout << " 3. Monitor Memory Usage\n";
        std::cout << " 4. Process Management (Details, Pause, Resume, Terminate)\n";
        std::cout << " 5. Deadlock Detection (Simulation & Wait-For Graph)\n";
        std::cout << " 6. Banker's Algorithm (Safe/Unsafe State & Resource Requests)\n";
        std::cout << " 7. View Alerts\n";
        std::cout << " 8. View Logs\n";
        std::cout << " 9. Configuration\n";
        std::cout << " 10. Distributed Network Agent (Report to Master Server)\n";
        std::cout << " 11. Exit\n";
        std::cout << "========================================================================================\n";
        std::cout << "Enter choice (1-11): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "[!] Invalid input. Please enter a number between 1 and 11.\n";
            pauseForUser();
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
            case 1:
                monitor.refresh();
                monitor.displayRunningProcesses(25);
                pauseForUser();
                break;
            case 2:
                monitor.runCPUAnalysis(true);
                pauseForUser();
                break;
            case 3:
                monitor.runMemoryAnalysis(true);
                pauseForUser();
                break;
            case 4:
                handleProcessManagement(monitor, controller);
                break;
            case 5:
                handleDeadlockMenu(deadlockSim);
                break;
            case 6:
                handleBankersMenu(bankers);
                break;
            case 7:
                handleAlertsMenu();
                pauseForUser();
                break;
            case 8:
                handleLogsMenu();
                pauseForUser();
                break;
            case 9:
                handleConfigMenu(monitor);
                break;
            case 10:
                handleAgentMenu();
                pauseForUser();
                break;
            case 11:
                std::cout << "\n[*] Shutting down ProcessShield... Goodbye!\n";
                Logger::getInstance().info("ProcessShield: Application session terminated cleanly");
                running = false;
                break;
            default:
                std::cout << "[!] Invalid choice (" << choice << "). Please select 1-11.\n";
                pauseForUser();
                break;
        }
    }

    return 0;
}

