#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

IMAGE_TAG="${IMAGE_TAG:-1.0}"
OUTPUT_FILE="${OUTPUT_FILE:-${DOCKER_DIR}/opentcs-images.tar}"

echo "Exporting images to ${OUTPUT_FILE} ..."

docker save -o "${OUTPUT_FILE}" \
  "opentcs-kernel:${IMAGE_TAG}" \
  "opentcs-rcs:${IMAGE_TAG}" \
  "eclipse-mosquitto:2"

ls -lh "${OUTPUT_FILE}"
