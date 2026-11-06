# ProcessShield — Demonstration & Viva Presentation Guide

This guide provides a structured, step-by-step 5–10 minute demonstration sequence for evaluators, project reviews, and viva examinations.

---

## Prerequisites & Setup

Open two Terminal tabs/windows on your Mac:
* **Terminal 1**: Used for running `ProcessShield`.
* **Terminal 2**: Used for running controlled test workload processes (`cpu_stress`, `memory_test`).

Ensure the project is compiled:
```bash
cd ~/Downloads/ProcessShield
make clean
make
make tests
```

---

## 10-Minute Live Demonstration Walkthrough

### Step 1: Launch ProcessShield
In **Terminal 1**, run:
```bash
./processshield
```
**Talking Points / Viva Concept:**
* Notice the real-time system header displaying live Total CPU %, Resident Physical Memory (MB), active process count, active alert count, and dynamic thresholds.
* Highlight that ProcessShield is designed natively for macOS (using POSIX APIs and BSD process querying rather than Linux-only `/proc`).

---

### Step 2: Display Running Processes
1. In the main menu, type `1` and press Enter.
2. Observe the tabular listing showing real macOS processes currently active on the host machine.
3. Press Enter to return to the main menu.

**Talking Points / Viva Concept:**
* Demonstrates OS **Process Management** and **Process States** (`RUNNING`, `SLEEPING`, `STOPPED`, `ZOMBIE`).
* Live metrics parsed safely: PID, PPID, Process Name, State, CPU %, Resident Set Size (RSS Memory), Accumulated CPU Time, and Username.

---

### Step 3: Launch CPU Stress Workload
In **Terminal 2**, run:
```bash
./tests/cpu_stress
```
* Note down the PID displayed on the screen (e.g. `2814`).
* The process begins executing a mathematical computation loop that safely loads one CPU core to near 100%.

---

### Step 4: Detect High CPU Utilization
In **Terminal 1** (`ProcessShield`):
1. Select Option `2` (Monitor CPU Usage).
2. ProcessShield inspects all active processes against the configured threshold (default: 70%).
3. Observe that `cpu_stress` is flagged with `[!] THRESHOLD EXCEEDED` and a `WARNING: HIGH CPU USAGE DETECTED` alert box is displayed on screen.

**Talking Points / Viva Concept:**
* Demonstrates **Resource Threshold Monitoring** and **Automated Threat Detection**.
* Alerts are simultaneously logged into `logs/processshield.log` and registered in `AlertManager`.

---

### Step 5: Pause and Resume Process via POSIX Signals
In **Terminal 1** (`ProcessShield`):
1. Select Option `4` (Process Management).
2. Select Sub-option `2` (Pause Process - SIGSTOP).
3. Enter the PID of `cpu_stress` (e.g. `2814`).
4. Notice the success confirmation: `Successfully executed PAUSE (SIGSTOP) on PID <pid>`.
5. Switch to **Terminal 2**: observe that CPU execution has frozen and the process state is paused.
6. Return to **Terminal 1**: Select Sub-option `3` (Resume Process - SIGCONT).
7. Enter the PID again. Notice execution immediately resumes in Terminal 2.

**Talking Points / Viva Concept:**
* Explains **POSIX Signal Delivery** (`SIGSTOP` cannot be caught or ignored, forcing the scheduler to suspend the process; `SIGCONT` restores it to the ready queue).

---

### Step 6: Graceful Process Termination
In **Terminal 1** (`ProcessShield`):
1. Select Sub-option `4` (Terminate Process - SIGTERM).
2. Enter the PID of `cpu_stress`.
3. ProcessShield prompts: `Are you sure you want to terminate PID <pid>? (y/n):`
4. Type `y` and press Enter.
5. Notice the message: `Successfully executed TERMINATE (SIGTERM) on PID <pid>`.
6. Switch to **Terminal 2**: notice that `cpu_stress` caught `SIGTERM`, exited cleanly, and closed.

**Talking Points / Viva Concept:**
* Demonstrates **Controlled Process Termination** with safeguards against accidental termination of critical OS system daemons (PID 0, PID 1 `launchd`, self-PID).

---

### Step 7: Memory Threshold Monitoring
1. In **Terminal 2**, launch the memory stress utility with 600 MB (exceeding the 500 MB default threshold):
   ```bash
   ./tests/memory_test 600
   ```
2. In **Terminal 1**, select Option `3` (Monitor Memory Usage).
3. Observe `memory_test` flagged with `[!] THRESHOLD EXCEEDED` (~601 MB) along with browser and background apps exceeding threshold.
4. Clean up the memory test in **Terminal 2** by pressing `Ctrl+C` or terminating it via ProcessShield Option `4`.

**Talking Points / Viva Concept:**
* Highlights difference between Virtual Memory (allocated address space) and **Resident Set Size (RSS)** (physical RAM pages actually resident and backed). The test touches each 4KB virtual page to trigger hardware page faults into RSS.

---

### Step 8: Deadlock Detection & Wait-For Graph (WFG)
In **Terminal 1** (`ProcessShield`):
1. Select Option `5` (Deadlock Detection).
2. Select Option `1` (Run Classic Deadlock Scenario).
   * Visualizes the Resource Allocation Graph (RAG):
     - P1 holds R1, requests R2 (held by P2).
     - P2 holds R2, requests R1 (held by P1).
   * Generates the directed Wait-For Graph:
     - `P1 -> P2`
     - `P2 -> P1`
   * Executes Depth-First Search (DFS) 3-color cycle detection.
   * Outputs:
     ```
     STATUS: CRITICAL — DEADLOCK DETECTED!
     Cycle Path: P1 -> P2 -> P1
     Processes Involved: P1, P2
     ```
3. Select Option `2` to demonstrate a 3-process cycle (`P1 -> P2 -> P3 -> P1`).
4. Select Option `3` to demonstrate the safe, cycle-free case (`STATUS: SAFE — NO DEADLOCK DETECTED`).
5. Return to menu (Option `5`).

**Talking Points / Viva Concept:**
* Thoroughly explains the **4 Coffman Conditions for Deadlock**:
  1. Mutual Exclusion
  2. Hold and Wait
  3. No Preemption
  4. Circular Wait
* Explains why cycles in a Wait-For Graph are necessary and sufficient for deadlocks in single-instance resource environments.

---

### Step 9: Banker's Algorithm (Deadlock Avoidance)
In **Terminal 1** (`ProcessShield`):
1. Select Option `6` (Banker's Algorithm).
2. Select Option `1` (Run Classic SAFE Scenario).
   * Displays the Matrices: Allocation, Max, Need ($Need = Max - Allocation$), and Available.
   * Executes safety search algorithm.
   * Outputs:
     ```
     System State: SAFE
     Safe Sequence Found: P1 -> P3 -> P4 -> P0 -> P2
     ```
3. Select Option `2` (Run Classic UNSAFE Scenario).
   * Demonstrates state where resource demands exceed available capacities.
   * Identifies unsatisfied processes and flags `Deadlock Risk!`.
4. Select Option `3` (Request Resource Allocation).
   * Attempts to request resources dynamically. Demonstrates safety evaluation and automatic rollback if an unsafe state would result.
5. Return to menu (Option `5`).

**Talking Points / Viva Concept:**
* Differentiates **Deadlock Avoidance** (Banker's algorithm dynamically inspecting resource states to prevent entering unsafe states) from **Deadlock Detection** (reactive cycle detection).
* Reassures evaluators that an "unsafe state" does not mean deadlock has occurred yet, but means the system cannot guarantee avoidance of deadlock if processes demand maximum claims.

---

### Step 10: Review Alerts, Logs & Runtime Configuration
In **Terminal 1** (`ProcessShield`):
1. Select Option `7` (View Alerts): review all timestamped `WARNING` and `CRITICAL` alerts accumulated during the demo.
2. Select Option `8` (View Logs): view recent log entries directly from `logs/processshield.log`.
3. Select Option `9` (Configuration):
   - Modify the CPU threshold from 70% to 85%.
   - Notice the main menu banner instantly updates without requiring recompilation!
4. Select Option `10` (Exit).

---

## Common Viva Questions & Quick Answers

| Question | Answer |
| :--- | :--- |
| **Why does ProcessShield not use `/proc`?** | macOS is a BSD-derived Darwin Unix kernel that does not mount a Linux-style `/proc` pseudo-filesystem. ProcessShield interfaces cleanly with macOS system tools (`ps -axo`) and POSIX kernel APIs. |
| **What is the difference between Virtual Memory and RSS?** | Virtual Memory is the address space reserved by the process. Resident Set Size (RSS) is the actual physical RAM occupied by pages that have been faulted into memory. ProcessShield tracks RSS. |
| **How does Wait-For Graph differ from Resource Allocation Graph?** | A Resource Allocation Graph has two types of nodes (Processes and Resources). A Wait-For Graph is a collapsed bipartite graph containing only process nodes, where an edge `Pi -> Pj` exists if `Pi` is waiting for a resource held by `Pj`. |
| **How does your cycle detection work?** | Uses Depth-First Search (DFS) with a 3-color state machine: White (unvisited), Gray (currently in recursion stack), and Black (completed). An edge pointing to a Gray node represents a back-edge, proving the existence of a circular dependency. |
| **Is an Unsafe state in Banker's algorithm always deadlocked?** | No. An unsafe state means there is no sequence of processes that can be guaranteed to finish if all processes suddenly request their maximum claim. Deadlock is only a possibility, not a certainty, but the OS avoids unsafe states to guarantee safety. |
