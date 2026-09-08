#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

export JAVA_TOOL_OPTIONS="${JAVA_TOOL_OPTIONS:-} -Dopentcs.mqtt.brokerUri=$AGV_MQTT_URI -Dopentcs.mqtt.commandTopic=$AGV_COMMAND_TOPIC -Dopentcs.mqtt.feedbackTopic=$AGV_FEEDBACK_TOPIC -Dopentcs.mqtt.pointIdMap=$AGV_POINT_ID_MAP -Dopentcs.mqtt.agvId=$AGV_ID"

if [[ -n "${OPENTCS_KERNEL_CMD:-}" ]]; then
  run_shell_or_start opentcs-kernel "$OPENTCS_KERNEL_CMD"
elif [[ -x "$OPENTCS_HOME/bin/startKernel.sh" ]]; then
  run_or_start opentcs-kernel "$OPENTCS_HOME/bin/startKernel.sh"
elif [[ -x "$PROJECT_HOME/gradlew" ]]; then
  run_or_start opentcs-kernel "$PROJECT_HOME/gradlew" -p "$PROJECT_HOME" :opentcs-kernel:run
else
  echo "Cannot find openTCS kernel start command. Set OPENTCS_KERNEL_CMD in config/agv-suite.env." >&2
  exit 1
fi
