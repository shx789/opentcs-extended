#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

"$SCRIPT_DIR/start-kernel.sh"
sleep 5
"$SCRIPT_DIR/start-rcs.sh"
sleep 2
"$SCRIPT_DIR/start-config-web.sh"
"$SCRIPT_DIR/start-dashboard.sh"

echo
echo "Started. Check pages:"
echo "  config web: http://<server-ip>:$CONFIG_WEB_PORT/"
echo "  dashboard : http://<server-ip>:$DASHBOARD_PORT/"
echo "Logs are under: $LOG_DIR"
