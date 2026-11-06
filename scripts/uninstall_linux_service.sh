#!/usr/bin/env bash
# ProcessShield Linux Systemd Service Uninstaller

SERVICE_TYPE="${1:-all}"
SYSTEMD_DIR="/etc/systemd/system"

if [ "$EUID" -ne 0 ]; then
    echo "[-] Please run as root (sudo $0 [master|agent])"
    exit 1
fi

remove_svc() {
    local svc="$1"
    if [ -f "$SYSTEMD_DIR/$svc" ]; then
        systemctl stop "$svc" 2>/dev/null || true
        systemctl disable "$svc" 2>/dev/null || true
        rm -f "$SYSTEMD_DIR/$svc"
        echo " [✓] Removed $svc"
    fi
}

if [ "$SERVICE_TYPE" = "master" ]; then
    remove_svc "processshield-master.service"
elif [ "$SERVICE_TYPE" = "agent" ]; then
    remove_svc "processshield-agent.service"
else
    remove_svc "processshield-master.service"
    remove_svc "processshield-agent.service"
fi

systemctl daemon-reload
echo "ProcessShield systemd services removed."

