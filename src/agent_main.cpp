#include "Agent.h"
#include <iostream>
#include <string>

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --host <ip/hostname>  Master Server IP or Hostname (default: 127.0.0.1)\n"
              << "  --port <port>         Master Server Port (default: 8080)\n"
              << "  --key <key>           Authorization Key (default: ProcessShield-OS-2026)\n"
              << "  --interval <sec>      Reporting interval in seconds (default: 5)\n"
              << "  --once                Send one report and exit\n"
              << "  --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string masterHost = "127.0.0.1";
    int masterPort = 8080;
    int intervalSec = 5;
    std::string authKey = "ProcessShield-OS-2026";
    bool once = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            masterHost = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            masterPort = std::stoi(argv[++i]);
        } else if (arg == "--key" && i + 1 < argc) {
            authKey = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            intervalSec = std::stoi(argv[++i]);
        } else if (arg == "--once") {
            once = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    Agent agent(masterHost, masterPort, intervalSec, authKey);

    if (once) {
        std::cout << "[*] Sending single telemetry report to " << masterHost << ":" << masterPort << "...\n";
        bool ok = agent.collectAndSendOnce();
        return ok ? 0 : 1;
    }

    agent.run();
    return 0;
}
