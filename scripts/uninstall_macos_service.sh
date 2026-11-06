#!/usr/bin/env bash
# ProcessShield macOS LaunchAgent Service Uninstaller

SERVICE_TYPE="${1:-all}"
LAUNCH_AGENTS_DIR="$HOME/Library/LaunchAgents"

remove_service() {
    local name="$1"
    local plist="$LAUNCH_AGENTS_DIR/$name"
    if [ -f "$plist" ]; then
        launchctl unload "$plist" 2>/dev/null || true
        rm -f "$plist"
        echo " [✓] Removed $name"
    fi
}

if [ "$SERVICE_TYPE" = "master" ]; then
    remove_service "com.processshield.master.plist"
elif [ "$SERVICE_TYPE" = "agent" ]; then
    remove_service "com.processshield.agent.plist"
else
    remove_service "com.processshield.master.plist"
    remove_service "com.processshield.agent.plist"
fi

echo "ProcessShield LaunchAgents uninstalled."

