#!/usr/bin/env sh
# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT
#
# Patches Gradle wrapper for Docker builds (mirror + longer timeout), then runs gradlew.

set -eu

GRADLE_VERSION="${GRADLE_VERSION:-8.14.4}"
GRADLE_DISTRIBUTION_URL="${GRADLE_DISTRIBUTION_URL:-https://mirrors.cloud.tencent.com/gradle/gradle-${GRADLE_VERSION}-bin.zip}"
GRADLE_NETWORK_TIMEOUT="${GRADLE_NETWORK_TIMEOUT:-600000}"
GRADLE_USE_MAVEN_MIRROR="${GRADLE_USE_MAVEN_MIRROR:-true}"

WRAPPER_PROPS="gradle/wrapper/gradle-wrapper.properties"
INIT_SCRIPT="deploy/docker/gradle/init.gradle"

# Java properties file escapes colons in URLs.
case "${GRADLE_DISTRIBUTION_URL}" in
  https\\://*) DISTRIBUTION_URL="${GRADLE_DISTRIBUTION_URL}" ;;
  https://*) DISTRIBUTION_URL="$(printf '%s' "${GRADLE_DISTRIBUTION_URL}" | sed 's/:/\\:/g')" ;;
  *) DISTRIBUTION_URL="${GRADLE_DISTRIBUTION_URL}" ;;
esac

if [ ! -f "${WRAPPER_PROPS}" ]; then
  echo "ERROR: ${WRAPPER_PROPS} not found (run from repository root)" >&2
  exit 1
fi

sed -i "s|^distributionUrl=.*|distributionUrl=${DISTRIBUTION_URL}|" "${WRAPPER_PROPS}"
sed -i "s/^networkTimeout=.*/networkTimeout=${GRADLE_NETWORK_TIMEOUT}/" "${WRAPPER_PROPS}"
sed -i 's/^validateDistributionUrl=.*/validateDistributionUrl=false/' "${WRAPPER_PROPS}"

export GRADLE_USE_MAVEN_MIRROR

chmod +x gradlew
echo "Gradle distribution: ${GRADLE_DISTRIBUTION_URL}"
echo "Maven mirror: ${GRADLE_USE_MAVEN_MIRROR}"

exec ./gradlew --init-script "${INIT_SCRIPT}" --no-daemon "$@"
