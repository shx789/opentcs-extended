#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

require_dir "$ADAPTER_ROOT" ADAPTER_ROOT

VENV_DIR="${VENV_DIR:-$AGV_SUITE_HOME/venv}"
log "creating python venv: $VENV_DIR"
"$PYTHON_BIN" -m venv "$VENV_DIR"

# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"
python -m pip install --upgrade pip

if [[ -f "$ADAPTER_ROOT/requirements.txt" ]]; then
  python -m pip install -r "$ADAPTER_ROOT/requirements.txt"
fi

log "python env ready: $VENV_DIR"
