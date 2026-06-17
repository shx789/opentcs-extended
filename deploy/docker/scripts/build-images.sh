#!/usr/bin/env bash
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

set -euo pipefail

HOST_GRADLE=false
for arg in "$@"; do
  case "${arg}" in
    --host-gradle) HOST_GRADLE=true ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DOCKER_DIR="${REPO_ROOT}/deploy/docker"

IMAGE_TAG="${IMAGE_TAG:-1.0}"
DOCKER_PLATFORM="${DOCKER_PLATFORM:-linux/amd64}"
JDK_IMAGE="${JDK_IMAGE:-eclipse-temurin:21-jdk-jammy}"
JRE_IMAGE="${JRE_IMAGE:-eclipse-temurin:21-jre-jammy}"
GRADLE_DISTRIBUTION_URL="${GRADLE_DISTRIBUTION_URL:-https://mirrors.cloud.tencent.com/gradle/gradle-8.14.4-bin.zip}"

if [[ "${HOST_GRADLE}" == "true" ]]; then
  echo "Host Gradle mode: building artifacts on host, then packaging into images..."
  (cd "${REPO_ROOT}" && ./gradlew :opentcs-kernel:installDist :opentcs-rcs-integration-sample:shadowJar -x test --no-daemon)
  bash "${SCRIPT_DIR}/stage-host-artifacts.sh"
  KERNEL_DOCKERFILE="${DOCKER_DIR}/Dockerfile.kernel.host"
  RCS_DOCKERFILE="${DOCKER_DIR}/Dockerfile.rcs.host"
  KERNEL_BUILD_ARGS=(--build-arg "JRE_IMAGE=${JRE_IMAGE}")
  RCS_BUILD_ARGS=(--build-arg "JRE_IMAGE=${JRE_IMAGE}")
else
  echo "In-container Gradle mode (mirror: ${GRADLE_DISTRIBUTION_URL})..."
  KERNEL_DOCKERFILE="${DOCKER_DIR}/Dockerfile.kernel"
  RCS_DOCKERFILE="${DOCKER_DIR}/Dockerfile.rcs"
  KERNEL_BUILD_ARGS=(
    --build-arg "JDK_IMAGE=${JDK_IMAGE}"
    --build-arg "JRE_IMAGE=${JRE_IMAGE}"
    --build-arg "GRADLE_DISTRIBUTION_URL=${GRADLE_DISTRIBUTION_URL}"
  )
  RCS_BUILD_ARGS=("${KERNEL_BUILD_ARGS[@]}")
fi

echo "Building images for platform ${DOCKER_PLATFORM} (tag: ${IMAGE_TAG})..."
echo "JDK base: ${JDK_IMAGE}"
echo "JRE base: ${JRE_IMAGE}"

docker buildx build \
  --platform "${DOCKER_PLATFORM}" \
  --load \
  "${KERNEL_BUILD_ARGS[@]}" \
  -f "${KERNEL_DOCKERFILE}" \
  -t "opentcs-kernel:${IMAGE_TAG}" \
  "${REPO_ROOT}"

docker buildx build \
  --platform "${DOCKER_PLATFORM}" \
  --load \
  "${RCS_BUILD_ARGS[@]}" \
  -f "${RCS_DOCKERFILE}" \
  -t "opentcs-rcs:${IMAGE_TAG}" \
  "${REPO_ROOT}"

echo "Pulling eclipse-mosquitto:2..."
docker pull eclipse-mosquitto:2

echo "Done. Images:"
docker images | grep -E 'opentcs|mosquitto' || true
