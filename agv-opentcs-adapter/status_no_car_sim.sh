#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$ROOT_DIR/logs/agv_no_car_feedback_simulator.pid"
STATUS_FILE="$ROOT_DIR/logs/agv_no_car_feedback_simulator_status.json"
if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
  echo "process: running pid=$(cat "$PID_FILE")"
else
  echo "process: stopped"
fi
if [[ -f "$STATUS_FILE" ]]; then
  python3 -m json.tool "$STATUS_FILE"
else
  echo "status file not found: $STATUS_FILE"
fi
