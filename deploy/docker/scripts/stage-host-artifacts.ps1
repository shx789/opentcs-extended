# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT
#
# Stages host Gradle outputs into deploy/docker/dist/ for Dockerfile.*.host builds.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..")
$DistDir = Join-Path $RepoRoot "deploy\docker\dist"
$KernelSrc = Join-Path $RepoRoot "opentcs-kernel\build\install\opentcs-kernel"
$RcsSrc = Join-Path $RepoRoot "opentcs-rcs-integration-sample\build\libs"

if (-not (Test-Path $KernelSrc)) {
  throw "Kernel installDist output not found: $KernelSrc`nRun: .\gradlew.bat :opentcs-kernel:installDist -x test"
}

$RcsJar = Get-ChildItem -Path $RcsSrc -Filter "*-all.jar" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $RcsJar) {
  throw "RCS shadow jar not found under: $RcsSrc`nRun: .\gradlew.bat :opentcs-rcs-integration-sample:shadowJar -x test"
}

if (Test-Path $DistDir) {
  Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force -Path (Join-Path $DistDir "kernel"), (Join-Path $DistDir "rcs") | Out-Null

Copy-Item -Path (Join-Path $KernelSrc "*") -Destination (Join-Path $DistDir "kernel") -Recurse -Force

$KeystorePath = Join-Path $DistDir "kernel\config\keystore.p12"
if (-not (Test-Path $KeystorePath)) {
  Write-Host "Generating Kernel SSL keystore/truststore..."
  Push-Location (Join-Path $DistDir "kernel")
  try {
    if (Test-Path ".\generateKeystores.bat") {
      cmd /c generateKeystores.bat
    } else {
      bash ./generateKeystores.sh
    }
  } finally {
    Pop-Location
  }
}

Copy-Item -Path $RcsJar.FullName -Destination (Join-Path $DistDir "rcs\app.jar") -Force

Write-Host "Staged host build artifacts:"
Write-Host "  $DistDir\kernel\"
Write-Host "  $DistDir\rcs\app.jar"
