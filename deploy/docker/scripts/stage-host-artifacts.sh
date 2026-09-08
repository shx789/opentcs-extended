#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT
#
# Stages host Gradle outputs into deploy/docker/dist/ for Dockerfile.*.host builds.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DIST_DIR="${REPO_ROOT}/deploy/docker/dist"
KERNEL_SRC="${REPO_ROOT}/opentcs-kernel/build/install/opentcs-kernel"
RCS_SRC="${REPO_ROOT}/opentcs-rcs-integration-sample/build/libs"

if [[ ! -d "${KERNEL_SRC}" ]]; then
  echo "ERROR: Kernel installDist output not found: ${KERNEL_SRC}" >&2
  echo "Run: ./gradlew :opentcs-kernel:installDist -x test" >&2
  exit 1
fi

RCS_JAR="$(find "${RCS_SRC}" -maxdepth 1 -name '*-all.jar' -print -quit 2>/dev/null || true)"
if [[ -z "${RCS_JAR}" || ! -f "${RCS_JAR}" ]]; then
  echo "ERROR: RCS shadow jar not found under: ${RCS_SRC}" >&2
  echo "Run: ./gradlew :opentcs-rcs-integration-sample:shadowJar -x test" >&2
  exit 1
fi

rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}/kernel" "${DIST_DIR}/rcs"
cp -a "${KERNEL_SRC}/." "${DIST_DIR}/kernel/"

if [[ ! -f "${DIST_DIR}/kernel/config/keystore.p12" ]]; then
  echo "Generating Kernel SSL keystore/truststore..."
  (cd "${DIST_DIR}/kernel" && chmod +x generateKeystores.sh && ./generateKeystores.sh)
fi

cp "${RCS_JAR}" "${DIST_DIR}/rcs/app.jar"

echo "Staged host build artifacts:"
echo "  ${DIST_DIR}/kernel/"
echo "  ${DIST_DIR}/rcs/app.jar"
