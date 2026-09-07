#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ENV_FILE="${AGV_SUITE_ENV:-$DEPLOY_DIR/config/agv-suite.env}"
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
else
  # shellcheck disable=SC1091
  source "$DEPLOY_DIR/config/agv-suite.env.example"
fi

LOG_DIR="${LOG_DIR:-$DEPLOY_DIR/logs}"
PID_DIR="$LOG_DIR/pids"
mkdir -p "$LOG_DIR" "$PID_DIR"

timestamp() {
  date +"%Y-%m-%d %H:%M:%S"
}

log() {
  echo "[$(timestamp)] $*"
}

pid_file() {
  echo "$PID_DIR/$1.pid"
}

is_running() {
  local pid="$1"
  [[ -n "$pid" ]] && kill -0 "$pid" >/dev/null 2>&1
}

start_background() {
  local name="$1"
  shift
  local pf
  pf="$(pid_file "$name")"
  if [[ -f "$pf" ]]; then
    local old_pid
    old_pid="$(cat "$pf" || true)"
    if is_running "$old_pid"; then
      log "$name already running, pid=$old_pid"
      return 0
    fi
  fi
  log "starting $name"
  nohup "$@" >"$LOG_DIR/$name.out.log" 2>"$LOG_DIR/$name.err.log" &
  echo "$!" >"$pf"
  log "$name pid=$(cat "$pf")"
}

run_or_start() {
  local name="$1"
  shift
  if [[ "${AGV_FOREGROUND:-0}" == "1" ]]; then
    log "running $name in foreground"
    exec "$@"
  fi
  start_background "$name" "$@"
}

run_shell_or_start() {
  local name="$1"
  local cmd="$2"
  if [[ "${AGV_FOREGROUND:-0}" == "1" ]]; then
    log "running $name in foreground: $cmd"
    exec bash -lc "$cmd"
  fi
  start_background "$name" bash -lc "$cmd"
}

stop_by_pidfile() {
  local name="$1"
  local pf
  pf="$(pid_file "$name")"
  if [[ ! -f "$pf" ]]; then
    log "$name pid file not found"
    return 0
  fi
  local pid
  pid="$(cat "$pf" || true)"
  if ! is_running "$pid"; then
    log "$name is not running"
    rm -f "$pf"
    return 0
  fi
  log "stopping $name pid=$pid"
  kill "$pid" || true
  for _ in {1..20}; do
    if ! is_running "$pid"; then
      rm -f "$pf"
      log "$name stopped"
      return 0
    fi
    sleep 0.5
  done
  log "$name did not stop gracefully, sending SIGKILL"
  kill -9 "$pid" || true
  rm -f "$pf"
}

require_dir() {
  local dir="$1"
  local label="$2"
  if [[ ! -d "$dir" ]]; then
    echo "Missing $label directory: $dir" >&2
    return 1
  fi
}
