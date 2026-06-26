#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT
#
# Starts containers with plain "docker run" (no compose plugin required).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

ensure_env_file() {
  local docker_dir="$1"
  local env_file="${docker_dir}/.env"
  local example_file="${docker_dir}/.env.example"

  if [[ ! -f "${env_file}" && -f "${example_file}" ]]; then
    sed 's/\r$//' "${example_file}" > "${env_file}"
    echo "Created .env from .env.example"
  elif [[ -f "${env_file}" ]]; then
    local tmp="${env_file}.unix"
    sed 's/\r$//' "${env_file}" > "${tmp}"
    mv "${tmp}" "${env_file}"
  fi
}

load_env_file() {
  local env_file="$1"
  if [[ ! -f "${env_file}" ]]; then
    return 0
  fi
  set -a
  # shellcheck disable=SC1090
  source <(sed 's/\r$//' "${env_file}")
  set +a
}

cd "${DOCKER_DIR}"
ensure_env_file "${DOCKER_DIR}"
load_env_file "${DOCKER_DIR}/.env"

IMAGE_TAG="${IMAGE_TAG:-1.0}"
NETWORK_NAME="${NETWORK_NAME:-opentcs-net}"
MQTT_HOST_PORT="${MQTT_HOST_PORT:-1883}"
KERNEL_HOST_PORT="${KERNEL_HOST_PORT:-55200}"
RCS_HOST_PORT="${RCS_HOST_PORT:-8090}"
SKIP_MQTT="${SKIP_MQTT:-false}"
EXTERNAL_MQTT_HOST="${EXTERNAL_MQTT_HOST:-172.17.0.1}"
RCS_AGV_COMMAND_TOPIC="${RCS_AGV_COMMAND_TOPIC:-robot_control}"
RCS_AGV_MQTT_TOPIC="${RCS_AGV_MQTT_TOPIC:-robot_status}"
RCS_AGV_POINT_ID_MAP="${RCS_AGV_POINT_ID_MAP:-Point-0020=20,Point-0026=26}"

# Some cloud ECS hosts block pthread in containers unless seccomp is relaxed.
DOCKER_RUN_OPTS=()
if [[ "${DOCKER_SECCOMP_UNCONFINED:-true}" == "true" ]]; then
  DOCKER_RUN_OPTS+=(--security-opt seccomp=unconfined)
fi
DOCKER_RUN_OPTS+=(--ulimit nproc=65535:65535)

is_port_in_use() {
  local port="$1"
  if command -v ss >/dev/null 2>&1; then
    ss -tln | grep -q ":${port} "
    return $?
  fi
  if command -v netstat >/dev/null 2>&1; then
    netstat -tln | grep -q ":${port} "
    return $?
  fi
  return 1
}

docker network inspect "${NETWORK_NAME}" >/dev/null 2>&1 || docker network create "${NETWORK_NAME}"

echo "Removing stale opentcs containers (if any) ..."
docker rm -f opentcs-mqtt opentcs-kernel opentcs-rcs >/dev/null 2>&1 || true

if [[ "${RESET_KERNEL_VOLUMES:-false}" == "true" ]]; then
  echo "RESET_KERNEL_VOLUMES=true — removing opentcs-kernel-data"
  docker volume rm opentcs-kernel-data >/dev/null 2>&1 || true
fi

docker volume create opentcs-kernel-data >/dev/null 2>&1 || true
docker volume create opentcs-rcs-data >/dev/null 2>&1 || true

MQTT_BROKER_HOST="opentcs-mqtt"

if [[ "${SKIP_MQTT}" == "true" ]]; then
  echo "SKIP_MQTT=true — using existing broker at ${EXTERNAL_MQTT_HOST}:${MQTT_HOST_PORT}"
  MQTT_BROKER_HOST="${EXTERNAL_MQTT_HOST}"
else
  if is_port_in_use "${MQTT_HOST_PORT}"; then
    echo "ERROR: host port ${MQTT_HOST_PORT} is already in use." >&2
    echo "Options:" >&2
    echo "  1) Stop the process/container using port ${MQTT_HOST_PORT}" >&2
    echo "  2) Use another host port: MQTT_HOST_PORT=1884 bash start-containers.sh" >&2
    echo "  3) Reuse host MQTT broker: SKIP_MQTT=true bash start-containers.sh" >&2
    echo "Check: ss -tlnp | grep ${MQTT_HOST_PORT}  OR  docker ps -a" >&2
    exit 1
  fi
  echo "Starting MQTT (host ${MQTT_HOST_PORT} -> container 1883)..."
  docker run -d \
    --name opentcs-mqtt \
    --network "${NETWORK_NAME}" \
    -p "${MQTT_HOST_PORT}:1883" \
    -v "${DOCKER_DIR}/mosquitto.conf:/mosquitto/config/mosquitto.conf:ro" \
    --restart unless-stopped \
    eclipse-mosquitto:2
fi

if is_port_in_use "${KERNEL_HOST_PORT}"; then
  echo "ERROR: host port ${KERNEL_HOST_PORT} is already in use." >&2
  exit 1
fi

KERNEL_MODEL_FILE="${KERNEL_MODEL_FILE:-}"

KERNEL_VOLUME_MOUNTS=(
  -v opentcs-kernel-data:/opt/opentcs-kernel/data
)

if [[ -n "${KERNEL_MODEL_FILE}" ]]; then
  if [[ ! -f "${KERNEL_MODEL_FILE}" ]]; then
    echo "ERROR: KERNEL_MODEL_FILE not found: ${KERNEL_MODEL_FILE}" >&2
    exit 1
  fi
  KERNEL_VOLUME_MOUNTS+=(
    -v "${KERNEL_MODEL_FILE}:/opt/opentcs-kernel/data/model.xml:ro"
  )
  echo "Kernel plant model: ${KERNEL_MODEL_FILE} -> /opt/opentcs-kernel/data/model.xml"
fi

echo "Starting openTCS Kernel (host ${KERNEL_HOST_PORT})..."
docker run -d \
  "${DOCKER_RUN_OPTS[@]}" \
  --name opentcs-kernel \
  --network "${NETWORK_NAME}" \
  -p "${KERNEL_HOST_PORT}:55200" \
  "${KERNEL_VOLUME_MOUNTS[@]}" \
  -e "JAVA_TOOL_OPTIONS=${JAVA_TOOL_OPTIONS:--Xmx512m}" \
  --restart unless-stopped \
  --entrypoint sh \
  "opentcs-kernel:${IMAGE_TAG}" \
  -c 'set -ex
export PATH=/opt/java/openjdk/bin:$PATH
cd /opt/opentcs-kernel
if [ ! -f config/keystore.p12 ] || [ ! -f config/truststore.p12 ]; then
  chmod +x ./generateKeystores.sh ./startKernel.sh
  ./generateKeystores.sh
fi
test -f config/logging.config
test -f config/keystore.p12
exec ./startKernel.sh'

echo "Waiting for Kernel (30s)..."
sleep 30

if is_port_in_use "${RCS_HOST_PORT}"; then
  echo "ERROR: host port ${RCS_HOST_PORT} is already in use." >&2
  exit 1
fi

RCS_ENV=(
  -e "RCS_OPENTCS_BASE_URL=http://opentcs-kernel:55200"
  -e "RCS_OPENTCS_SSE_ENABLED=true"
  -e "RCS_STORE_MODE=file"
  -e "RCS_STORE_FILE_DIR=/data/rcs-store"
  -e "RCS_AGV_COMMAND_ENABLED=true"
  -e "RCS_AGV_COMMAND_BROKER_URI=tcp://${MQTT_BROKER_HOST}:1883"
  -e "RCS_AGV_COMMAND_TOPIC=${RCS_AGV_COMMAND_TOPIC}"
  -e "RCS_AGV_POINT_ID_MAP=${RCS_AGV_POINT_ID_MAP}"
  -e "RCS_AGV_MQTT_ENABLED=true"
  -e "RCS_AGV_MQTT_BROKER_URI=tcp://${MQTT_BROKER_HOST}:1883"
  -e "RCS_AGV_MQTT_TOPIC=${RCS_AGV_MQTT_TOPIC}"
)

if [[ -n "${RCS_WMS_BASE_URL:-}" ]]; then
  RCS_ENV+=(-e "RCS_WMS_BASE_URL=${RCS_WMS_BASE_URL}")
fi
if [[ -n "${RCS_CALLBACK_BASE_URL:-}" ]]; then
  RCS_ENV+=(-e "RCS_CALLBACK_BASE_URL=${RCS_CALLBACK_BASE_URL}")
fi

echo "Starting RCS (host ${RCS_HOST_PORT})..."
docker run -d \
  "${DOCKER_RUN_OPTS[@]}" \
  --name opentcs-rcs \
  --network "${NETWORK_NAME}" \
  -p "${RCS_HOST_PORT}:8090" \
  "${RCS_ENV[@]}" \
  -v opentcs-rcs-data:/data/rcs-store \
  --restart unless-stopped \
  "opentcs-rcs:${IMAGE_TAG}"

echo "Containers started."
docker ps --filter "name=opentcs-"
