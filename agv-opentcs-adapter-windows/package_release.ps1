param(
  [string]$ReleaseDir = ".\\release",
  [string]$ZipName = "agv-opentcs-adapter-windows-release.zip"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$ReleaseDir = [System.IO.Path]::GetFullPath((Join-Path $Root $ReleaseDir))
$ZipPath = Join-Path $Root $ZipName

if (!(Test-Path $ReleaseDir)) {
  throw "release dir not found: $ReleaseDir"
}

if (Test-Path $ZipPath) {
  Remove-Item $ZipPath -Force
}

Compress-Archive -Path (Join-Path $ReleaseDir "*") -DestinationPath $ZipPath -Force
Write-Host "package created: $ZipPath"
