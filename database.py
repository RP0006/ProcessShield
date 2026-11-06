#!/usr/bin/env python3
"""
ProcessShield Persistent Storage & Database Engine
Uses SQLite for zero-configuration, robust, file-backed telemetry logging,
historical queries, alerts tracking, data retention pruning, and daily reporting.
"""

import os
import json
import time
import sqlite3
import threading
from datetime import datetime, timedelta

DB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
DB_PATH = os.path.join(DB_DIR, "processshield.db")
REPORTS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "reports")

# Thread-local storage for safe multi-threaded SQLite connections
_local = threading.local()

def get_connection():
    if not hasattr(_local, "connection") or _local.connection is None:
        os.makedirs(DB_DIR, exist_ok=True)
        os.makedirs(REPORTS_DIR, exist_ok=True)
        conn = sqlite3.connect(DB_PATH, timeout=10.0, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        # Enable WAL mode for concurrent reads and writes
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute("PRAGMA synchronous=NORMAL;")
        _local.connection = conn
    return _local.connection

def init_db():
    """Initializes tables and indexes for ProcessShield."""
    conn = get_connection()
    with conn:
        # 1. Registered Nodes Table
        conn.execute("""
            CREATE TABLE IF NOT EXISTS nodes (
                node_id TEXT PRIMARY KEY,
                hostname TEXT NOT NULL,
                ip TEXT NOT NULL,
                mac TEXT,
                interface TEXT,
                system_type TEXT,
                is_simulated INTEGER DEFAULT 0,
                first_seen REAL,
                last_seen REAL,
                status TEXT DEFAULT 'ONLINE',
                health TEXT DEFAULT 'HEALTHY'
            );
        """)

        # 2. Telemetry History Table
        conn.execute("""
            CREATE TABLE IF NOT EXISTS telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT NOT NULL,
                timestamp REAL NOT NULL,
                time_str TEXT NOT NULL,
                cpu_percent REAL,
                ram_percent REAL,
                ram_used_gb REAL,
                ram_total_gb REAL,
                disk_percent REAL,
                disk_used_gb REAL,
                disk_total_gb REAL,
                net_sent_kb REAL DEFAULT 0,
                net_recv_kb REAL DEFAULT 0,
                process_count INTEGER DEFAULT 0,
                health TEXT DEFAULT 'HEALTHY',
                health_reasons TEXT,
                FOREIGN KEY(node_id) REFERENCES nodes(node_id) ON DELETE CASCADE
            );
        """)

        # 3. Process Snapshots Table (Latest process table per node)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS process_snapshots (
                node_id TEXT PRIMARY KEY,
                updated_at REAL NOT NULL,
                processes_json TEXT NOT NULL,
                FOREIGN KEY(node_id) REFERENCES nodes(node_id) ON DELETE CASCADE
            );
        """)

        # 4. Alerts History Table
        conn.execute("""
            CREATE TABLE IF NOT EXISTS alerts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT NOT NULL,
                timestamp REAL NOT NULL,
                time_str TEXT NOT NULL,
                level TEXT NOT NULL,
                message TEXT NOT NULL,
                resolved INTEGER DEFAULT 0
            );
        """)

        # 5. Daily Aggregated Reports Table
        conn.execute("""
            CREATE TABLE IF NOT EXISTS daily_reports (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id TEXT NOT NULL,
                report_date TEXT NOT NULL,
                hostname TEXT NOT NULL,
                is_simulated INTEGER DEFAULT 0,
                avg_cpu REAL,
                max_cpu REAL,
                avg_ram REAL,
                max_ram REAL,
                avg_disk REAL,
                max_disk REAL,
                uptime_percent REAL,
                samples_count INTEGER,
                alerts_count INTEGER,
                generated_at REAL,
                summary_json TEXT,
                UNIQUE(node_id, report_date)
            );
        """)

        # Indexes for fast querying
        conn.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_node_time ON telemetry(node_id, timestamp);")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry(timestamp);")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_alerts_time ON alerts(timestamp);")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_daily_reports_date ON daily_reports(report_date);")

def upsert_node(node_dict):
    """Inserts or updates node registration."""
    conn = get_connection()
    now = time.time()
    node_id = node_dict.get('node_id') or f"{node_dict.get('hostname')} ({node_dict.get('ip')})"
    hostname = node_dict.get('hostname', 'unknown')
    ip = node_dict.get('ip', '0.0.0.0')
    mac = node_dict.get('mac')
    interface = node_dict.get('interface')
    sys_type = node_dict.get('system_type', 'macOS')
    is_sim = 1 if node_dict.get('is_simulated', False) else 0
    health = node_dict.get('health', 'HEALTHY')
    status = node_dict.get('status', 'ONLINE')

    with conn:
        conn.execute("""
            INSERT INTO nodes (node_id, hostname, ip, mac, interface, system_type, is_simulated, first_seen, last_seen, status, health)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(node_id) DO UPDATE SET
                hostname=excluded.hostname,
                ip=excluded.ip,
                mac=COALESCE(excluded.mac, nodes.mac),
                interface=COALESCE(excluded.interface, nodes.interface),
                system_type=excluded.system_type,
                is_simulated=excluded.is_simulated,
                last_seen=excluded.last_seen,
                status=excluded.status,
                health=excluded.health;
        """, (node_id, hostname, ip, mac, interface, sys_type, is_sim, now, now, status, health))
    return node_id

def record_telemetry(node_dict):
    """Saves a telemetry point to the persistent database."""
    node_id = upsert_node(node_dict)
    conn = get_connection()
    now = time.time()
    time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(now))

    cpu = float(node_dict.get('cpu_percent', 0.0))
    ram_pct = float(node_dict.get('ram_percent', 0.0))
    ram_used = float(node_dict.get('ram_used_gb', 0.0))
    ram_total = float(node_dict.get('ram_total_gb', 0.0))
    disk_pct = float(node_dict.get('disk_percent', 0.0))
    disk_used = float(node_dict.get('disk_used_gb', 0.0))
    disk_total = float(node_dict.get('disk_total_gb', 0.0))
    net_sent = float(node_dict.get('net_sent_kb', 0.0))
    net_recv = float(node_dict.get('net_recv_kb', 0.0))
    procs_count = int(node_dict.get('process_count', 0))
    health = node_dict.get('health', 'HEALTHY')
    reasons = json.dumps(node_dict.get('health_reasons', []))

    with conn:
        conn.execute("""
            INSERT INTO telemetry (
                node_id, timestamp, time_str, cpu_percent, ram_percent,
                ram_used_gb, ram_total_gb, disk_percent, disk_used_gb, disk_total_gb,
                net_sent_kb, net_recv_kb, process_count, health, health_reasons
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        """, (
            node_id, now, time_str, cpu, ram_pct,
            ram_used, ram_total, disk_pct, disk_used, disk_total,
            net_sent, net_recv, procs_count, health, reasons
        ))

        # Save latest processes list if provided
        procs = node_dict.get('processes')
        if procs is not None:
            conn.execute("""
                INSERT INTO process_snapshots (node_id, updated_at, processes_json)
                VALUES (?, ?, ?)
                ON CONFLICT(node_id) DO UPDATE SET
                    updated_at=excluded.updated_at,
                    processes_json=excluded.processes_json;
            """, (node_id, now, json.dumps(procs)))

    return node_id

def update_node_status(node_id, status, health=None):
    """Updates status (e.g. OFFLINE) and optional health in nodes table."""
    conn = get_connection()
    with conn:
        if health:
            conn.execute("UPDATE nodes SET status=?, health=? WHERE node_id=?;", (status, health, node_id))
        else:
            conn.execute("UPDATE nodes SET status=? WHERE node_id=?;", (status, node_id))

def get_all_nodes():
    """Retrieves all registered nodes from database."""
    conn = get_connection()
    cur = conn.cursor()
    cur.execute("SELECT * FROM nodes ORDER BY is_simulated ASC, hostname ASC;")
    rows = cur.fetchall()
    nodes = []
    for r in rows:
        n = dict(r)
        n['is_simulated'] = bool(n['is_simulated'])
        nodes.append(n)
    return nodes

def get_node_latest_processes(node_id):
    """Fetches the latest process snapshot for a given node."""
    conn = get_connection()
    cur = conn.cursor()
    cur.execute("SELECT processes_json FROM process_snapshots WHERE node_id=?;", (node_id,))
    row = cur.fetchone()
    if row and row['processes_json']:
        try:
            return json.loads(row['processes_json'])
        except Exception:
            return []
    return []

def get_telemetry_history(node_id, hours=24, limit=120):
    """
    Returns time-series monitoring history for a node over the specified hours.
    Downsamples if total points exceed limit to keep UI smooth.
    """
    conn = get_connection()
    since_ts = time.time() - (hours * 3600)
    cur = conn.cursor()
    cur.execute("""
        SELECT timestamp, time_str, cpu_percent, ram_percent, disk_percent,
               net_sent_kb, net_recv_kb, process_count, health
        FROM telemetry
        WHERE node_id=? AND timestamp >= ?
        ORDER BY timestamp ASC;
    """, (node_id, since_ts))
    rows = cur.fetchall()
    
    if not rows:
        return []

    # Downsample if more than limit
    total = len(rows)
    if total > limit:
        step = max(1, total // limit)
        selected = [dict(rows[i]) for i in range(0, total, step)]
        # Always include the very last reading
        if selected[-1]['timestamp'] != rows[-1]['timestamp']:
            selected.append(dict(rows[-1]))
        return selected

    return [dict(r) for r in rows]

def record_alert(node_id, level, message):
    """Logs an alert to the database."""
    conn = get_connection()
    now = time.time()
    time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(now))
    with conn:
        conn.execute("""
            INSERT INTO alerts (node_id, timestamp, time_str, level, message, resolved)
            VALUES (?, ?, ?, ?, ?, 0);
        """, (node_id, now, time_str, level, message))

def get_recent_alerts(limit=50):
    """Fetches the most recent alerts."""
    conn = get_connection()
    cur = conn.cursor()
    cur.execute("""
        SELECT a.*, n.hostname, n.ip, n.is_simulated
        FROM alerts a
        LEFT JOIN nodes n ON a.node_id = n.node_id
        ORDER BY a.timestamp DESC
        LIMIT ?;
    """, (limit,))
    return [dict(r) for r in cur.fetchall()]

def prune_old_data(retention_days=7):
    """
    Deletes telemetry data older than retention_days.
    Retains daily reports forever, preventing disk exhaustion.
    """
    conn = get_connection()
    cutoff_ts = time.time() - (retention_days * 86400)
    with conn:
        cur = conn.execute("DELETE FROM telemetry WHERE timestamp < ?;", (cutoff_ts,))
        deleted_telemetry = cur.rowcount
        cur = conn.execute("DELETE FROM alerts WHERE timestamp < ? AND resolved=1;", (cutoff_ts,))
        deleted_alerts = cur.rowcount
    return {"deleted_telemetry": deleted_telemetry, "deleted_alerts": deleted_alerts}

def generate_daily_reports(target_date_str=None):
    """
    Aggregates telemetry records for a given date (default: today) for each node,
    creates persistent summary records in daily_reports, and writes JSON/markdown reports.
    """
    conn = get_connection()
    if not target_date_str:
        target_date_str = datetime.now().strftime('%Y-%m-%d')

    start_dt = datetime.strptime(target_date_str, '%Y-%m-%d')
    start_ts = start_dt.timestamp()
    end_ts = (start_dt + timedelta(days=1)).timestamp()

    # Get all nodes that reported on this day
    cur = conn.cursor()
    cur.execute("""
        SELECT DISTINCT node_id FROM telemetry
        WHERE timestamp >= ? AND timestamp < ?;
    """, (start_ts, end_ts))
    node_ids = [r['node_id'] for r in cur.fetchall()]

    generated_reports = []

    for nid in node_ids:
        cur.execute("SELECT * FROM nodes WHERE node_id=?;", (nid,))
        node_row = cur.fetchone()
        hostname = node_row['hostname'] if node_row else nid
        is_sim = node_row['is_simulated'] if node_row else 0

        # Aggregate metrics
        cur.execute("""
            SELECT 
                COUNT(*) as count,
                AVG(cpu_percent) as avg_cpu,
                MAX(cpu_percent) as max_cpu,
                MIN(cpu_percent) as min_cpu,
                AVG(ram_percent) as avg_ram,
                MAX(ram_percent) as max_ram,
                AVG(disk_percent) as avg_disk,
                MAX(disk_percent) as max_disk,
                SUM(CASE WHEN health != 'OFFLINE' THEN 1 ELSE 0 END) as online_samples
            FROM telemetry
            WHERE node_id=? AND timestamp >= ? AND timestamp < ?;
        """, (nid, start_ts, end_ts))
        metrics = cur.fetchone()

        if not metrics or metrics['count'] == 0:
            continue

        count = metrics['count']
        avg_cpu = round(metrics['avg_cpu'] or 0.0, 1)
        max_cpu = round(metrics['max_cpu'] or 0.0, 1)
        avg_ram = round(metrics['avg_ram'] or 0.0, 1)
        max_ram = round(metrics['max_ram'] or 0.0, 1)
        avg_disk = round(metrics['avg_disk'] or 0.0, 1)
        max_disk = round(metrics['max_disk'] or 0.0, 1)
        
        # Calculate uptime estimate (based on expected samples in 24h at 5s interval = 17280)
        # Or percentage of actual reported intervals
        uptime_pct = round(min(100.0, (metrics['online_samples'] / max(1, count)) * 100.0), 1)

        # Count alerts on this day
        cur.execute("""
            SELECT COUNT(*) as alert_count FROM alerts
            WHERE node_id=? AND timestamp >= ? AND timestamp < ?;
        """, (nid, start_ts, end_ts))
        alert_count = cur.fetchone()['alert_count']

        summary_data = {
            "node_id": nid,
            "hostname": hostname,
            "report_date": target_date_str,
            "is_simulated": bool(is_sim),
            "samples_collected": count,
            "uptime_percent": uptime_pct,
            "alerts_count": alert_count,
            "cpu": {"avg": avg_cpu, "max": max_cpu},
            "ram": {"avg": avg_ram, "max": max_ram},
            "disk": {"avg": avg_disk, "max": max_disk},
            "generated_at": time.time()
        }

        with conn:
            conn.execute("""
                INSERT INTO daily_reports (
                    node_id, report_date, hostname, is_simulated,
                    avg_cpu, max_cpu, avg_ram, max_ram, avg_disk, max_disk,
                    uptime_percent, samples_count, alerts_count, generated_at, summary_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(node_id, report_date) DO UPDATE SET
                    avg_cpu=excluded.avg_cpu,
                    max_cpu=excluded.max_cpu,
                    avg_ram=excluded.avg_ram,
                    max_ram=excluded.max_ram,
                    avg_disk=excluded.avg_disk,
                    max_disk=excluded.max_disk,
                    uptime_percent=excluded.uptime_percent,
                    samples_count=excluded.samples_count,
                    alerts_count=excluded.alerts_count,
                    generated_at=excluded.generated_at,
                    summary_json=excluded.summary_json;
            """, (
                nid, target_date_str, hostname, is_sim,
                avg_cpu, max_cpu, avg_ram, max_ram, avg_disk, max_disk,
                uptime_pct, count, alert_count, time.time(), json.dumps(summary_data)
            ))

        # Also write report file to reports/
        safe_name = hostname.replace('/', '_').replace(' ', '_')
        report_file = os.path.join(REPORTS_DIR, f"daily_report_{safe_name}_{target_date_str}.json")
        try:
            with open(report_file, 'w', encoding='utf-8') as f:
                json.dump(summary_data, f, indent=2)
        except Exception:
            pass

        generated_reports.append(summary_data)

    return generated_reports

def get_daily_reports_list(node_id=None, limit=30):
    """Fetches daily reports from database."""
    conn = get_connection()
    cur = conn.cursor()
    if node_id:
        cur.execute("""
            SELECT * FROM daily_reports
            WHERE node_id=?
            ORDER BY report_date DESC
            LIMIT ?;
        """, (node_id, limit))
    else:
        cur.execute("""
            SELECT * FROM daily_reports
            ORDER BY report_date DESC, hostname ASC
            LIMIT ?;
        """, (limit,))
    
    rows = cur.fetchall()
    reports = []
    for r in rows:
        item = dict(r)
        item['is_simulated'] = bool(item['is_simulated'])
        if item.get('summary_json'):
            try:
                item['summary'] = json.loads(item['summary_json'])
            except Exception:
                item['summary'] = {}
        reports.append(item)
    return reports

def get_db_stats():
    """Returns database size, row counts, and storage metrics."""
    conn = get_connection()
    cur = conn.cursor()
    cur.execute("SELECT COUNT(*) FROM nodes;")
    node_count = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM telemetry;")
    telemetry_count = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM alerts;")
    alert_count = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM daily_reports;")
    report_count = cur.fetchone()[0]

    size_bytes = 0
    if os.path.exists(DB_PATH):
        size_bytes = os.path.getsize(DB_PATH)

    return {
        "db_path": DB_PATH,
        "size_kb": round(size_bytes / 1024.0, 1),
        "size_mb": round(size_bytes / (1024.0 * 1024.0), 2),
        "total_nodes": node_count,
        "total_telemetry_records": telemetry_count,
        "total_alerts": alert_count,
        "total_reports": report_count
    }
