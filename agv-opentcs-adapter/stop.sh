#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$ROOT_DIR/logs/agv_native_feedback_adapter.pid"
if [[ ! -f "$PID_FILE" ]]; then
  echo "Adapter pid file not found."
  exit 0
fi
PID="$(cat "$PID_FILE")"
if kill -0 "$PID" 2>/dev/null; then
  kill "$PID"
  echo "Stopped AGV adapter: pid=$PID"
else
  echo "Adapter was not running: pid=$PID"
fi
rm -f "$PID_FILE"
