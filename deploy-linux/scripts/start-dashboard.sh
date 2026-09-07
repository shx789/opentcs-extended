#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

require_dir "$ADAPTER_ROOT" ADAPTER_ROOT
require_dir "$DASHBOARD_ROOT" DASHBOARD_ROOT

export AGV_ADAPTER_ROOT="$ADAPTER_ROOT"
export AGV_DASHBOARD_ROOT="$DASHBOARD_ROOT"
export AGV_DASHBOARD_HOST="$DASHBOARD_HOST"
export AGV_DASHBOARD_PORT="$DASHBOARD_PORT"
export AGV_DASHBOARD_OPENTCS_URL="$OPENTCS_HTTP_URL"
export AGV_DASHBOARD_RCS_URL="$RCS_HTTP_URL"
export AGV_DASHBOARD_MQTT_URI="$AGV_MQTT_URI_WEB"

if [[ -f "$AGV_SUITE_HOME/venv/bin/python" ]]; then
  PY="$AGV_SUITE_HOME/venv/bin/python"
else
  PY="$PYTHON_BIN"
fi

run_or_start agv-dashboard "$PY" "$DASHBOARD_ROOT/map_mapping_output/agv_opentcs_dashboard_server.py"
