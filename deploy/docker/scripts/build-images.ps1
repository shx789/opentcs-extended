# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

param(
  [switch]$HostGradle
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..")
$DockerDir = Join-Path $RepoRoot "deploy\docker"

$ImageTag = if ($env:IMAGE_TAG) { $env:IMAGE_TAG } else { "1.0" }
$Platform = if ($env:DOCKER_PLATFORM) { $env:DOCKER_PLATFORM } else { "linux/amd64" }
$JdkImage = if ($env:JDK_IMAGE) { $env:JDK_IMAGE } else { "eclipse-temurin:21-jdk-jammy" }
$JreImage = if ($env:JRE_IMAGE) { $env:JRE_IMAGE } else { "eclipse-temurin:21-jre-jammy" }
$GradleDistributionUrl = if ($env:GRADLE_DISTRIBUTION_URL) {
  $env:GRADLE_DISTRIBUTION_URL
} else {
  "https://mirrors.cloud.tencent.com/gradle/gradle-8.14.4-bin.zip"
}

$BuildArgs = @(
  "--build-arg", "JDK_IMAGE=$JdkImage",
  "--build-arg", "JRE_IMAGE=$JreImage",
  "--build-arg", "GRADLE_DISTRIBUTION_URL=$GradleDistributionUrl"
)

if ($HostGradle) {
  Write-Host "Host Gradle mode: building artifacts on host, then packaging into images..."
  Push-Location $RepoRoot
  try {
    & .\gradlew.bat :opentcs-kernel:installDist :opentcs-rcs-integration-sample:shadowJar -x test --no-daemon
  } finally {
    Pop-Location
  }
  & (Join-Path $ScriptDir "stage-host-artifacts.ps1")
  $KernelDockerfile = Join-Path $DockerDir "Dockerfile.kernel.host"
  $RcsDockerfile = Join-Path $DockerDir "Dockerfile.rcs.host"
  $KernelBuildArgs = @("--build-arg", "JRE_IMAGE=$JreImage")
  $RcsBuildArgs = @("--build-arg", "JRE_IMAGE=$JreImage")
} else {
  Write-Host "In-container Gradle mode (mirror: $GradleDistributionUrl)..."
  $KernelDockerfile = Join-Path $DockerDir "Dockerfile.kernel"
  $RcsDockerfile = Join-Path $DockerDir "Dockerfile.rcs"
  $KernelBuildArgs = $BuildArgs
  $RcsBuildArgs = $BuildArgs
}

Write-Host "Building images for platform $Platform (tag: $ImageTag)..."
Write-Host "JDK base: $JdkImage"
Write-Host "JRE base: $JreImage"

docker buildx build `
  --platform $Platform `
  --load `
  @KernelBuildArgs `
  -f $KernelDockerfile `
  -t "opentcs-kernel:$ImageTag" `
  $RepoRoot

docker buildx build `
  --platform $Platform `
  --load `
  @RcsBuildArgs `
  -f $RcsDockerfile `
  -t "opentcs-rcs:$ImageTag" `
  $RepoRoot

Write-Host "Pulling eclipse-mosquitto:2..."
docker pull eclipse-mosquitto:2

Write-Host "Done. Images:"
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | Select-String -Pattern "opentcs|mosquitto"
