#include "ProcessController.h"
#include "AlertManager.h"
#include "Logger.h"
#include <csignal>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

bool ProcessController::isProtectedProcess(pid_t pid) const {
    if (pid <= 1) return true;
    if (pid == getpid()) return true;
    return false;
}

ControlResult ProcessController::sendSignal(pid_t pid, int signal, const std::string& actionName) {
    if (pid <= 0) {
        return {false, "Error: Invalid PID provided (" + std::to_string(pid) + ")"};
    }

    if (isProtectedProcess(pid)) {
        std::string err = "Action BLOCKED: PID " + std::to_string(pid) + " is a protected system/self process!";
        Logger::getInstance().warning("ProcessController: " + err);
        return {false, err};
    }

    if (kill(pid, signal) == 0) {
        std::string successMsg = "Successfully executed " + actionName + " on PID " + std::to_string(pid);
        Logger::getInstance().info("ProcessController: " + successMsg);
        AlertManager::getInstance().addAlert(
            AlertLevel::INFO,
            "Process Controller",
            successMsg,
            "Signal: " + std::to_string(signal)
        );
        return {true, successMsg};
    }

    // Handle error
    std::string errDetail;
    switch (errno) {
        case EPERM:
            errDetail = "Permission denied. (Process owned by another user/root)";
            break;
        case ESRCH:
            errDetail = "Process does not exist or has already exited.";
            break;
        case EINVAL:
            errDetail = "Invalid signal specified.";
            break;
        default:
            errDetail = std::strerror(errno);
            break;
    }

    std::string fullError = "Failed to " + actionName + " PID " + std::to_string(pid) + ": " + errDetail;
    Logger::getInstance().warning("ProcessController: " + fullError);
    return {false, fullError};
}

ControlResult ProcessController::pauseProcess(pid_t pid) {
    return sendSignal(pid, SIGSTOP, "PAUSE (SIGSTOP)");
}

ControlResult ProcessController::resumeProcess(pid_t pid) {
    return sendSignal(pid, SIGCONT, "RESUME (SIGCONT)");
}

ControlResult ProcessController::terminateProcess(pid_t pid, bool confirmed) {
    if (!confirmed) {
        return {false, "Termination cancelled: Confirmation was not provided."};
    }
    return sendSignal(pid, SIGTERM, "TERMINATE (SIGTERM)");
}

void ProcessController::displayProcessDetails(pid_t pid, const ProcessManager& pm) {
    auto optProc = pm.getProcessByPid(pid);
    if (!optProc.has_value()) {
        std::cout << "\n[!] Process with PID " << pid << " was not found in current process snapshot.\n";
        return;
    }

    const auto& p = optProc.value();
    std::cout << "\n============================================================\n";
    std::cout << "PROCESS DETAILS: " << p.name << " (PID: " << p.pid << ")\n";
    std::cout << "============================================================\n";
    std::cout << "  Process Name:       " << p.name << "\n";
    std::cout << "  Full Command / Path:" << p.fullPath << "\n";
    std::cout << "  PID:                " << p.pid << "\n";
    std::cout << "  Parent PID (PPID):  " << p.ppid << "\n";
    std::cout << "  User:               " << p.user << "\n";
    std::cout << "  State (Raw / Full): " << p.rawState << " (" << p.stateDescription << ")\n";
    std::cout << "  CPU Usage:          " << std::fixed << std::setprecision(1) << p.cpuPercent << "%\n";
    std::cout << "  Memory (RSS):       " << std::fixed << std::setprecision(1) << p.rssMB << " MB (" << p.rssKB << " KB)\n";
    std::cout << "  Total Run Time:     " << p.runTime << "\n";
    std::cout << "  Is System/Protected:" << (isProtectedProcess(p.pid) ? " YES" : " NO") << "\n";
    std::cout << "============================================================\n";
}
