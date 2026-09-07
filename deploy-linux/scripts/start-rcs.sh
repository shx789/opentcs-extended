#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

RCS_JAR="${RCS_JAR:-$PROJECT_HOME/opentcs-rcs-integration-sample/build/libs/opentcs-rcs-integration-sample-7.3.0-SNAPSHOT-all.jar}"

if [[ -n "${RCS_CMD:-}" ]]; then
  run_shell_or_start opentcs-rcs "$RCS_CMD"
elif [[ -f "$RCS_JAR" ]]; then
  run_or_start opentcs-rcs "$JAVA_BIN" \
    "-Drcs.openTcs.baseUrl=$OPENTCS_HTTP_URL" \
    "-Dserver.host=$RCS_HTTP_HOST" \
    "-Dserver.port=$RCS_HTTP_PORT" \
    -jar "$RCS_JAR"
elif [[ -x "$PROJECT_HOME/gradlew" ]]; then
  run_or_start opentcs-rcs "$PROJECT_HOME/gradlew" -p "$PROJECT_HOME" :opentcs-rcs-integration-sample:runRcsSample -PrcsPort="$RCS_HTTP_PORT" "-Drcs.openTcs.baseUrl=$OPENTCS_HTTP_URL"
else
  echo "Cannot find RCS jar or Gradle wrapper. Set RCS_CMD in config/agv-suite.env." >&2
  exit 1
fi
