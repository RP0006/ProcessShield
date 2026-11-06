#!/usr/bin/env python3
"""
ProcessShield Network Auto-Discovery Service
Enables zero-configuration pairing between Master Server and Agents over Ethernet LAN.
Uses lightweight UDP broadcast beacons and discovery request/response handshakes.
"""

import time
import json
import socket
import threading

DISCOVERY_PORT = 8081
MAGIC_REQUEST = b"PROCESSSHIELD_DISCOVER_V1"

class MasterDiscoveryService:
    """Runs on Master Server to announce its presence on the Ethernet / LAN."""
    def __init__(self, master_ip, master_port, hostname, broadcast_interval=4):
        self.master_ip = master_ip
        self.master_port = master_port
        self.hostname = hostname
        self.broadcast_interval = broadcast_interval
        self.running = False
        self._thread = None
        self._sock = None

    def start(self):
        if self.running:
            return
        self.running = True
        self._thread = threading.Thread(target=self._run_loop, daemon=True, name="DiscoveryDaemon")
        self._thread.start()

    def stop(self):
        self.running = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass

    def _run_loop(self):
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            if hasattr(socket, 'SO_REUSEPORT'):
                self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._sock.settimeout(0.5)
            self._sock.bind(('0.0.0.0', DISCOVERY_PORT))
        except Exception as e:
            return

        payload = json.dumps({
            "service": "ProcessShield-Master",
            "version": "2.0",
            "ip": self.master_ip,
            "port": self.master_port,
            "hostname": self.hostname
        }).encode('utf-8')

        last_beacon = 0
        while self.running:
            now = time.time()
            # Periodic broadcast beacon to LAN
            if now - last_beacon >= self.broadcast_interval:
                last_beacon = now
                try:
                    self._sock.sendto(payload, ('255.255.255.255', DISCOVERY_PORT))
                    self._sock.sendto(payload, ('127.0.0.1', DISCOVERY_PORT))
                except Exception:
                    pass

            # Listen for discovery requests
            try:
                data, addr = self._sock.recvfrom(1024)
                if MAGIC_REQUEST in data or b"DISCOVER" in data:
                    self._sock.sendto(payload, addr)
            except (socket.timeout, OSError):
                continue
            except Exception:
                pass

def discover_master_server(timeout=2.0, port=DISCOVERY_PORT):
    """
    Called by Agent to automatically locate Master Server on the LAN without manual IP configuration.
    Returns: dict with 'ip', 'port', 'hostname' or None.
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(0.6)

        # Broadcast probe to LAN and localhost
        try:
            sock.sendto(MAGIC_REQUEST, ('255.255.255.255', port))
            sock.sendto(MAGIC_REQUEST, ('127.0.0.1', port))
        except Exception:
            pass

        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                data, addr = sock.recvfrom(2048)
                resp = json.loads(data.decode('utf-8'))
                if resp.get('service') == 'ProcessShield-Master':
                    sock.close()
                    # If server sent 0.0.0.0 or 127.0.0.1 but we received from remote addr, use remote IP
                    if resp.get('ip') in ('0.0.0.0', None):
                        resp['ip'] = addr[0]
                    return resp
            except socket.timeout:
                # Re-try sending probe if not heard yet
                try:
                    sock.sendto(MAGIC_REQUEST, ('255.255.255.255', port))
                    sock.sendto(MAGIC_REQUEST, ('127.0.0.1', port))
                except Exception:
                    pass
            except Exception:
                continue
        sock.close()
    except Exception:
        pass
    return None
