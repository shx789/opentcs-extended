#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

echo "== AGV openTCS Linux environment check =="
echo "deploy dir: $DEPLOY_DIR"
echo "env file: ${ENV_FILE}"
echo

check_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "OK   command: $cmd -> $(command -v "$cmd")"
  else
    echo "MISS command: $cmd"
  fi
}

check_dir() {
  local dir="$1"
  local label="$2"
  if [[ -d "$dir" ]]; then
    echo "OK   $label: $dir"
  else
    echo "MISS $label: $dir"
  fi
}

check_http() {
  local url="$1"
  local label="$2"
  if command -v curl >/dev/null 2>&1; then
    if curl -fsS --max-time 3 "$url" >/dev/null 2>&1; then
      echo "OK   $label: $url"
    else
      echo "WARN $label not reachable now: $url"
    fi
  else
    echo "SKIP $label http check, curl not installed"
  fi
}

check_cmd "$JAVA_BIN"
check_cmd "$PYTHON_BIN"
check_cmd curl
check_cmd nc
check_dir "$PROJECT_HOME" PROJECT_HOME
check_dir "$ADAPTER_ROOT" ADAPTER_ROOT
check_dir "$DASHBOARD_ROOT" DASHBOARD_ROOT
check_dir "$OPENTCS_HOME" OPENTCS_HOME
echo

if command -v nc >/dev/null 2>&1; then
  if nc -vz -w 3 "$AGV_MQTT_HOST" "$AGV_MQTT_PORT" >/dev/null 2>&1; then
    echo "OK   MQTT reachable: $AGV_MQTT_HOST:$AGV_MQTT_PORT"
  else
    echo "WARN MQTT not reachable now: $AGV_MQTT_HOST:$AGV_MQTT_PORT"
  fi
else
  echo "SKIP MQTT port check, nc not installed"
fi

check_http "$OPENTCS_HTTP_URL/v1/kernel/version" openTCS
check_http "$RCS_HTTP_URL/api/v1/health" RCS
