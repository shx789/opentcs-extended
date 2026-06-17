# SPDX-FileCopyrightText: The openTCS Authors
# SPDX-License-Identifier: MIT

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DockerDir = Resolve-Path (Join-Path $ScriptDir "..")

$ImageTag = if ($env:IMAGE_TAG) { $env:IMAGE_TAG } else { "1.0" }
$OutputFile = if ($env:OUTPUT_FILE) { $env:OUTPUT_FILE } else { Join-Path $DockerDir "opentcs-images.tar" }

Write-Host "Exporting images to $OutputFile ..."

docker save -o $OutputFile `
  "opentcs-kernel:$ImageTag" `
  "opentcs-rcs:$ImageTag" `
  "eclipse-mosquitto:2"

$sizeMb = [math]::Round((Get-Item $OutputFile).Length / 1MB, 1)
Write-Host "Exported ($sizeMb MB): $OutputFile"
