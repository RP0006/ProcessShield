# ProcessShield — Intelligent Process Resource Guardian

A terminal-based Operating Systems resource monitoring, process control, and deadlock analysis application built natively for **macOS** in **C++17**.

---

## 🌟 Overview

Operating Systems allocate physical hardware (CPU, physical memory, device resources) among competing processes while preserving system responsiveness and preventing deadlocks. **ProcessShield** bridges classical Operating Systems theoretical principles with modern systems programming on macOS.

---

## 🚀 Key Features & Operating Systems Concepts

| Module | Core OS Concepts Demonstrated |
| :--- | :--- |
| **Process Monitor** | Real-time process telemetry, Process States (`RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`), PID/PPID hierarchy, CPU %, Resident Set Size (RSS Memory), User permissions. |
| **CPU Analyzer** | Dynamic threshold detection, CPU core utilization tracking, automated WARNING alert generation. |
| **Memory Analyzer** | Resident physical memory analysis (RSS vs. virtual address space), memory leak/spike detection. |
| **Process Controller** | POSIX signal delivery (`kill(pid, SIGSTOP)`, `kill(pid, SIGCONT)`, `kill(pid, SIGTERM)`), system process protection, graceful error handling (`EPERM`, `ESRCH`). |
| **Deadlock Simulator** | Resource Allocation Graphs (RAG), Wait-For Graphs (WFG), the 4 Coffman conditions, circular wait simulation without OS destabilization. |
| **Wait-For Graph** | Directed dependency graphs, Depth-First Search (DFS) 3-color cycle detection ($O(V + E)$), cycle path reconstruction. |
| **Banker's Algorithm** | Dijkstra's Deadlock Avoidance, Safe vs. Unsafe states, Need matrix ($Need = Max - Alloc$), Safe sequence generation, dynamic resource requests with rollback. |
| **Alert Manager** | Centralized multilevel event alerts (`INFO`, `WARNING`, `CRITICAL`). |
| **System Logger** | Thread-safe persistent audit logging with ISO timestamps into `logs/processshield.log`. |
| **Runtime Config** | Dynamic threshold and interval configuration without recompilation. |

---

## 📂 Directory Structure

```
~/Downloads/ProcessShield/
├── Makefile                # Build system (clang++, C++17, -Wall -Wextra)
├── README.md               # Project documentation
├── processshield           # Main compiled executable
│
├── src/                    # C++ source and header files
│   ├── main.cpp            # Interactive menu and application entry point
│   ├── ProcessManager.h/cpp# macOS process query engine (ps parser)
│   ├── ResourceMonitor.h/cpp# Telemetry coordinator and threshold engine
│   ├── CPUAnalyzer.h/cpp   # High CPU utilization detector
│   ├── MemoryAnalyzer.h/cpp# High memory (RSS) detector
│   ├── ProcessController.h/cpp# POSIX signal process controller
│   ├── DeadlockSimulator.h/cpp# Safe internal RAG simulator
│   ├── WaitForGraph.h/cpp  # DFS cycle detection & WFG graph
│   ├── BankersAlgorithm.h/cpp# Banker's safety & resource allocation algorithm
│   ├── AlertManager.h/cpp  # Centralized alert repository
│   └── Logger.h/cpp        # Thread-safe timestamped file logger
│
├── tests/                  # Workload and test programs
│   ├── cpu_stress.cpp      # Safe mathematical CPU stress generator
│   ├── memory_test.cpp     # Memory allocation & page touch stress tool
│   └── deadlock_test.cpp   # Automated unit and algorithmic test suite
│
├── docs/                   # Documentation and reports
│   ├── DEMONSTRATION.md    # 5-10 minute viva presentation walkthrough
│   └── PROJECT_REPORT.md   # Comprehensive academic semester project report
│
└── logs/                   # System runtime logs
    └── processshield.log   # Persistent event log
```

---

## 🛠️ Build & Installation

### Prerequisites
* **Operating System:** macOS (Apple Silicon or Intel)
* **Compiler:** `clang++` with C++17 support
* **Build Tool:** GNU Make

### Compilation
From the root directory:
```bash
# Clean previous artifacts
make clean

# Compile the main application
make

# Compile the test workload programs
make tests
```

---

## 🖥️ Running ProcessShield

Launch the interactive guardian:
```bash
./processshield
```

### Main Menu Navigation
```
========================================================================================
                         PROCESS SHIELD - RESOURCE GUARDIAN                             
                             Intelligent Process Monitor                                
========================================================================================
 System CPU: 24.5% | System Memory: 16210.4 MB | Active Processes: 682 | Alerts: 0
 Thresholds: CPU >= 70.0%, Memory >= 500.0 MB | Log: logs/processshield.log
========================================================================================

MAIN MENU:
 1. View Running Processes
 2. Monitor CPU Usage
 3. Monitor Memory Usage
 4. Process Management (Details, Pause, Resume, Terminate)
 5. Deadlock Detection (Simulation & Wait-For Graph)
 6. Banker's Algorithm (Safe/Unsafe State & Resource Requests)
 7. View Alerts
 8. View Logs
 9. Configuration
 10. Exit
========================================================================================
```

---

## 🧪 Testing & Verification

### 1. Automated Algorithmic Test Suite
Run the built-in unit tests verifying WFG cycle detection and Banker's algorithm:
```bash
./tests/deadlock_test
```
*Outputs:*
```
[Test 1] Wait-For Graph: 2-Process Cycle Detection (P1 <-> P2)...   --> PASS
[Test 2] Wait-For Graph: Acyclic Graph (P1 -> P2 -> P3)...          --> PASS
[Test 3] Deadlock Simulator: Classic RAG Simulation...               --> PASS
[Test 4] Banker's Algorithm: Classic Safe Case...                   --> PASS
[Test 5] Banker's Algorithm: Unsafe Case...                         --> PASS
ALL ALGORITHMIC TESTS PASSED SUCCESSFULLY! (5/5)
```

### 2. Live CPU Threshold & Process Control Test
1. In a separate terminal, launch:
   ```bash
   ./tests/cpu_stress
   ```
2. In ProcessShield, choose Option `2` (Monitor CPU Usage). ProcessShield flags `cpu_stress` exceeding 70% CPU.
3. Choose Option `4` -> Sub-option `2` (Pause) or `4` (Terminate) to control the process via POSIX signals.

### 3. Live Memory Threshold Test
1. In a separate terminal, launch:
   ```bash
   ./tests/memory_test 600
   ```
2. In ProcessShield, choose Option `3` (Monitor Memory Usage). ProcessShield flags the process exceeding 500 MB RAM.

---

## 🌐 Distributed Fleet Monitoring (LAN / Intranet)

ProcessShield features a **Master Server + Agent + Web Dashboard + Simulation** architecture that allows monitoring multiple computers over a local network.

```
       [ Real Mac Computer ]                     [ Remote LAN Computer ]
      (runs processshield-agent)                 (runs agent.py or agent)
                  │                                         │
                  ▼                                         ▼
            HTTP POST /api/report                     HTTP POST /api/report
                  └───────────────────► [ MASTER SERVER ] ◄─────────────────┘
                                        (master_server.py)
                                                ▲
                                                │ HTTP POST /api/report
                                     [ SIMULATOR FLEET ]
                                        (simulator.py)
                                                │
                                                ▼
                                    [ WEB DASHBOARD (UI) ]
                                    http://localhost:8080
```

### 1. Automated Master Server & Web Dashboard
Collects telemetry, validates security tokens, writes to SQLite, responds to UDP discovery beacons, and serves the dashboard:
```bash
python3 master_server.py --port 8080
```
* **Active Ethernet Detection:** Automatically detects actual physical Ethernet IP (e.g., `en2`/`en3`) and fallback LAN/Wi-Fi (`en0`).
* **Persistent Database:** Retains all node records and telemetry in `data/processshield.db` across system reboots.
* **Auto-Discovery Responder:** Announces Master presence on UDP port `8081`.

### 2. Automated Monitoring Agent (Real Computers)
Runs continuously in background, auto-reconnects across network drops, and streams CPU, RAM, disk, network, and processes every 5 seconds:
* **Python Agent (Zero-Configuration with Auto-Discovery):**
  ```bash
  python3 agent.py
  ```
  *(Automatically locates Master on Ethernet LAN via UDP! Or specify target: `python3 agent.py --server http://<MASTER_IP>:8080`)*
* **Native C++ Agent:**
  ```bash
  ./processshield-agent --host <MASTER_IP> --port 8080 --key ProcessShield-OS-2026
  ```

### 3. Automatic Startup on System Reboot (macOS & Linux Daemons)
To ensure monitoring starts automatically when computers power on without needing to open a terminal:
* **macOS (LaunchAgent):**
  ```bash
  ./scripts/install_macos_service.sh master   # Installs Master Server daemon
  ./scripts/install_macos_service.sh agent    # Installs Client Agent daemon
  ```
* **Linux (Systemd):**
  ```bash
  sudo ./scripts/install_linux_service.sh master
  sudo ./scripts/install_linux_service.sh agent
  ```

### 4. Multi-Computer Simulation Fleet
For testing without physical computers:
```bash
python3 simulator.py --server http://127.0.0.1:8080
```
* Simulates virtual machines (`ubuntu-web-srv-01`, `win11-eng-station-02`, `pg-cluster-primary-03`) with clearly labelled `SIMULATED VM` badges.
* Real computers are distinctly highlighted with `★ REAL SYSTEM` emerald borders and hardware MAC addresses.

### 5. Automated Daily Reporting & Historical Metrics
* Generates daily reports for every computer automatically at midnight (or on demand via the dashboard).
* Reports include Average/Peak CPU, Average/Peak RAM, Storage load, Uptime %, and alert logs stored in `reports/` and SQLite.

---

## 📖 Documentation
* [Demonstration Guide](docs/DEMONSTRATION.md): Complete 5–10 minute viva walkthrough with exact commands, outputs, and sample examiner questions.
* [Project Report](docs/PROJECT_REPORT.md): Full academic semester project report covering theoretical foundations, design, algorithms, complexity analysis, and results.

