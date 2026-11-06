#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>

std::atomic<bool> keepRunning(true);

void signalHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        std::cout << "\n[memory_test] Received signal " << sig << ". Freeing memory and exiting...\n";
        keepRunning = false;
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    size_t targetMB = 200; // default safe 200 MB
    if (argc > 1) {
        try {
            long val = std::stol(argv[1]);
            if (val > 0 && val <= 2048) {
                targetMB = static_cast<size_t>(val);
            } else {
                std::cout << "[!] Requested memory (" << val << " MB) out of safe test range (1 - 2048 MB). Using 200 MB.\n";
            }
        } catch (...) {
            std::cout << "[!] Invalid argument. Using default 200 MB.\n";
        }
    }

    pid_t pid = getpid();
    std::cout << "============================================================\n";
    std::cout << "          ProcessShield - Memory Test Process               \n";
    std::cout << "============================================================\n";
    std::cout << "  Process Name: memory_test\n";
    std::cout << "  PID:          " << pid << "\n";
    std::cout << "  Target Memory:" << targetMB << " MB\n";
    std::cout << "  Allocating and touching memory pages...\n";

    size_t totalBytes = targetMB * 1024 * 1024;
    char* buffer = nullptr;

    try {
        buffer = new char[totalBytes];
        // Ensure memory pages are actually faulted into resident physical RAM (RSS)
        // using memset and volatile access so clang -O2 cannot optimize it away.
        std::memset(buffer, 0x5A, totalBytes);
        volatile char* vbuf = buffer;
        for (size_t i = 0; i < totalBytes; i += 4096) {
            vbuf[i] = static_cast<char>((i / 4096) % 120 + 1);
        }
    } catch (const std::bad_alloc& e) {
        std::cerr << "[!] Memory allocation failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "  Status:       Successfully committed " << targetMB << " MB into resident memory (RSS)!\n";
    std::cout << "  Control:      ProcessShield will detect this as high memory if threshold < " << targetMB << " MB.\n";
    std::cout << "  To stop manually: Press Ctrl+C or kill " << pid << "\n";
    std::cout << "============================================================\n";

    // Keep holding the memory until signaled
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    delete[] buffer;
    std::cout << "[memory_test] Memory freed. Process exiting.\n";
    return 0;
}
