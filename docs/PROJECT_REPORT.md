# ProcessShield — Intelligent Process Resource Guardian
## Academic Operating Systems Project Report

---

## 1. Abstract & Objective
Operating Systems manage hardware resources—CPU, physical memory, and I/O devices—among concurrent, competing user processes. When software errors, infinite loops, or memory leaks occur, rogue processes can monopolize CPU cores or deplete system memory, starving critical background services. Furthermore, uncoordinated concurrent resource requests can result in deadlocks where processes are blocked perpetually.

**ProcessShield** is an interactive, terminal-based resource management and system protection guardian developed natively in modern C++17 for macOS. The primary objective is to bridge classical Operating Systems theoretical principles with modern systems programming:
1. Continuous real-time process telemetry (CPU, RSS memory, process states, hierarchical relations).
2. Automated threshold monitoring with multilevel alert dispatching.
3. Safe process state manipulation using standard POSIX signals (`SIGSTOP`, `SIGCONT`, `SIGTERM`).
4. Algorithmic deadlock detection utilizing directed Wait-For Graphs and Depth-First Search cycle detection.
5. Algorithmic deadlock avoidance implementing Dijkstra's Banker's Algorithm with safe sequence computation and dynamic request simulation.
6. Persistent audit logging for forensic and post-mortem analysis.

---

## 2. Problem Statement
Many contemporary desktop environments provide graphical task managers (such as macOS Activity Monitor), yet they serve primarily as passive viewers rather than pedagogical tools demonstrating foundational OS mechanisms. Students and systems engineers frequently struggle to correlate theoretical models—such as the 4 Coffman conditions, Wait-For Graph cycle reductions, and matrix-based Banker's safety proofs—with actual runtime process dynamics. 

Moreover, Unix process management tutorials frequently assume Linux-specific pseudo-filesystems (such as `/proc`), which do not exist on BSD/Darwin architectures like macOS. **ProcessShield** solves this by establishing a portable, robust macOS architecture for monitoring, controlling, and simulating resource allocation and contention states.

---

## 3. Core Operating Systems Concepts Demonstrated

### 3.1 Process Management & States
Processes transition through discrete lifecycle states dictated by the OS scheduler:
* **Running ($R$):** The process is currently executing on a CPU core or is ready in the dispatch queue.
* **Sleeping ($S$):** The process is awaiting an event, I/O operation, or timer signal.
* **Idle ($I$):** The process has been quiescent for a prolonged duration (>20 seconds on BSD).
* **Stopped ($T$):** Execution is temporarily halted by a signal (e.g. `SIGSTOP`).
* **Zombie ($Z$):** The process has terminated execution, but its exit status remains in the process table pending retrieval by its parent (`wait()`/`waitpid()`).

### 3.2 Memory Architecture: Virtual Memory vs. Resident Set Size (RSS)
The OS uses virtual memory to provide each process with an isolated, contiguous 64-bit address space. However, reserving virtual memory does not consume physical RAM until pages are accessed, triggering demand paging hardware page faults. ProcessShield tracks **Resident Set Size (RSS)**—the exact amount of physical RAM currently allocated and resident for each process.

### 3.3 POSIX Signals & Process Inter-Communication
Signals are asynchronous notifications delivered by the kernel to a target process:
* `SIGSTOP` (Signal 17 on macOS): Suspends process execution immediately. It cannot be caught, blocked, or ignored by the target.
* `SIGCONT` (Signal 19 on macOS): Instructs the kernel scheduler to resume execution of a stopped process.
* `SIGTERM` (Signal 15): Polite request for termination, giving the process opportunity to release locks, flush file buffers, and free resources.

### 3.4 Deadlock Theory & Coffman Conditions
A deadlock in a multi-tasking system occurs if and only if all four **Coffman conditions** hold simultaneously:
1. **Mutual Exclusion:** Resources cannot be shared simultaneously.
2. **Hold and Wait:** A process holds allocated resources while awaiting new ones.
3. **No Preemption:** Resources cannot be forcibly expropriated from a holding process.
4. **Circular Wait:** A closed chain of processes exists such that each process holds at least one resource needed by the next process in the chain.

### 3.5 Wait-For Graph (WFG) & Cycle Detection
In systems where each resource type has a single instance, a Resource Allocation Graph (RAG) can be collapsed into a **Wait-For Graph (WFG)**. Vertices represent processes, and directed edges $P_i \rightarrow P_j$ indicate that process $P_i$ is blocked waiting for process $P_j$ to release a required resource. 

**Theorem:** A single-instance resource system is deadlocked if and only if its Wait-For Graph contains a directed cycle. ProcessShield computes this via 3-color Depth-First Search ($O(V + E)$ complexity).

### 3.6 Banker's Algorithm (Deadlock Avoidance)
Formulated by Edsger Dijkstra, the Banker's Algorithm prevents deadlocks by evaluating whether granting an allocation request leaves the system in a **Safe State**—a state in which there exists at least one dispatch order (Safe Sequence) allowing every process to obtain its declared maximum resources and finish.
* $Allocation[i][j]$: Units of resource $j$ currently held by process $i$.
* $Max[i][j]$: Maximum declared units of resource $j$ that process $i$ may claim.
* $Need[i][j] = Max[i][j] - Allocation[i][j]$: Remaining units required.
* $Available[j]$: Free unallocated units of resource $j$.

---

## 4. System Architecture & Modular Design

ProcessShield follows a modular, decoupled object-oriented architecture in C++17:

```
                                +-------------------+
                                |     main.cpp      |
                                | (Terminal UI/Menu)|
                                +---------+---------+
                                          |
         +-------------------+------------+------------+--------------------+
         |                   |                         |                    |
+--------v---------+ +-------v--------+       +--------v---------+ +--------v---------+
| ResourceMonitor  | |ProcessController|      |DeadlockSimulator | | BankersAlgorithm |
+--------+---------+ +----------------+       +--------+---------+ +------------------+
         |                                             |
   +-----+-----+                               +-------v--------+
   |           |                               |  WaitForGraph  |
+--v---+   +---v----+                          +----------------+
| CPU  |   | Memory |                                  |
|Analyz|   | Analyz |                                  |
+--+---+   +---+----+                                  |
   |           |                                       |
   +-----+-----+-------------------+-------------------+
         |                         |
+--------v---------+      +--------v---------+
|   AlertManager   | ---> |     Logger       | ---> logs/processshield.log
+------------------+      +------------------+
```

### Module Breakdown:
1. **`ProcessManager` (`src/ProcessManager.h/cpp`):**
   Interacts with the Darwin kernel through `popen()` executing `ps -axo pid,ppid,comm,state,%cpu,rss,time,user`. Employs a resilient reverse-token parsing engine that correctly handles spaces in process names and file paths, computing CPU%, RSS (MB), parent-child relationships, and formatted states.
2. **`CPUAnalyzer` & `MemoryAnalyzer` (`src/CPUAnalyzer.h/cpp`, `src/MemoryAnalyzer.h/cpp`):**
   Compares live metrics against user-defined thresholds (default: 70% CPU, 500 MB RAM). Automatically constructs structured violation models and raises alerts.
3. **`ResourceMonitor` (`src/ResourceMonitor.h/cpp`):**
   Coordinates telemetry aggregation, system-wide summaries, and threshold configuration.
4. **`ProcessController` (`src/ProcessController.h/cpp`):**
   Dispatches kernel signals (`kill(pid, sig)`). Enforces critical safety boundaries protecting PID 0, PID 1 (`launchd`), and ProcessShield itself. Translates POSIX `errno` codes (`EPERM`, `ESRCH`, `EINVAL`) into human-readable diagnostics.
5. **`WaitForGraph` (`src/WaitForGraph.h/cpp`):**
   Maintains adjacency list representations of process dependencies. Implements DFS 3-color graph traversal with back-edge detection and explicit cycle path reconstruction.
6. **`DeadlockSimulator` (`src/DeadlockSimulator.h/cpp`):**
   Simulates resource allocations and requests safely in memory without destabilizing the host OS. Converts RAG bipartite state into a Wait-For Graph and verifies deadlocks.
7. **`BankersAlgorithm` (`src/BankersAlgorithm.h/cpp`):**
   Full matrix-based implementation of Dijkstra's algorithm. Evaluates safety vectors, generates safe sequences, and simulates the resource-request step with automatic rollback.
8. **`AlertManager` (`src/AlertManager.h/cpp`):**
   Thread-safe, singleton alert repository categorized by severity (`INFO`, `WARNING`, `CRITICAL`).
9. **`Logger` (`src/Logger.h/cpp`):**
   Thread-safe file logger emitting ISO-formatted timestamps and events to `logs/processshield.log`.

---

## 5. Algorithms & Implementation Details

### 5.1 Depth-First Search (DFS) Cycle Detection in Wait-For Graph
To detect cycles in a directed graph $G = (V, E)$, each node is labeled with one of three colors:
* **White (0):** Unvisited node.
* **Gray (1):** Node currently residing in the active recursion call stack.
* **Black (2):** Node whose subtree has been completely processed.

```cpp
bool dfsVisit(node u):
    color[u] = Gray
    for each v in adjacency[u]:
        if color[v] == Gray:
            // Back-edge detected! Cycle found.
            reconstructCycle(from: u, to: v)
            return true
        if color[v] == White:
            parent[v] = u
            if dfsVisit(v):
                return true
    color[u] = Black
    return false
```
* **Time Complexity:** $O(|V| + |E|)$ where $|V|$ is the number of processes and $|E|$ is the number of wait dependencies.
* **Space Complexity:** $O(|V|)$ for color map and recursion stack.

### 5.2 Banker's Safety Algorithm
Given $n$ processes and $m$ resource types:
1. Let $Work = Available$ and $Finish[i] = false$ for $i = 0, 1, \dots, n-1$.
2. Find an index $i$ such that:
   $$Finish[i] == false \quad \text{and} \quad Need[i][j] \le Work[j] \quad (\forall j = 0, \dots, m-1)$$
   If no such $i$ exists, go to step 4.
3. $Work[j] = Work[j] + Allocation[i][j]$ for all $j$.
   $Finish[i] = true$.
   Append $P_i$ to $SafeSequence$.
   Go to step 2.
4. If $Finish[i] == true$ for all $i$, then the system is in a **SAFE state** with sequence $SafeSequence$. Otherwise, the system is in an **UNSAFE state**.
* **Time Complexity:** $O(m \times n^2)$.

---

## 6. Experimental Results & Verification

### 6.1 Test Suite Verification
The automated test suite (`tests/deadlock_test.cpp`) was executed to confirm algorithmic correctness:
```
[Test 1] Wait-For Graph: 2-Process Cycle Detection (P1 <-> P2)...   --> PASS
[Test 2] Wait-For Graph: Acyclic Graph (P1 -> P2 -> P3)...          --> PASS
[Test 3] Deadlock Simulator: Classic RAG Simulation...               --> PASS
[Test 4] Banker's Algorithm: Classic Safe Case...                   --> PASS
[Test 5] Banker's Algorithm: Unsafe Case...                         --> PASS
```

### 6.2 Empirical Runtime Tests
1. **Live Process Snapshot:** Correctly extracted over 680 concurrent macOS processes, populating PID, PPID, state, CPU%, RSS, and user information.
2. **CPU Stress Test (`cpu_stress`):** ProcessShield flagged high CPU usage at 97.3% against the 70.0% threshold, logging a WARNING alert. Pausing via `SIGSTOP` suspended the loop, `SIGCONT` resumed it, and `SIGTERM` gracefully terminated the process.
3. **Memory Stress Test (`memory_test 600`):** Committed 601.4 MB into resident physical RAM via 4KB page touches. ProcessShield detected the threshold violation (>500 MB), triggered a WARNING alert, and successfully terminated the process upon user command.
4. **Deadlock Simulation:** Successfully reconstructed cycle `P1 -> P2 -> P1` in 2-process contention and `P1 -> P2 -> P3 -> P1` in 3-process circular wait, escalating to CRITICAL alerts.
5. **Banker's Algorithm Verification:** Correctly generated safe execution order `P1 -> P3 -> P4 -> P0 -> P2` for valid allocations, and properly identified blocked processes under starved resource matrices.

---

## 7. Limitations & Future Scope

### Limitations:
* ProcessShield queries `ps` periodically; sub-millisecond transient spikes may not be captured between refresh intervals.
* Resource Allocation Graph simulation assumes single-unit resources per type. Multi-instance resource deadlock detection requires matrix reduction.

### Future Scope:
1. **Kernel Event Integration:** Implementing macOS `kqueue` / `EVFILT_PROC` to receive asynchronous kernel push notifications on process fork, exec, and exit events.
2. **Interactive TUI:** Integrating `ncurses` for interactive scrolling and real-time histogram rendering.
3. **Automated Enforcement Policies:** Enabling configurable automatic throttling (e.g. automatically applying `SIGSTOP` or `setpriority()` nice levels when CPU threshold is violated for $>30$ seconds).

---

## 8. Conclusion
ProcessShield successfully realizes a robust, educational, and practical Operating Systems resource guardian on macOS. By unifying live process monitoring, POSIX signal-based process control, graph-theoretic deadlock detection, and Banker's safety proofs, the project demonstrates how fundamental OS concepts govern computing stability and performance in real-world systems.
