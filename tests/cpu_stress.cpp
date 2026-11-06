#include <iostream>
#include <unistd.h>
#include <csignal>
#include <cmath>
#include <atomic>
#include <chrono>
#include <thread>

std::atomic<bool> keepRunning(true);

void signalHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        std::cout << "\n[cpu_stress] Received signal " << sig << ". Exiting gracefully...\n";
        keepRunning = false;
    }
}

int main() {
    // Register signal handlers
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    pid_t pid = getpid();
    std::cout << "============================================================\n";
    std::cout << "           ProcessShield - CPU Stress Test Process          \n";
    std::cout << "============================================================\n";
    std::cout << "  Process Name: cpu_stress\n";
    std::cout << "  PID:          " << pid << "\n";
    std::cout << "  Status:       Generating controlled CPU load on 1 core...\n";
    std::cout << "  Control:      Can be paused (SIGSTOP), resumed (SIGCONT),\n";
    std::cout << "                or terminated (SIGTERM) from ProcessShield.\n";
    std::cout << "  To stop manually: Press Ctrl+C or kill " << pid << "\n";
    std::cout << "============================================================\n";

    // Intensive calculation loop
    volatile double dummy = 0.0;
    long long counter = 0;
    while (keepRunning) {
        for (int i = 0; i < 500000; ++i) {
            dummy += std::sin(i * 0.001) * std::cos(i * 0.002);
        }
        counter++;
        if (counter % 40 == 0) {
            // Slight yield to remain responsive to signals
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::cout << "[cpu_stress] Terminated successfully.\n";
    return 0;
}
