#!/usr/bin/env python3
"""
ProcessShield Fully Automated Lightweight Client Agent (Python / Cross-Platform)
Runs continuously in background, automatically discovers Master Server on LAN/Ethernet,
auto-reconnects across network drops, and streams OS telemetry every 5 seconds.
"""

import sys
import os
import time
import json
import socket
import re
import shutil
import platform
import subprocess
import urllib.request
import urllib.error

# Import local discovery service
try:
    from discovery import discover_master_server
except ImportError:
    discover_master_server = None

DEFAULT_AUTH_KEY = "ProcessShield-OS-2026"
DEFAULT_INTERVAL = 5

def get_macos_hardware_ports():
    ports = {}
    if platform.system() != "Darwin":
        return ports
    try:
        out = subprocess.check_output(['networksetup', '-listallhardwareports'], text=True, stderr=subprocess.DEVNULL)
        blocks = out.strip().split('Hardware Port: ')
        for b in blocks[1:]:
            lines = b.strip().splitlines()
            name = lines[0].strip()
            dev = None
            for l in lines[1:]:
                if l.startswith('Device:'):
                    dev = l.split(':', 1)[1].strip()
            if dev:
                ports[dev] = name
    except Exception:
        pass
    return ports

def detect_agent_network():
    """
    Detects hostname, primary Ethernet IP, and active LAN IP.
    Prioritizes Ethernet if connected.
    """
    hostname = socket.gethostname()
    sys_name = platform.system()
    hw_ports = get_macos_hardware_ports() if sys_name == 'Darwin' else {}

    eth_ip = None
    eth_dev = None
    lan_ip = None
    lan_dev = None
    lan_mac = None

    try:
        out = subprocess.check_output(['ifconfig'], text=True, stderr=subprocess.DEVNULL)
        current_if = None
        for line in out.splitlines():
            if line and not line.startswith('\t') and not line.startswith(' '):
                parts = line.split(':')
                current_if = parts[0].strip()
            elif current_if:
                hw_name = hw_ports.get(current_if, '').lower()
                is_eth = (
                    'ethernet' in hw_name or
                    current_if.startswith(('eth', 'enp', 'eno', 'ens', 'end')) or
                    (sys_name == 'Darwin' and current_if in ('en1', 'en2', 'en3', 'en4') and 'wi-fi' not in hw_name)
                )

                m_ip = re.search(r'inet\s+(\d+\.\d+\.\d+\.\d+)', line)
                if m_ip:
                    ip = m_ip.group(1)
                    if ip != '127.0.0.1':
                        if is_eth and not eth_ip:
                            eth_ip = ip
                            eth_dev = current_if
                        if not lan_ip:
                            lan_ip = ip
                            lan_dev = current_if

                m_mac = re.search(r'ether\s+([0-9a-fA-F:]{17})', line)
                if m_mac and (current_if == eth_dev or current_if == lan_dev):
                    lan_mac = m_mac.group(1)
    except Exception:
        pass

    # UDP fallback if ifconfig didn't yield an IP
    if not lan_ip:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.settimeout(0.5)
            s.connect(('10.255.255.255', 1))
            sock_ip = s.getsockname()[0]
            s.close()
            if sock_ip and not sock_ip.startswith('127.'):
                lan_ip = sock_ip
                lan_dev = "en0"
        except Exception:
            pass

    chosen_ip = eth_ip or lan_ip or "127.0.0.1"
    chosen_dev = eth_dev if eth_ip else (lan_dev or "lo0")
    is_real_ethernet = (eth_ip is not None)

    return {
        "hostname": hostname,
        "ip": chosen_ip,
        "interface": chosen_dev,
        "mac": lan_mac,
        "is_ethernet": is_real_ethernet
    }

def get_disk_stats():
    """Returns disk usage for root filesystem."""
    try:
        total, used, free = shutil.disk_usage("/")
        total_gb = total / (1024 ** 3)
        used_gb = used / (1024 ** 3)
        pct = (used / total) * 100.0 if total > 0 else 0.0
        return round(total_gb, 1), round(used_gb, 1), round(pct, 1)
    except Exception:
        return 0.0, 0.0, 0.0

def get_ram_stats():
    """Returns total, used and percent RAM on macOS/Linux."""
    sys_name = platform.system()
    if sys_name == "Darwin":
        try:
            out = subprocess.check_output(["sysctl", "-n", "hw.memsize"], stderr=subprocess.DEVNULL).decode().strip()
            total_bytes = int(out)
            total_gb = total_bytes / (1024 ** 3)

            vm = subprocess.check_output(["vm_stat"], stderr=subprocess.DEVNULL).decode()
            page_size = 4096
            active = 0
            wire = 0
            compressed = 0
            for line in vm.splitlines():
                if "Pages active" in line:
                    active = int(line.split(":")[1].strip().rstrip("."))
                elif "Pages wired down" in line:
                    wire = int(line.split(":")[1].strip().rstrip("."))
                elif "Pages occupied by compressor" in line:
                    compressed = int(line.split(":")[1].strip().rstrip("."))
            used_bytes = (active + wire + compressed) * page_size
            used_gb = used_bytes / (1024 ** 3)
            pct = (used_bytes / total_bytes) * 100.0 if total_bytes > 0 else 50.0
            return round(total_gb, 1), round(used_gb, 1), round(pct, 1)
        except Exception:
            return 16.0, 8.0, 50.0
    elif sys_name == "Linux":
        try:
            with open("/proc/meminfo") as f:
                lines = f.readlines()
            mem_info = {}
            for line in lines:
                parts = line.split(":")
                if len(parts) == 2:
                    mem_info[parts[0].strip()] = int(parts[1].split()[0].strip())
            total_kb = mem_info.get("MemTotal", 1024)
            avail_kb = mem_info.get("MemAvailable", mem_info.get("MemFree", 512))
            used_kb = total_kb - avail_kb
            total_gb = total_kb / (1024 * 1024)
            used_gb = used_kb / (1024 * 1024)
            pct = (used_kb / total_kb) * 100.0
            return round(total_gb, 1), round(used_gb, 1), round(pct, 1)
        except Exception:
            return 8.0, 4.0, 50.0
    return 16.0, 8.0, 50.0

def get_net_io_stats():
    """Returns cumulative network KB sent and received."""
    sys_name = platform.system()
    if sys_name == "Darwin":
        try:
            out = subprocess.check_output(['netstat', '-ib'], text=True, stderr=subprocess.DEVNULL)
            lines = out.strip().splitlines()
            total_ibytes = 0
            total_obytes = 0
            seen_if = set()
            for l in lines[1:]:
                parts = l.split()
                if len(parts) >= 11 and parts[0] not in ('lo0', 'gif0', 'stf0'):
                    if parts[0] not in seen_if:
                        try:
                            total_ibytes += int(parts[6])
                            total_obytes += int(parts[9])
                            seen_if.add(parts[0])
                        except (ValueError, IndexError):
                            pass
            return round(total_obytes / 1024.0, 1), round(total_ibytes / 1024.0, 1)
        except Exception:
            return 0.0, 0.0
    elif sys_name == "Linux":
        try:
            with open("/proc/net/dev") as f:
                lines = f.readlines()
            total_rx = 0
            total_tx = 0
            for line in lines[2:]:
                parts = line.split(":")
                if len(parts) == 2 and not parts[0].strip().startswith("lo"):
                    cols = parts[1].split()
                    total_rx += int(cols[0])
                    total_tx += int(cols[8])
            return round(total_tx / 1024.0, 1), round(total_rx / 1024.0, 1)
        except Exception:
            return 0.0, 0.0
    return 0.0, 0.0

def get_processes():
    """Collects process count, separating user applications from background daemons."""
    processes = []
    total_cpu = 0.0
    try:
        cmd = ["ps", "-axo", "pid,ppid,comm,state,%cpu,rss,user"]
        out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode("utf-8", errors="replace")
        lines = out.strip().splitlines()
        if len(lines) > 1:
            for line in lines[1:]:
                parts = line.split()
                if len(parts) >= 7:
                    try:
                        pid = int(parts[0])
                        ppid = int(parts[1])
                        name = os.path.basename(parts[2])
                        state_raw = parts[3]
                        cpu = float(parts[4])
                        rss_kb = int(parts[5])
                        user = parts[6]
                        mem_mb = round(rss_kb / 1024.0, 1)
                        total_cpu += cpu

                        state_map = {'R': 'RUNNING', 'S': 'SLEEPING', 'I': 'IDLE', 'T': 'STOPPED', 'Z': 'ZOMBIE'}
                        state_desc = state_map.get(state_raw[0] if state_raw else 'S', 'ACTIVE')

                        # Distinguish user application vs background system daemon
                        is_user_app = (user not in ('root', 'daemon', 'nobody') and not user.startswith('_'))

                        processes.append({
                            "pid": pid,
                            "ppid": ppid,
                            "name": name,
                            "user": user,
                            "is_user_app": is_user_app,
                            "category": "Application" if is_user_app else "Background Process",
                            "state": state_desc,
                            "cpu_percent": cpu,
                            "memory_mb": mem_mb
                        })
                    except Exception:
                        continue
    except Exception:
        pass

    ncpu = os.cpu_count() or 1
    norm_cpu = min(100.0, total_cpu / ncpu)
    processes.sort(key=lambda p: p["cpu_percent"], reverse=True)
    return round(norm_cpu, 1), len(processes), processes[:30]

def evaluate_health(cpu_pct, ram_pct, disk_pct):
    reasons = []
    status = "HEALTHY"

    if cpu_pct >= 85.0:
        reasons.append(f"Severe CPU saturation ({int(cpu_pct)}%)")
        status = "CRITICAL"
    elif cpu_pct >= 70.0:
        reasons.append(f"Elevated CPU utilization ({int(cpu_pct)}%)")
        status = "WARNING"

    if ram_pct >= 90.0:
        reasons.append(f"Critically low RAM ({int(ram_pct)}%)")
        status = "CRITICAL"
    elif ram_pct >= 75.0:
        reasons.append(f"Elevated memory usage ({int(ram_pct)}%)")
        if status != "CRITICAL": status = "WARNING"

    if disk_pct >= 90.0:
        reasons.append(f"Disk storage critically full ({int(disk_pct)}%)")
        status = "CRITICAL"
    elif disk_pct >= 80.0:
        reasons.append(f"Disk space filling up ({int(disk_pct)}%)")
        if status != "CRITICAL": status = "WARNING"

    if not reasons:
        reasons.append("Operating system operating within healthy parameters")

    return status, reasons

def main():
    import argparse
    parser = argparse.ArgumentParser(description="ProcessShield Automated Client Agent")
    parser.add_argument("--server", default=None, help="Master server URL (e.g. http://192.168.1.10:8080)")
    parser.add_argument("--host", default=None, help="Master server hostname or IP")
    parser.add_argument("--port", type=int, default=8080, help="Master server port (default: 8080)")
    parser.add_argument("--key", default=DEFAULT_AUTH_KEY, help=f"Security authorization key (default: {DEFAULT_AUTH_KEY})")
    parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL, help="Reporting interval in seconds (default: 5)")
    parser.add_argument("--once", action="store_true", help="Send single report and exit")
    args = parser.parse_args()

    net_info = detect_agent_network()
    hostname = net_info['hostname']
    ip = net_info['ip']
    iface = net_info['interface']

    # Determine Master Server URL
    target_server = None
    if args.server:
        target_server = args.server.rstrip('/')
    elif args.host:
        target_server = f"http://{args.host}:{args.port}"
    else:
        # Automated network discovery
        print(" [*] Searching for ProcessShield Master Server on LAN via UDP Auto-Discovery...")
        discovered = discover_master_server(timeout=1.5) if discover_master_server else None
        if discovered:
            target_server = f"http://{discovered['ip']}:{discovered['port']}"
            print(f" [✓] Discovered Master Server at {target_server} ({discovered.get('hostname', 'Master')})")
        else:
            target_server = f"http://127.0.0.1:{args.port}"
            print(f" [-] Discovery broadcast not heard. Defaulting to: {target_server}")

    report_url = f"{target_server}/api/report"

    print("\n" + "=" * 72)
    print("      PROCESSSHIELD AUTOMATED MONITORING AGENT (Daemon Mode)       ")
    print("=" * 72)
    print(f" [*] Monitored Hostname:  {hostname}")
    print(f" [*] Detected IP Address: {ip} ({'Physical Ethernet' if net_info['is_ethernet'] else 'LAN / Wi-Fi'})")
    print(f" [*] Network Interface:   {iface} (MAC: {net_info['mac'] or 'N/A'})")
    print(f" [*] Target Master URL:   {report_url}")
    print(f" [*] Telemetry Cadence:   Every {args.interval} seconds")
    print(f" [*] Auto-Reconnection:   ENABLED (Infinite resilient loop)")
    print(f" [*] Security Auth Key:   {args.key}")
    print("=" * 72 + "\n")

    consecutive_failures = 0

    while True:
        total_cpu, proc_count, top_procs = get_processes()
        total_ram, used_ram, ram_pct = get_ram_stats()
        total_disk, used_disk, disk_pct = get_disk_stats()
        net_sent_kb, net_recv_kb = get_net_io_stats()
        health, health_reasons = evaluate_health(total_cpu, ram_pct, disk_pct)

        payload = {
            "node_id": f"{hostname} ({ip})",
            "hostname": hostname,
            "ip": ip,
            "mac": net_info['mac'],
            "interface": iface,
            "is_simulated": False,
            "system_type": f"{platform.system()} ({platform.machine()})",
            "cpu_percent": total_cpu,
            "ram_used_gb": used_ram,
            "ram_total_gb": total_ram,
            "ram_percent": ram_pct,
            "disk_used_gb": used_disk,
            "disk_total_gb": total_disk,
            "disk_percent": disk_pct,
            "net_sent_kb": net_sent_kb,
            "net_recv_kb": net_recv_kb,
            "process_count": proc_count,
            "health": health,
            "health_reasons": health_reasons,
            "processes": top_procs,
            "auth_key": args.key
        }

        try:
            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                report_url,
                data=data,
                headers={
                    "Content-Type": "application/json",
                    "User-Agent": "ProcessShield-AutomatedAgent/2.0",
                    "X-ProcessShield-Key": args.key
                }
            )
            with urllib.request.urlopen(req, timeout=4) as resp:
                status_code = resp.getcode()
                if status_code == 200:
                    if consecutive_failures > 0:
                        print(f" [✓] CONNECTION RESTORED: Successfully reconnected to Master Server.")
                        consecutive_failures = 0
                    print(f" [Agent Telemetry] Host: {hostname} | IP: {ip} | CPU: {total_cpu}% | RAM: {ram_pct}% | Disk: {disk_pct}% | Health: {health} | Procs: {proc_count}")
                else:
                    print(f" [!] Master Server returned HTTP {status_code}")
        except urllib.error.HTTPError as e:
            if e.code == 401:
                print(f" [✗] AUTHENTICATION FAILED (401 Unauthorized): Check --key argument.")
            else:
                print(f" [!] HTTP Error {e.code}: {e.reason}")
            consecutive_failures += 1
        except (urllib.error.URLError, socket.timeout, ConnectionError) as e:
            consecutive_failures += 1
            if consecutive_failures == 1 or consecutive_failures % 6 == 0:
                print(f" [!] Network Interruption: Master Server unreachable ({e}). Retrying automatically in {args.interval}s...")
        except Exception as e:
            consecutive_failures += 1
            print(f" [!] Unexpected error: {e}. Retrying...")

        if args.once:
            break
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
