param(
  [string]$Version = "1.0.16"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = Split-Path -Parent $Root
$ReleaseDir = Join-Path $Repo "release"
$Name = "agv-mqtt-tester"
$VersionSuffix = if ([string]::IsNullOrWhiteSpace($Version)) { "" } else { "-v$Version" }
$PackageName = "$Name$VersionSuffix"
$OfflinePackageName = "$Name-offline$VersionSuffix"
$Zip = Join-Path $ReleaseDir "$PackageName.zip"
$OfflineZip = Join-Path $ReleaseDir "$OfflinePackageName.zip"
$Stage = Join-Path $ReleaseDir "$PackageName-package-stage"
$OfflineStage = Join-Path $ReleaseDir "$OfflinePackageName-package-stage"

if (Test-Path $Stage) {
  Remove-Item -Recurse -Force $Stage
}
if (Test-Path $OfflineStage) {
  Remove-Item -Recurse -Force $OfflineStage
}
New-Item -ItemType Directory -Force $Stage | Out-Null
New-Item -ItemType Directory -Force $ReleaseDir | Out-Null

Get-ChildItem $Root -Force | Where-Object {
  $_.Name -notin @(".venv", ".pack-venv", "build", "dist", "logs", "__pycache__")
} | ForEach-Object {
  Copy-Item $_.FullName -Destination $Stage -Recurse -Force
}

if (Test-Path $Zip) {
  Remove-Item -Force $Zip
}
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $Zip
Write-Host "Created $Zip"

$Exe = Join-Path $Root "dist\agv-mqtt-tester.exe"
if (Test-Path $Exe) {
  New-Item -ItemType Directory -Force $OfflineStage | Out-Null
  New-Item -ItemType Directory -Force (Join-Path $OfflineStage "bin") | Out-Null
  Copy-Item $Exe -Destination (Join-Path $OfflineStage "bin\agv-mqtt-tester.exe") -Force
  Copy-Item (Join-Path $Root "config.example.json") -Destination $OfflineStage -Force
  Copy-Item (Join-Path $Root "start-exe.bat") -Destination $OfflineStage -Force
  Copy-Item (Join-Path $Root "README.md") -Destination $OfflineStage -Force
  Copy-Item (Join-Path $Root "README_OFFLINE.md") -Destination $OfflineStage -Force
  if (Test-Path $OfflineZip) {
    Remove-Item -Force $OfflineZip
  }
  Compress-Archive -Path (Join-Path $OfflineStage "*") -DestinationPath $OfflineZip
  Write-Host "Created $OfflineZip"
} else {
  Write-Host "Offline exe not found, skipped offline zip: $Exe"
}
