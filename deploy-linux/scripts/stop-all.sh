#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

stop_by_pidfile agv-dashboard
stop_by_pidfile agv-config-web
stop_by_pidfile opentcs-rcs
stop_by_pidfile opentcs-kernel
