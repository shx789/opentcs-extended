#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${1:-$ROOT_DIR/.env}"
if [[ ! -f "$ENV_FILE" ]]; then
  echo "Missing env file: $ENV_FILE" >&2
  exit 1
fi
set -a
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a
mkdir -p "$ROOT_DIR/logs"
LOG_FILE="$ROOT_DIR/logs/agv_native_feedback_adapter.log"
PID_FILE="$ROOT_DIR/logs/agv_native_feedback_adapter.pid"
if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "Adapter is already running: pid=$(cat "$PID_FILE")"
  exit 0
fi
setsid python3 -u "$ROOT_DIR/bin/agv_native_feedback_adapter.py" > "$LOG_FILE" 2>&1 < /dev/null &
echo $! > "$PID_FILE"
echo "Started AGV adapter: pid=$(cat "$PID_FILE") log=$LOG_FILE"
