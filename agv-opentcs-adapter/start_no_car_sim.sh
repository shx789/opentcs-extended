#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$ROOT_DIR/logs"
LOG_FILE="$ROOT_DIR/logs/agv_no_car_feedback_simulator.log"
PID_FILE="$ROOT_DIR/logs/agv_no_car_feedback_simulator.pid"
if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "No-car simulator is already running: pid=$(cat "$PID_FILE")"
  exit 0
fi
setsid python3 -u "$ROOT_DIR/bin/agv_no_car_feedback_simulator.py" > "$LOG_FILE" 2>&1 < /dev/null &
echo $! > "$PID_FILE"
echo "Started no-car simulator: pid=$(cat "$PID_FILE") log=$LOG_FILE"
