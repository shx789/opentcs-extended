#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$ROOT_DIR/logs/agv_no_car_feedback_simulator.pid"
if [[ ! -f "$PID_FILE" ]]; then
  echo "No-car simulator pid file not found."
  exit 0
fi
PID="$(cat "$PID_FILE")"
if kill -0 "$PID" 2>/dev/null; then
  kill "$PID"
  echo "Stopped no-car simulator: pid=$PID"
else
  echo "No-car simulator was not running: pid=$PID"
fi
rm -f "$PID_FILE"
