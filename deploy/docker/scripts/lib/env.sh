#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

# Ensures deploy/docker/.env exists and uses Unix (LF) line endings.
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
