#!/usr/bin/env bash
# ProcessShield Linux Systemd Service Installer
# Requires sudo / root privileges.

set -e

SERVICE_TYPE="${1:-agent}"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$(which python3)"
SYSTEMD_DIR="/etc/systemd/system"

if [ "$EUID" -ne 0 ]; then
    echo "[-] Please run as root (sudo $0 [master|agent])"
    exit 1
fi

mkdir -p "$CURRENT_DIR/logs"

if [ "$SERVICE_TYPE" = "master" ]; then
    SERVICE_NAME="processshield-master.service"
    EXEC_CMD="$PYTHON_BIN $CURRENT_DIR/master_server.py --port 8080"
    DESC="ProcessShield Master Monitoring Service"
elif [ "$SERVICE_TYPE" = "agent" ]; then
    SERVICE_NAME="processshield-agent.service"
    MASTER_URL="${2:-}"
    if [ -n "$MASTER_URL" ]; then
        EXEC_CMD="$PYTHON_BIN $CURRENT_DIR/agent.py --server $MASTER_URL"
    else
        EXEC_CMD="$PYTHON_BIN $CURRENT_DIR/agent.py"
    fi
    DESC="ProcessShield Client Monitoring Agent"
else
    echo "Usage: sudo $0 [master|agent] [optional_master_url_for_agent]"
    exit 1
fi

cat <<EOF > "$SYSTEMD_DIR/$SERVICE_NAME"
[Unit]
Description=$DESC
After=network.target

[Service]
Type=simple
User=${SUDO_USER:-root}
WorkingDirectory=$CURRENT_DIR
ExecStart=$EXEC_CMD
Restart=always
RestartSec=5
StandardOutput=append:$CURRENT_DIR/logs/${SERVICE_TYPE}_stdout.log
StandardError=append:$CURRENT_DIR/logs/${SERVICE_TYPE}_stderr.log

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"

echo "============================================================"
echo " [✓] ProcessShield Linux Service $SERVICE_NAME Installed!"
echo "============================================================"
echo " • Status:       systemctl status $SERVICE_NAME"
echo " • Logs:         journalctl -u $SERVICE_NAME -f"
echo " • Auto-restart: Enabled across reboots"
echo "============================================================"

