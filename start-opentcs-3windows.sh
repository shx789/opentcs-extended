#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_JAVA_HOME="$HOME/.local/jdk/jdk-21.0.10+7"
JAVA_HOME="${JAVA_HOME:-$DEFAULT_JAVA_HOME}"

if [[ ! -x "$JAVA_HOME/bin/java" ]]; then
  echo "ERROR: Java not found at: $JAVA_HOME"
  echo "Set JAVA_HOME first, e.g.:"
  echo "  export JAVA_HOME=$DEFAULT_JAVA_HOME"
  exit 1
fi

if ! command -v gnome-terminal >/dev/null 2>&1; then
  echo "ERROR: gnome-terminal not found."
  exit 1
fi

launch_if_not_running() {
  local process_pattern="$1"
  local title="$2"
  local gradle_task="$3"

  if pgrep -f "$process_pattern" >/dev/null 2>&1; then
    echo "$title is already running. Skip launching."
    return
  fi

  gnome-terminal \
    --title="$title" \
    -- bash -lc "cd '$PROJECT_DIR'; export JAVA_HOME='$JAVA_HOME'; export PATH=\"\$JAVA_HOME/bin:\$PATH\"; ./gradlew $gradle_task; exec bash"
}

launch_if_not_running "org\\.opentcs\\.kernel\\.RunKernel" "openTCS Kernel" ":opentcs-kernel:run"
sleep 2
launch_if_not_running "org\\.opentcs\\.kernelcontrolcenter\\.RunKernelControlCenter" "openTCS KernelControlCenter" ":opentcs-kernelcontrolcenter:run"
sleep 2
launch_if_not_running "org\\.opentcs\\.operationsdesk\\.RunOperationsDesk" "openTCS OperationsDesk" ":opentcs-operationsdesk:run"
sleep 2
launch_if_not_running "org\\.opentcs\\.modeleditor\\.RunModelEditor" "openTCS ModelEditor" ":opentcs-modeleditor:run"

echo "Launch requests submitted."
