#!/usr/bin/env python3
"""
ProcessShield Simulation Mode (Virtual Computers Fleet)
Simulates multiple virtual computers with clearly labelled simulated IP addresses
and dynamic CPU/RAM/disk/network/process telemetry for automated testing.
"""

import sys
import os
import time
import json
import math
import random
import urllib.request
import urllib.error

DEFAULT_AUTH_KEY = "ProcessShield-OS-2026"

# Simulated virtual machines profiles
VIRTUAL_NODES_CONFIG = [
    {
        "hostname": "ubuntu-web-srv-01",
        "ip": "192.168.1.101 (SIMULATED)",
        "system_type": "Ubuntu Linux 22.04 LTS (SIMULATED VM)",
        "ram_total_gb": 16.0,
        "disk_total_gb": 250.0,
        "base_cpu": 32.0,
        "base_ram_used": 6.2,
        "base_disk_used": 68.0,
        "base_proc_count": 145,
        "process_templates": [
            {"name": "nginx: worker process", "user": "www-data", "state": "RUNNING", "cpu_base": 8.5, "mem_base": 120.0},
            {"name": "nginx: master process", "user": "root", "state": "SLEEPING", "cpu_base": 0.2, "mem_base": 45.0},
            {"name": "python3 (uvicorn-api)", "user": "appuser", "state": "RUNNING", "cpu_base": 14.0, "mem_base": 420.0},
            {"name": "redis-server", "user": "redis", "state": "SLEEPING", "cpu_base": 2.1, "mem_base": 210.0},
            {"name": "systemd", "user": "root", "state": "SLEEPING", "cpu_base": 0.1, "mem_base": 30.0},
            {"name": "sshd: root@pts/0", "user": "root", "state": "SLEEPING", "cpu_base": 0.3, "mem_base": 15.0},
            {"name": "rsyslogd", "user": "syslog", "state": "SLEEPING", "cpu_base": 0.2, "mem_base": 22.0},
            {"name": "cron", "user": "root", "state": "SLEEPING", "cpu_base": 0.0, "mem_base": 8.0}
        ]
    },
    {
        "hostname": "win11-eng-station-02",
        "ip": "192.168.1.102 (SIMULATED)",
        "system_type": "Windows 11 Pro Build 22631 (SIMULATED VM)",
        "ram_total_gb": 32.0,
        "disk_total_gb": 1000.0,
        "base_cpu": 76.0,  # Fluctuate into Warning/Critical
        "base_ram_used": 26.5,
        "base_disk_used": 710.0,
        "base_proc_count": 224,
        "process_templates": [
            {"name": "cl.exe (MSVC C++ Compiler)", "user": "developer", "state": "RUNNING", "cpu_base": 42.0, "mem_base": 1450.0},
            {"name": "link.exe (Incremental Linker)", "user": "developer", "state": "RUNNING", "cpu_base": 24.5, "mem_base": 2100.0},
            {"name": "Code.exe (VS Code)", "user": "developer", "state": "SLEEPING", "cpu_base": 4.5, "mem_base": 850.0},
            {"name": "chrome.exe (DevTools)", "user": "developer", "state": "SLEEPING", "cpu_base": 3.8, "mem_base": 1100.0},
            {"name": "explorer.exe", "user": "developer", "state": "SLEEPING", "cpu_base": 0.8, "mem_base": 180.0},
            {"name": "dwm.exe (Desktop Window Mgr)", "user": "SYSTEM", "state": "RUNNING", "cpu_base": 2.2, "mem_base": 95.0},
            {"name": "MsMpEng.exe (Defender)", "user": "SYSTEM", "state": "RUNNING", "cpu_base": 5.0, "mem_base": 320.0}
        ]
    },
    {
        "hostname": "pg-cluster-primary-03",
        "ip": "192.168.1.103 (SIMULATED)",
        "system_type": "Debian Linux 12 Bookworm (SIMULATED VM)",
        "ram_total_gb": 64.0,
        "disk_total_gb": 2000.0,
        "base_cpu": 52.0,
        "base_ram_used": 54.2,
        "base_disk_used": 1780.0,
        "base_proc_count": 188,
        "process_templates": [
            {"name": "postgres: checkpointer", "user": "postgres", "state": "RUNNING", "cpu_base": 18.2, "mem_base": 3200.0},
            {"name": "postgres: writer", "user": "postgres", "state": "RUNNING", "cpu_base": 14.5, "mem_base": 2800.0},
            {"name": "postgres: walwriter", "user": "postgres", "state": "RUNNING", "cpu_base": 9.0, "mem_base": 1500.0},
            {"name": "postgres: autovacuum worker", "user": "postgres", "state": "RUNNING", "cpu_base": 7.4, "mem_base": 980.0},
            {"name": "pgbouncer", "user": "postgres", "state": "SLEEPING", "cpu_base": 2.1, "mem_base": 150.0},
            {"name": "prometheus-node-exporter", "user": "prometheus", "state": "SLEEPING", "cpu_base": 0.5, "mem_base": 45.0},
            {"name": "systemd-journald", "user": "root", "state": "SLEEPING", "cpu_base": 0.3, "mem_base": 85.0}
        ]
    }
]

def generate_telemetry(config, tick, key):
    """Generates dynamically changing telemetry for a simulated node."""
    cpu_osc = math.sin(tick * 0.2 + random.uniform(-0.1, 0.1)) * 12.0 + random.uniform(-3, 3)
    cpu_val = max(5.0, min(99.0, config["base_cpu"] + cpu_osc))

    ram_osc = math.cos(tick * 0.15) * 1.5 + random.uniform(-0.4, 0.4)
    ram_used = max(1.0, min(config["ram_total_gb"] * 0.98, config["base_ram_used"] + ram_osc))
    ram_pct = (ram_used / config["ram_total_gb"]) * 100.0

    disk_used = config["base_disk_used"] + (tick % 20) * 0.2
    disk_pct = (disk_used / config["disk_total_gb"]) * 100.0

    proc_count = int(config["base_proc_count"] + math.sin(tick * 0.3) * 6 + random.randint(-2, 3))

    net_sent = round(50000.0 + tick * 120.5 + random.uniform(10, 50), 1)
    net_recv = round(120000.0 + tick * 450.2 + random.uniform(20, 100), 1)

    # Evaluate health
    reasons = []
    status = "HEALTHY"

    if cpu_val >= 85.0:
        reasons.append(f"Severe CPU saturation ({int(cpu_val)}%)")
        status = "CRITICAL"
    elif cpu_val >= 70.0:
        reasons.append(f"Elevated CPU utilization ({int(cpu_val)}%)")
        status = "WARNING"

    if ram_pct >= 90.0:
        reasons.append(f"Critically low RAM ({int(ram_pct)}%)")
        status = "CRITICAL"
    elif ram_pct >= 75.0:
        reasons.append(f"Elevated memory consumption ({int(ram_pct)}%)")
        if status != "CRITICAL": status = "WARNING"

    if disk_pct >= 90.0:
        reasons.append(f"Root filesystem critically full ({int(disk_pct)}%)")
        status = "CRITICAL"
    elif disk_pct >= 80.0:
        reasons.append(f"Storage volume nearly full ({int(disk_pct)}%)")
        if status != "CRITICAL": status = "WARNING"

    if not reasons:
        reasons.append("All virtual machine metrics within normal thresholds")

    # Generate process list
    processes = []
    pid_base = 1000 + random.randint(100, 300)
    for i, t in enumerate(config["process_templates"]):
        pid = pid_base + i * 42
        p_cpu = max(0.1, round(t["cpu_base"] + random.uniform(-2.0, 2.0), 1))
        p_mem = max(5.0, round(t["mem_base"] + random.uniform(-10.0, 15.0), 1))
        is_user = (t["user"] not in ("root", "SYSTEM", "redis", "syslog"))
        processes.append({
            "pid": pid,
            "ppid": 1 if i > 0 else 0,
            "name": t["name"],
            "user": t["user"],
            "is_user_app": is_user,
            "category": "Application" if is_user else "Background Process",
            "state": t["state"],
            "cpu_percent": p_cpu,
            "memory_mb": p_mem
        })

    processes.sort(key=lambda p: p["cpu_percent"], reverse=True)

    return {
        "node_id": f"{config['hostname']} ({config['ip']})",
        "hostname": config["hostname"],
        "ip": config["ip"],
        "is_simulated": True,
        "system_type": config["system_type"],
        "cpu_percent": round(cpu_val, 1),
        "ram_used_gb": round(ram_used, 1),
        "ram_total_gb": round(config["ram_total_gb"], 1),
        "ram_percent": round(ram_pct, 1),
        "disk_used_gb": round(disk_used, 1),
        "disk_total_gb": round(config["disk_total_gb"], 1),
        "disk_percent": round(disk_pct, 1),
        "net_sent_kb": net_sent,
        "net_recv_kb": net_recv,
        "process_count": proc_count,
        "health": status,
        "health_reasons": reasons,
        "processes": processes,
        "auth_key": key
    }

def main():
    import argparse
    parser = argparse.ArgumentParser(description="ProcessShield Virtual Computers Simulator")
    parser.add_argument("--server", default="http://127.0.0.1:8080", help="Master server URL (default: http://127.0.0.1:8080)")
    parser.add_argument("--key", default=DEFAULT_AUTH_KEY, help=f"Security key (default: {DEFAULT_AUTH_KEY})")
    parser.add_argument("--interval", type=int, default=5, help="Telemetry reporting interval in seconds (default: 5)")
    parser.add_argument("--nodes", type=int, default=3, help="Number of simulated nodes (1 to 3, default: 3)")
    args = parser.parse_args()

    active_nodes = VIRTUAL_NODES_CONFIG[:max(1, min(len(VIRTUAL_NODES_CONFIG), args.nodes))]
    target_url = f"{args.server.rstrip('/')}/api/report"

    print("=" * 72)
    print("      PROCESSSHIELD MULTI-NODE SIMULATOR (VIRTUAL COMPUTERS)        ")
    print("=" * 72)
    print(f" [*] Master Server:  {target_url}")
    print(f" [*] Auth Key:       {args.key}")
    print(f" [*] Virtual Nodes:  {len(active_nodes)} Simulated Computers")
    for n in active_nodes:
        print(f"     - {n['hostname']:<22} IP: {n['ip']:<25} ({n['system_type']})")
    print(f" [*] Update Interval:{args.interval}s")
    print(" [*] All simulated nodes are clearly labelled 'SIMULATED'!")
    print(" [*] Press Ctrl+C to terminate Simulation.")
    print("=" * 72 + "\n")

    tick = 0
    while True:
        tick += 1
        for node_conf in active_nodes:
            telemetry = generate_telemetry(node_conf, tick, args.key)
            try:
                data = json.dumps(telemetry).encode("utf-8")
                req = urllib.request.Request(
                    target_url,
                    data=data,
                    headers={
                        "Content-Type": "application/json",
                        "User-Agent": "ProcessShield-Simulator/2.0",
                        "X-ProcessShield-Key": args.key
                    }
                )
                with urllib.request.urlopen(req, timeout=3) as resp:
                    status_str = "OK 200" if resp.getcode() == 200 else f"RESP {resp.getcode()}"
            except urllib.error.URLError as e:
                status_str = f"REFUSED ({e.reason})"
            except Exception as e:
                status_str = f"ERR ({e})"

            print(f"[Simulator] [{status_str:<12}] Node: {telemetry['hostname']:<22} "
                  f"| IP: {telemetry['ip']:<24} "
                  f"| CPU: {telemetry['cpu_percent']:>4.1f}% "
                  f"| RAM: {telemetry['ram_percent']:>4.1f}% "
                  f"| Health: {telemetry['health']:<8}")

        print("-" * 72)
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
