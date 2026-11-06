#include "Agent.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

Agent::Agent(const std::string& masterHost, int masterPort, int intervalSec, const std::string& authKey)
    : masterHost_(masterHost), masterPort_(masterPort), intervalSec_(intervalSec), authKey_(authKey) {}

std::string Agent::detectHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "Unknown-Mac";
}

std::string Agent::detectLocalIP() {
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) {
        return "127.0.0.1";
    }

    std::string candidateIP = "127.0.0.1";
    for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (!(ifa->ifa_flags & IFF_LOOPBACK) && (ifa->ifa_flags & IFF_UP)) {
                char host[NI_MAXHOST];
                if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0) {
                    std::string ip = host;
                    if (ip != "127.0.0.1") {
                        candidateIP = ip;
                        // On macOS, 'en0' is typically primary Wi-Fi / Ethernet
                        std::string ifName = ifa->ifa_name;
                        if (ifName == "en0" || ifName.rfind("en", 0) == 0) {
                            break;
                        }
                    }
                }
            }
        }
    }
    freeifaddrs(ifaddr);
    return candidateIP;
}

bool Agent::getDiskStats(double& totalGB, double& usedGB, double& percent) {
    struct statvfs vfs;
    if (statvfs("/", &vfs) != 0) {
        totalGB = 0; usedGB = 0; percent = 0;
        return false;
    }
    uint64_t totalBytes = (uint64_t)vfs.f_blocks * vfs.f_frsize;
    uint64_t freeBytes = (uint64_t)vfs.f_bavail * vfs.f_frsize;
    uint64_t usedBytes = (totalBytes > freeBytes) ? (totalBytes - freeBytes) : 0;

    totalGB = (double)totalBytes / (1024.0 * 1024.0 * 1024.0);
    usedGB = (double)usedBytes / (1024.0 * 1024.0 * 1024.0);
    percent = (totalBytes > 0) ? ((double)usedBytes / totalBytes * 100.0) : 0.0;
    return true;
}

bool Agent::getRAMStats(double& totalGB, double& usedGB, double& percent) {
    // 1. Total physical RAM via sysctl
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    int64_t totalRamBytes = 0;
    size_t length = sizeof(totalRamBytes);
    if (sysctl(mib, 2, &totalRamBytes, &length, NULL, 0) != 0) {
        totalGB = 16.0; usedGB = 8.0; percent = 50.0;
        return false;
    }

    // 2. Used RAM via Mach host statistics
    vm_size_t pageSize = 4096;
    mach_port_t machHost = mach_host_self();
    host_page_size(machHost, &pageSize);

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = sizeof(vmStats) / sizeof(natural_t);
    uint64_t usedRamBytes = 0;

    if (host_statistics64(machHost, HOST_VM_INFO64, (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
        uint64_t activeMem = (uint64_t)vmStats.active_count * pageSize;
        uint64_t wireMem = (uint64_t)vmStats.wire_count * pageSize;
        uint64_t compressedMem = (uint64_t)vmStats.compressor_page_count * pageSize;
        usedRamBytes = activeMem + wireMem + compressedMem;
    } else {
        usedRamBytes = totalRamBytes / 2;
    }

    totalGB = (double)totalRamBytes / (1024.0 * 1024.0 * 1024.0);
    usedGB = (double)usedRamBytes / (1024.0 * 1024.0 * 1024.0);
    percent = (totalRamBytes > 0) ? ((double)usedRamBytes / totalRamBytes * 100.0) : 0.0;
    return true;
}

std::string Agent::evaluateHealth(double cpuPct, double ramPct, double diskPct, std::vector<std::string>& reasons) {
    reasons.clear();
    bool isCritical = false;
    bool isWarning = false;

    if (cpuPct >= 85.0) {
        reasons.push_back("Severe CPU overload (" + std::to_string((int)cpuPct) + "%)");
        isCritical = true;
    } else if (cpuPct >= 70.0) {
        reasons.push_back("Elevated CPU utilization (" + std::to_string((int)cpuPct) + "%)");
        isWarning = true;
    }

    if (ramPct >= 90.0) {
        reasons.push_back("Critically low physical memory (" + std::to_string((int)ramPct) + "%)");
        isCritical = true;
    } else if (ramPct >= 75.0) {
        reasons.push_back("High memory usage (" + std::to_string((int)ramPct) + "%)");
        isWarning = true;
    }

    if (diskPct >= 90.0) {
        reasons.push_back("Root storage critically full (" + std::to_string((int)diskPct) + "%)");
        isCritical = true;
    } else if (diskPct >= 80.0) {
        reasons.push_back("Low disk free space (" + std::to_string((int)diskPct) + "%)");
        isWarning = true;
    }

    if (isCritical) {
        return "CRITICAL";
    }
    if (isWarning) {
        return "WARNING";
    }
    reasons.push_back("All operating system resources within normal parameters");
    return "HEALTHY";
}

std::string Agent::escapeJson(const std::string& s) const {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if ((unsigned char)c < 0x20) {
            // control char
        } else {
            o << c;
        }
    }
    return o.str();
}

std::string Agent::buildJsonPayload(const NodeTelemetry& tele) const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(1);

    json << "{\n";
    json << "  \"node_id\": \"" << escapeJson(tele.nodeId) << "\",\n";
    json << "  \"hostname\": \"" << escapeJson(tele.hostname) << "\",\n";
    json << "  \"ip\": \"" << escapeJson(tele.ip) << "\",\n";
    json << "  \"is_simulated\": " << (tele.isSimulated ? "true" : "false") << ",\n";
    json << "  \"system_type\": \"" << escapeJson(tele.systemType) << "\",\n";
    json << "  \"cpu_percent\": " << tele.cpuPercent << ",\n";
    json << "  \"ram_used_gb\": " << tele.ramUsedGB << ",\n";
    json << "  \"ram_total_gb\": " << tele.ramTotalGB << ",\n";
    json << "  \"ram_percent\": " << tele.ramPercent << ",\n";
    json << "  \"disk_used_gb\": " << tele.diskUsedGB << ",\n";
    json << "  \"disk_total_gb\": " << tele.diskTotalGB << ",\n";
    json << "  \"disk_percent\": " << tele.diskPercent << ",\n";
    json << "  \"process_count\": " << tele.processCount << ",\n";
    json << "  \"health\": \"" << escapeJson(tele.health) << "\",\n";
    json << "  \"auth_key\": \"" << escapeJson(authKey_) << "\",\n";

    // health reasons
    json << "  \"health_reasons\": [";
    for (size_t i = 0; i < tele.healthReasons.size(); ++i) {
        json << "\"" << escapeJson(tele.healthReasons[i]) << "\"";
        if (i + 1 < tele.healthReasons.size()) json << ", ";
    }
    json << "],\n";

    // top processes list (up to 25)
    json << "  \"processes\": [\n";
    for (size_t i = 0; i < tele.topProcesses.size(); ++i) {
        const auto& p = tele.topProcesses[i];
        json << "    {";
        json << "\"pid\": " << p.pid << ", ";
        json << "\"ppid\": " << p.ppid << ", ";
        json << "\"name\": \"" << escapeJson(p.name) << "\", ";
        json << "\"user\": \"" << escapeJson(p.user) << "\", ";
        json << "\"state\": \"" << escapeJson(p.stateDescription) << "\", ";
        json << "\"cpu_percent\": " << p.cpuPercent << ", ";
        json << "\"memory_mb\": " << p.rssMB;
        json << "}";
        if (i + 1 < tele.topProcesses.size()) json << ",\n";
        else json << "\n";
    }
    json << "  ]\n";
    json << "}";

    return json.str();
}

bool Agent::sendHttpReport(const std::string& jsonPayload) const {
    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(masterPort_);
    if (getaddrinfo(masterHost_.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        std::cerr << "[-] Error resolving Master Server host: " << masterHost_ << "\n";
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        std::cerr << "[-] Error creating socket\n";
        return false;
    }

    // Set 3-second timeout for connect/send/recv
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        std::cerr << "[-] Connection to Master Server (" << masterHost_ << ":" << masterPort_ << ") refused.\n";
        return false;
    }
    freeaddrinfo(res);

    // Build HTTP/1.1 POST
    std::ostringstream httpReq;
    httpReq << "POST /api/report HTTP/1.1\r\n"
            << "Host: " << masterHost_ << ":" << masterPort_ << "\r\n"
            << "User-Agent: ProcessShield-Agent/2.0 (Darwin)\r\n"
            << "X-ProcessShield-Key: " << authKey_ << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << jsonPayload.length() << "\r\n"
            << "Connection: close\r\n\r\n"
            << jsonPayload;

    std::string reqStr = httpReq.str();
    ssize_t sent = send(sock, reqStr.c_str(), reqStr.length(), 0);
    if (sent < (ssize_t)reqStr.length()) {
        close(sock);
        std::cerr << "[-] Incomplete data sent to Master Server\n";
        return false;
    }

    // Read response header
    char buf[512];
    ssize_t recvd = recv(sock, buf, sizeof(buf) - 1, 0);
    close(sock);

    if (recvd > 0) {
        buf[recvd] = '\0';
        if (std::strstr(buf, "200 OK") != nullptr) {
            return true;
        }
    }
    return false;
}

bool Agent::collectAndSendOnce() {
    processManager_.refresh();

    NodeTelemetry tele;
    tele.hostname = detectHostname();
    tele.ip = detectLocalIP();
    tele.nodeId = tele.hostname + " (" + tele.ip + ")";
    tele.isSimulated = false; // Real hardware Mac
    tele.systemType = "macOS (Real Mac)";

    // CPU normalized by core count
    int ncpu = 1;
    size_t ncpuLen = sizeof(ncpu);
    if (sysctlbyname("hw.ncpu", &ncpu, &ncpuLen, NULL, 0) != 0 || ncpu < 1) {
        ncpu = 1;
    }
    double rawCpu = processManager_.getTotalCPUUsage();
    tele.cpuPercent = std::min(100.0, rawCpu / (double)ncpu);
    tele.processCount = processManager_.getProcessCount();
    
    // Top 25 processes sorted by CPU usage
    tele.topProcesses = processManager_.getTopProcessesByCPU(25);

    // RAM & Disk
    getRAMStats(tele.ramTotalGB, tele.ramUsedGB, tele.ramPercent);
    getDiskStats(tele.diskTotalGB, tele.diskUsedGB, tele.diskPercent);

    // Evaluate health
    tele.health = evaluateHealth(tele.cpuPercent, tele.ramPercent, tele.diskPercent, tele.healthReasons);

    std::string json = buildJsonPayload(tele);
    bool ok = sendHttpReport(json);

    std::cout << "[Agent] [" << (ok ? "SENT" : "FAILED") << "] Node: " << tele.hostname
              << " | Real IP: " << tele.ip 
              << " | CPU: " << std::fixed << std::setprecision(1) << tele.cpuPercent << "%"
              << " | RAM: " << tele.ramPercent << "% (" << tele.ramUsedGB << "/" << tele.ramTotalGB << " GB)"
              << " | Disk: " << tele.diskPercent << "%"
              << " | Health: " << tele.health
              << " | Procs: " << tele.processCount << "\n";

    return ok;
}

void Agent::run() {
    std::cout << "====================================================================\n";
    std::cout << "        PROCESSSHIELD REAL-SYSTEM AGENT (C++17 macOS)               \n";
    std::cout << "====================================================================\n";
    std::cout << " [*] Hostname:       " << detectHostname() << "\n";
    std::cout << " [*] Actual Local IP:" << detectLocalIP() << " (REAL MAC HARDWARE)\n";
    std::cout << " [*] Target Master:  " << masterHost_ << ":" << masterPort_ << "\n";
    std::cout << " [*] Interval:       " << intervalSec_ << " seconds\n";
    std::cout << " [*] Press Ctrl+C to terminate Agent.\n";
    std::cout << "====================================================================\n\n";

    while (true) {
        collectAndSendOnce();
        sleep(intervalSec_);
    }
}
