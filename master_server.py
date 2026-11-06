#!/usr/bin/env python3
"""
ProcessShield Fully Automated Master Server & Web Dashboard
Collects, authenticates, and persistently stores telemetry from real and simulated computers.
Features network auto-discovery, SQLite persistence, 24-hour history, offline detection,
automated data retention pruning, and daily report generation.
"""

import sys
import os
import time
import json
import socket
import re
import threading
import subprocess
import platform
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn

# Import persistent database and discovery modules
import database
from discovery import MasterDiscoveryService

FLEET_LOCK = threading.Lock()
FLEET_NODES = {}
SIMULATOR_PROCESS = None
SERVER_START_TIME = time.time()
DISCOVERY_SERVICE = None

DEFAULT_AUTH_KEY = "ProcessShield-OS-2026"
SERVER_AUTH_KEY = os.environ.get("PROCESSSHIELD_KEY", DEFAULT_AUTH_KEY)
DEFAULT_RETENTION_DAYS = int(os.environ.get("PROCESSSHIELD_RETENTION_DAYS", "7"))

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

def detect_network_interfaces():
    hostname = socket.gethostname()
    sys_name = platform.system()
    hw_ports = get_macos_hardware_ports() if sys_name == 'Darwin' else {}

    interfaces_map = {}
    try:
        out = subprocess.check_output(['ifconfig'], text=True, stderr=subprocess.DEVNULL)
        current_if = None
        for line in out.splitlines():
            if line and not line.startswith('\t') and not line.startswith(' '):
                parts = line.split(':')
                current_if = parts[0].strip()
                hw_name = hw_ports.get(current_if, '')
                is_eth = (
                    'ethernet' in hw_name.lower() or 
                    current_if.startswith(('eth', 'enp', 'eno', 'ens', 'end')) or
                    (sys_name == 'Darwin' and current_if in ('en1', 'en2', 'en3', 'en4') and 'wi-fi' not in hw_name.lower())
                )
                is_wifi = 'wi-fi' in hw_name.lower() or 'wireless' in hw_name.lower() or (sys_name == 'Darwin' and hw_name == 'Wi-Fi')

                interfaces_map[current_if] = {
                    'device': current_if,
                    'hw_name': hw_name if hw_name else ('Ethernet' if is_eth else ('Wi-Fi' if is_wifi else 'Network Interface')),
                    'is_ethernet': is_eth,
                    'is_wifi': is_wifi,
                    'status': 'inactive',
                    'ips': [],
                    'mac': None
                }
            elif current_if and current_if in interfaces_map:
                m_ip = re.search(r'inet\s+(\d+\.\d+\.\d+\.\d+)', line)
                if m_ip:
                    ip = m_ip.group(1)
                    if ip != '127.0.0.1':
                        interfaces_map[current_if]['ips'].append(ip)
                m_status = re.search(r'status:\s+([a-zA-Z]+)', line)
                if m_status:
                    interfaces_map[current_if]['status'] = m_status.group(1).lower()
                elif 'flags=' in line and 'RUNNING' in line:
                    if interfaces_map[current_if]['status'] != 'active':
                        interfaces_map[current_if]['status'] = 'active'
                m_ether = re.search(r'ether\s+([0-9a-fA-F:]{17})', line)
                if m_ether:
                    interfaces_map[current_if]['mac'] = m_ether.group(1)
    except Exception:
        pass

    primary_eth_ip = None
    primary_eth_dev = None
    primary_lan_ip = None
    primary_lan_dev = None
    primary_lan_type = "LAN"

    for dev, info in interfaces_map.items():
        if info['is_ethernet'] and info['ips']:
            primary_eth_ip = info['ips'][0]
            primary_eth_dev = dev
            break

    for dev, info in interfaces_map.items():
        if info['ips']:
            if info['is_ethernet']:
                primary_lan_ip = info['ips'][0]
                primary_lan_dev = dev
                primary_lan_type = "Ethernet"
                break
            elif not primary_lan_ip:
                primary_lan_ip = info['ips'][0]
                primary_lan_dev = dev
                primary_lan_type = "Wi-Fi" if info['is_wifi'] else "LAN"

    if not primary_lan_ip:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.settimeout(0.5)
            s.connect(('10.255.255.255', 1))
            sock_ip = s.getsockname()[0]
            s.close()
            if sock_ip and not sock_ip.startswith('127.'):
                primary_lan_ip = sock_ip
                primary_lan_dev = "en0"
                primary_lan_type = "LAN"
        except Exception:
            pass

    if not primary_lan_ip:
        primary_lan_ip = "127.0.0.1"
        primary_lan_dev = "lo0"
        primary_lan_type = "Localhost"

    return {
        'hostname': hostname,
        'primary_ethernet_ip': primary_eth_ip,
        'primary_ethernet_dev': primary_eth_dev,
        'primary_lan_ip': primary_lan_ip,
        'primary_lan_dev': primary_lan_dev,
        'primary_type': primary_lan_type,
        'interfaces': list(interfaces_map.values())
    }

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

class MasterRequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass

    def send_json(self, status_code, data):
        payload = json.dumps(data).encode('utf-8')
        self.send_response(status_code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(payload)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(payload)

    def do_HEAD(self):
        path = self.path.split('?')[0]
        if path == '/' or path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
        else:
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, User-Agent, X-ProcessShield-Key')
        self.end_headers()

    def do_GET(self):
        path = self.path.split('?')[0]

        if path == '/' or path == '/index.html':
            html_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'web', 'index.html')
            if os.path.exists(html_path):
                with open(html_path, 'rb') as f:
                    content = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(content)))
                self.end_headers()
                self.wfile.write(content)
            else:
                self.send_json(404, {"error": "Dashboard HTML not found"})

        elif path == '/api/status':
            net_info = detect_network_interfaces()
            db_stats = database.get_db_stats()
            self.send_json(200, {
                "server": "ProcessShield Automated Master Server (Fleet Guardian)",
                "hostname": net_info['hostname'],
                "ethernet_ip": net_info['primary_ethernet_ip'],
                "lan_ip": net_info['primary_lan_ip'],
                "primary_interface": net_info['primary_lan_dev'],
                "primary_type": net_info['primary_type'],
                "port": self.server.server_port,
                "auth_required": True,
                "interfaces": net_info['interfaces'],
                "uptime_seconds": int(time.time() - SERVER_START_TIME),
                "simulator_running": (SIMULATOR_PROCESS is not None and SIMULATOR_PROCESS.poll() is None),
                "database": db_stats
            })

        elif path == '/api/nodes':
            now = time.time()
            with FLEET_LOCK:
                nodes_copy = {}
                total_nodes = len(FLEET_NODES)
                online_nodes = 0
                real_nodes = 0
                sim_nodes = 0
                warning_nodes = 0
                critical_nodes = 0

                for nid, n in FLEET_NODES.items():
                    n_copy = dict(n)
                    # Node considered offline if no heartbeat within 15 seconds (3 missed heartbeats)
                    seconds_since_seen = now - n_copy.get('last_seen', 0)
                    is_online = seconds_since_seen <= 15.0
                    n_copy['status'] = 'ONLINE' if is_online else 'OFFLINE'
                    n_copy['seconds_since_seen'] = round(seconds_since_seen, 1)

                    if is_online:
                        online_nodes += 1
                        if n_copy.get('health') == 'WARNING':
                            warning_nodes += 1
                        elif n_copy.get('health') == 'CRITICAL':
                            critical_nodes += 1
                    else:
                        n_copy['health'] = 'OFFLINE'
                        warning_nodes += 1
                        # Update status in DB
                        database.update_node_status(nid, 'OFFLINE', 'OFFLINE')

                    if n_copy.get('is_simulated', False):
                        sim_nodes += 1
                    else:
                        real_nodes += 1

                    # Attach latest processes if not present in memory
                    if not n_copy.get('processes'):
                        n_copy['processes'] = database.get_node_latest_processes(nid)

                    nodes_copy[nid] = n_copy

                summary = {
                    "total_nodes": total_nodes,
                    "online_nodes": online_nodes,
                    "offline_nodes": total_nodes - online_nodes,
                    "real_nodes": real_nodes,
                    "sim_nodes": sim_nodes,
                    "warning_nodes": warning_nodes,
                    "critical_nodes": critical_nodes
                }

            self.send_json(200, {"nodes": nodes_copy, "summary": summary})

        elif path == '/api/alerts':
            now = time.time()
            active_alerts = []
            with FLEET_LOCK:
                for nid, n in FLEET_NODES.items():
                    sec_since = now - n.get('last_seen', 0)
                    is_online = sec_since <= 15.0
                    if not is_online:
                        active_alerts.append({
                            "node_id": nid,
                            "hostname": n.get('hostname'),
                            "ip": n.get('ip'),
                            "level": "WARNING",
                            "message": f"Heartbeat lost (OFFLINE for {int(sec_since)}s)"
                        })
                    elif n.get('health') in ('WARNING', 'CRITICAL'):
                        active_alerts.append({
                            "node_id": nid,
                            "hostname": n.get('hostname'),
                            "ip": n.get('ip'),
                            "level": n.get('health'),
                            "reasons": n.get('health_reasons', [])
                        })
            
            # Also fetch persistent recent alerts from DB
            recent_db_alerts = database.get_recent_alerts(limit=25)
            self.send_json(200, {"alerts": active_alerts, "history": recent_db_alerts})

        elif path == '/api/history':
            query = self.path.split('?')
            node_id = None
            hours = 24
            if len(query) > 1:
                params = query[1].split('&')
                for p in params:
                    if p.startswith('node_id='):
                        node_id = urllib_unquote(p.split('=', 1)[1])
                    elif p.startswith('hours='):
                        try:
                            hours = int(p.split('=', 1)[1])
                        except ValueError:
                            hours = 24

            if node_id:
                history = database.get_telemetry_history(node_id, hours=hours, limit=120)
                self.send_json(200, {"node_id": node_id, "hours": hours, "history": history})
            else:
                self.send_json(400, {"error": "Missing node_id parameter"})

        elif path == '/api/reports':
            reports = database.get_daily_reports_list(limit=50)
            self.send_json(200, {"reports": reports})

        elif path == '/api/automation/status':
            net_info = detect_network_interfaces()
            db_stats = database.get_db_stats()
            now = time.time()
            
            with FLEET_LOCK:
                total_nodes = len(FLEET_NODES)
                online_count = sum(1 for n in FLEET_NODES.values() if (now - n.get('last_seen', 0)) <= 15.0)

            status_payload = {
                "master_service": {
                    "status": "RUNNING",
                    "uptime_seconds": int(now - SERVER_START_TIME),
                    "started_at": time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(SERVER_START_TIME)),
                    "port": self.server.server_port,
                    "bind_address": "0.0.0.0"
                },
                "network_discovery": {
                    "status": "ACTIVE",
                    "service": "UDP Broadcast Responder",
                    "discovery_port": 8081,
                    "primary_ip": net_info['primary_lan_ip'],
                    "interface": net_info['primary_lan_dev']
                },
                "database_storage": {
                    "status": "ACTIVE",
                    "engine": "SQLite 3 (WAL Mode)",
                    "file": db_stats["db_path"],
                    "size_mb": db_stats["size_mb"],
                    "total_telemetry_samples": db_stats["total_telemetry_records"],
                    "retention_policy_days": DEFAULT_RETENTION_DAYS
                },
                "fleet_monitoring": {
                    "status": "ACTIVE",
                    "total_registered_nodes": total_nodes,
                    "online_nodes": online_count,
                    "offline_nodes": total_nodes - online_count,
                    "telemetry_interval_sec": 5,
                    "offline_timeout_sec": 15
                },
                "automated_reporting": {
                    "status": "ACTIVE",
                    "cadence": "Daily at midnight & on-demand",
                    "reports_generated": db_stats["total_reports"]
                },
                "auto_reconnection": {
                    "status": "SUPPORTED",
                    "protocol": "HTTP/1.1 REST + Pre-shared Auth Key"
                }
            }
            self.send_json(200, status_payload)

        else:
            self.send_json(404, {"error": "Endpoint not found"})

    def do_POST(self):
        global SIMULATOR_PROCESS
        path = self.path.split('?')[0]

        if path == '/api/report':
            try:
                client_auth_key = self.headers.get('X-ProcessShield-Key')
                content_len = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(content_len)
                data = json.loads(body.decode('utf-8'))

                if not client_auth_key:
                    client_auth_key = data.get('auth_key')

                if client_auth_key != SERVER_AUTH_KEY:
                    client_ip = self.client_address[0]
                    print(f" [!] SECURITY ALERT: Unauthorized connection attempt from {client_ip}. Invalid or missing security key.")
                    self.send_json(401, {
                        "status": "error",
                        "error": "Unauthorized: Invalid or missing ProcessShield security key"
                    })
                    return

                node_id = data.get('node_id') or f"{data.get('hostname')} ({data.get('ip')})"
                now = time.time()
                data['last_seen'] = now
                data['last_seen_time'] = time.strftime('%H:%M:%S', time.localtime(now))
                data['client_remote_ip'] = self.client_address[0]
                data['status'] = 'ONLINE'

                is_sim = data.get('is_simulated', False)

                # Persist directly into SQLite database
                database.record_telemetry(data)

                # Track in-memory cache
                with FLEET_LOCK:
                    is_new = node_id not in FLEET_NODES
                    prev_health = FLEET_NODES.get(node_id, {}).get('health')
                    FLEET_NODES[node_id] = data

                if is_new:
                    node_type = "SIMULATED VM" if is_sim else "REAL COMPUTER"
                    print(f" [+] REGISTERED NEW NODE: {node_id} [{node_type}] from LAN IP {self.client_address[0]}")
                    database.record_alert(node_id, "INFO", f"Node registered: {data.get('hostname')} ({data.get('ip')})")
                elif prev_health != data.get('health') and data.get('health') in ('WARNING', 'CRITICAL'):
                    reasons = ", ".join(data.get('health_reasons', []))
                    database.record_alert(node_id, data.get('health'), f"Health state changed to {data.get('health')}: {reasons}")

                self.send_json(200, {"status": "ok", "message": "Telemetry received, authenticated, and saved to database"})
            except Exception as e:
                self.send_json(400, {"status": "error", "message": str(e)})

        elif path == '/api/reports/generate':
            # Trigger daily report generation on demand
            try:
                reports = database.generate_daily_reports()
                self.send_json(200, {"status": "ok", "generated_count": len(reports), "reports": reports})
            except Exception as e:
                self.send_json(500, {"status": "error", "message": str(e)})

        elif path == '/api/simulator/toggle':
            with FLEET_LOCK:
                if SIMULATOR_PROCESS is not None and SIMULATOR_PROCESS.poll() is None:
                    SIMULATOR_PROCESS.terminate()
                    SIMULATOR_PROCESS = None
                    running = False
                    print(" [*] Simulated test fleet terminated.")
                else:
                    sim_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'simulator.py')
                    SIMULATOR_PROCESS = subprocess.Popen(
                        [sys.executable, sim_script, f"--server=http://127.0.0.1:{self.server.server_port}", f"--key={SERVER_AUTH_KEY}"],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL
                    )
                    running = True
                    print(" [*] Simulated test fleet launched.")

            self.send_json(200, {"status": "ok", "running": running})

        else:
            self.send_json(404, {"error": "Endpoint not found"})

def urllib_unquote(s):
    import urllib.parse
    return urllib.parse.unquote(s)

def background_maintenance_worker(retention_days=DEFAULT_RETENTION_DAYS):
    """
    Background worker thread:
    1. Prunes old database records every 6 hours to prevent unbounded disk growth.
    2. Generates daily summaries at midnight.
    """
    last_prune = 0
    last_daily_report_day = None

    while True:
        now = time.time()
        current_day = datetime.now().strftime('%Y-%m-%d')

        # 1. Prune every 6 hours
        if now - last_prune > 21600:
            last_prune = now
            try:
                pruned = database.prune_old_data(retention_days=retention_days)
                if pruned['deleted_telemetry'] > 0:
                    print(f" [DB Maintenance] Pruned {pruned['deleted_telemetry']} telemetry records older than {retention_days} days.")
            except Exception as e:
                pass

        # 2. Daily report generation once per day
        if last_daily_report_day != current_day:
            last_daily_report_day = current_day
            try:
                reports = database.generate_daily_reports(current_day)
                if reports:
                    print(f" [Daily Reporting] Automated daily reports generated for {len(reports)} monitored computers.")
            except Exception as e:
                pass

        time.sleep(60)

def load_initial_nodes_from_db():
    """Restores registered nodes from database so history is preserved across server restarts."""
    nodes = database.get_all_nodes()
    with FLEET_LOCK:
        for n in nodes:
            nid = n['node_id']
            # Load processes snapshot
            n['processes'] = database.get_node_latest_processes(nid)
            # Default to OFFLINE until new heartbeat is received
            n['status'] = 'OFFLINE'
            FLEET_NODES[nid] = n
    if nodes:
        print(f" [*] Loaded {len(nodes)} previously registered computers from persistent database.")

def run_master_server(host='0.0.0.0', port=8080, auth_key=DEFAULT_AUTH_KEY, retention_days=DEFAULT_RETENTION_DAYS, manual_interface=None, manual_ip=None):
    global SERVER_AUTH_KEY, DISCOVERY_SERVICE
    SERVER_AUTH_KEY = auth_key

    # Initialize persistent storage
    database.init_db()
    load_initial_nodes_from_db()

    net_info = detect_network_interfaces()
    hostname = net_info['hostname']
    ethernet_ip = manual_ip or net_info['primary_ethernet_ip']
    lan_ip = manual_ip or net_info['primary_lan_ip']
    primary_dev = manual_interface or net_info['primary_lan_dev']
    primary_type = net_info['primary_type']

    # Start network auto-discovery responder
    DISCOVERY_SERVICE = MasterDiscoveryService(master_ip=lan_ip, master_port=port, hostname=hostname)
    DISCOVERY_SERVICE.start()

    # Start background database maintenance & reporting worker
    maint_thread = threading.Thread(target=background_maintenance_worker, args=(retention_days,), daemon=True, name="MaintenanceWorker")
    maint_thread.start()

    server = ThreadedHTTPServer((host, port), MasterRequestHandler)

    print("\n" + "=" * 76)
    print("      PROCESSSHIELD FULLY AUTOMATED MASTER SERVER & FLEET GUARDIAN   ")
    print("=" * 76)
    print(f" [*] Master Hostname:      {hostname}")
    if ethernet_ip:
        print(f" [✓] Active Ethernet IP:   {ethernet_ip} (College Switch Ready)")
    else:
        print(f" [-] Physical Ethernet:    Not active (Check Ethernet cable)")
    print(f" [*] Primary LAN IP:       {lan_ip} ({primary_type} on {primary_dev})")
    print(f" [*] Web Dashboard:        http://{lan_ip}:{port} (or http://localhost:{port})")
    print(f" [*] Telemetry Endpoint:   http://{lan_ip}:{port}/api/report")
    print(f" [*] Automation Status:    http://{lan_ip}:{port}/api/automation/status")
    print(f" [*] Persistent DB:        {database.DB_PATH}")
    print(f" [*] Data Retention:       {retention_days} days (Daily reports kept indefinitely)")
    print(f" [*] UDP Auto-Discovery:   Port 8081 (Zero-config pairing)")
    print(f" [*] Security Key:         {auth_key}")
    print("=" * 76)

    print(" [*] Detected Physical & Virtual Network Interfaces:")
    for iface in net_info['interfaces']:
        if iface['ips'] or iface['is_ethernet'] or iface['status'] == 'active':
            ip_str = ", ".join(iface['ips']) if iface['ips'] else "No IP (Disconnected)"
            stat_str = "ACTIVE" if iface['status'] == 'active' else "INACTIVE"
            tag = "[ETHERNET]" if iface['is_ethernet'] else ("[WI-FI]" if iface['is_wifi'] else "[INTERFACE]")
            print(f"     • {iface['device']} {tag} ({iface['hw_name']}): {ip_str} [{stat_str}]")

    print("-" * 76)
    active_ip = ethernet_ip or lan_ip
    print(" [*] FULLY AUTOMATED CLIENT INSTRUCTIONS (No daily intervention needed):")
    print(f"     1. Connect client computer to the same Ethernet switch.")
    print(f"     2. Run Automated Agent:")
    print(f"        python3 agent.py --server http://{active_ip}:{port} --key {auth_key}")
    print(f"        (Or omit --server to let it auto-discover via UDP!)")
    print(f"     3. Or run Native C++ Agent:")
    print(f"        ./processshield-agent --host {active_ip} --port {port} --key {auth_key}")
    print(f"     4. View live fleet & 24h history at http://localhost:{port}")
    print(" [*] Press Ctrl+C to shut down Master Server.")
    print("=" * 76 + "\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Shutting down Master Server...")
    finally:
        global SIMULATOR_PROCESS
        if SIMULATOR_PROCESS is not None and SIMULATOR_PROCESS.poll() is None:
            SIMULATOR_PROCESS.terminate()
        if DISCOVERY_SERVICE:
            DISCOVERY_SERVICE.stop()
        server.server_close()
        print("[*] Master Server terminated cleanly.")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description="ProcessShield Automated Master Server")
    parser.add_argument("--host", default="0.0.0.0", help="Binding host (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default: 8080)")
    parser.add_argument("--auth-key", default=DEFAULT_AUTH_KEY, help=f"Security key (default: {DEFAULT_AUTH_KEY})")
    parser.add_argument("--retention-days", type=int, default=DEFAULT_RETENTION_DAYS, help=f"Data retention in days (default: {DEFAULT_RETENTION_DAYS})")
    parser.add_argument("--interface", default=None, help="Force primary network interface")
    parser.add_argument("--ip", default=None, help="Force primary IP address manually")
    args = parser.parse_args()

    run_master_server(
        host=args.host,
        port=args.port,
        auth_key=args.auth_key,
        retention_days=args.retention_days,
        manual_interface=args.interface,
        manual_ip=args.ip
    )
