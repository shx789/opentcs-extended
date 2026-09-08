#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

OPENTCS_CONTAINERS=(opentcs-mqtt opentcs-kernel opentcs-rcs)

remove_stale_containers() {
  echo "Removing stale opentcs containers (if any) ..."
  docker rm -f "${OPENTCS_CONTAINERS[@]}" >/dev/null 2>&1 || true
}

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

resolve_tar_file() {
  if [[ -n "${1:-}" ]]; then
    echo "$1"
    return
  fi
  local candidate
  for candidate in \
    "${DOCKER_DIR}/opentcs-images.tar" \
    "${DOCKER_DIR}/../opentcs-images.tar" \
    "/root/services/opentcs/docker/opentcs-images.tar" \
    "/opt/opentcs/opentcs-images.tar"; do
    if [[ -f "${candidate}" ]]; then
      echo "${candidate}"
      return
    fi
  done
  echo ""
}

TAR_FILE="$(resolve_tar_file "${1:-}")"

if [[ -z "${TAR_FILE}" || ! -f "${TAR_FILE}" ]]; then
  echo "ERROR: Image tar not found." >&2
  echo "Usage: bash $0 [path/to/opentcs-images.tar]" >&2
  echo "Searched:" >&2
  echo "  ${DOCKER_DIR}/opentcs-images.tar" >&2
  echo "  ${DOCKER_DIR}/../opentcs-images.tar" >&2
  echo "  /root/services/opentcs/docker/opentcs-images.tar" >&2
  echo "  /opt/opentcs/opentcs-images.tar" >&2
  exit 1
fi

echo "Loading images from ${TAR_FILE} ..."
docker load -i "${TAR_FILE}"

cd "${DOCKER_DIR}"
ensure_env_file "${DOCKER_DIR}"
remove_stale_containers

echo "Starting containers (via start-containers.sh) ..."
bash "${SCRIPT_DIR}/start-containers.sh"

echo "Waiting for services..."
sleep 35

if curl -fsS "http://localhost:55200/v1/transportOrders" > /dev/null; then
  echo "Kernel: OK"
else
  echo "Kernel: not ready yet (check: docker logs opentcs-kernel)"
fi

if curl -fsS "http://localhost:8090/api/v1/wcs/agv/missions" > /dev/null; then
  echo "RCS: OK"
else
  echo "RCS: not ready yet (check: docker logs opentcs-rcs)"
fi

echo "Done. Demo: http://<host>:8090/demo/wcs"
