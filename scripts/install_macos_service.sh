#!/usr/bin/env bash
# ProcessShield macOS LaunchAgent Service Installer
# Configures ProcessShield Master Server or Client Agent to auto-start on macOS boot/login.

set -e

SERVICE_TYPE="${1:-master}"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$(which python3)"
LAUNCH_AGENTS_DIR="$HOME/Library/LaunchAgents"

mkdir -p "$LAUNCH_AGENTS_DIR"
mkdir -p "$CURRENT_DIR/logs"

if [ "$SERVICE_TYPE" = "master" ]; then
    PLIST_NAME="com.processshield.master.plist"
    TARGET_PLIST="$LAUNCH_AGENTS_DIR/$PLIST_NAME"
    LABEL="com.processshield.master"
    PROGRAM_ARGS="<string>$PYTHON_BIN</string><string>$CURRENT_DIR/master_server.py</string><string>--port</string><string>8080</string>"
    LOG_OUT="$CURRENT_DIR/logs/master_stdout.log"
    LOG_ERR="$CURRENT_DIR/logs/master_stderr.log"
    DESC="ProcessShield Master Server Monitoring Daemon"
elif [ "$SERVICE_TYPE" = "agent" ]; then
    PLIST_NAME="com.processshield.agent.plist"
    TARGET_PLIST="$LAUNCH_AGENTS_DIR/$PLIST_NAME"
    LABEL="com.processshield.agent"
    MASTER_URL="${2:-}"
    if [ -n "$MASTER_URL" ]; then
        PROGRAM_ARGS="<string>$PYTHON_BIN</string><string>$CURRENT_DIR/agent.py</string><string>--server</string><string>$MASTER_URL</string>"
    else
        PROGRAM_ARGS="<string>$PYTHON_BIN</string><string>$CURRENT_DIR/agent.py</string>"
    fi
    LOG_OUT="$CURRENT_DIR/logs/agent_stdout.log"
    LOG_ERR="$CURRENT_DIR/logs/agent_stderr.log"
    DESC="ProcessShield Client Monitoring Agent Daemon"
else
    echo "Usage: $0 [master|agent] [optional_master_url_for_agent]"
    exit 1
fi

# Stop existing service if loaded
launchctl unload "$TARGET_PLIST" 2>/dev/null || true

cat <<EOF > "$TARGET_PLIST"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$LABEL</string>
    <key>ProgramArguments</key>
    <array>
        $PROGRAM_ARGS
    </array>
    <key>WorkingDirectory</key>
    <string>$CURRENT_DIR</string>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>$LOG_OUT</string>
    <key>StandardErrorPath</key>
    <string>$LOG_ERR</string>
</dict>
</plist>
EOF

chmod 644 "$TARGET_PLIST"
launchctl load "$TARGET_PLIST"

echo "============================================================"
echo " [✓] ProcessShield $DESC Installed Successfully!"
echo "============================================================"
echo " • Service Plist:    $TARGET_PLIST"
echo " • Working Dir:      $CURRENT_DIR"
echo " • Logs:             $LOG_OUT"
echo " • Autostart:        Enabled on user login & system startup"
echo " • Status Check:     launchctl list | grep processshield"
echo " • To Uninstall:     $CURRENT_DIR/scripts/uninstall_macos_service.sh $SERVICE_TYPE"
echo "============================================================"

