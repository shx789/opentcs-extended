#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

require_dir "$ADAPTER_ROOT" ADAPTER_ROOT

export AGV_CONFIG_WEB_HOST="$CONFIG_WEB_HOST"
export AGV_CONFIG_WEB_PORT="$CONFIG_WEB_PORT"
export AGV_ADAPTER_RUNTIME_CONFIG_FILE="$ADAPTER_ROOT/config/runtime_config.json"

if [[ -f "$AGV_SUITE_HOME/venv/bin/python" ]]; then
  PY="$AGV_SUITE_HOME/venv/bin/python"
else
  PY="$PYTHON_BIN"
fi

run_or_start agv-config-web "$PY" "$ADAPTER_ROOT/bin/agv_config_web.py"
