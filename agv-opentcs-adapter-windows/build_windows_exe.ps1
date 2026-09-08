param(
  [string]$PythonExe = "python",
  [string]$OutputDir = ".\\release",
  [switch]$IncludeNoCarSimulator
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([System.IO.Path]::IsPathRooted($OutputDir)) {
  $OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
}
else {
  $OutputDir = [System.IO.Path]::GetFullPath((Join-Path $Root $OutputDir))
}
$BuildDir = Join-Path $Root "build"
$WorkDir = Join-Path $BuildDir "pyinstaller-work"
$SpecDir = Join-Path $BuildDir "pyinstaller-spec"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
New-Item -ItemType Directory -Force -Path $SpecDir | Out-Null

Push-Location $Root
try {
  & $PythonExe -m pip install -r .\requirements-build.txt

  & $PythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $OutputDir `
    --workpath $WorkDir `
    --specpath $SpecDir `
    --hidden-import amqtt.plugins.logging_amqtt `
    --name agv-native-feedback-adapter `
    .\bin\agv_native_feedback_adapter.py

  & $PythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $OutputDir `
    --workpath $WorkDir `
    --specpath $SpecDir `
    --name validate-runtime-config `
    .\bin\validate_runtime_config.py

  & $PythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $OutputDir `
    --workpath $WorkDir `
    --specpath $SpecDir `
    --name agv-config-web `
    .\bin\agv_config_web.py

  & $PythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $OutputDir `
    --workpath $WorkDir `
    --specpath $SpecDir `
    --name generate-map-topology `
    .\bin\generate_map_topology.py

  & $PythonExe -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $OutputDir `
    --workpath $WorkDir `
    --specpath $SpecDir `
    --hidden-import amqtt.plugins.authentication `
    --hidden-import amqtt.plugins.sys.broker `
    --name local-mqtt-broker `
    .\bin\local_mqtt_broker.py

  if ($IncludeNoCarSimulator) {
    & $PythonExe -m PyInstaller `
      --noconfirm `
      --clean `
      --onefile `
      --distpath $OutputDir `
      --workpath $WorkDir `
      --specpath $SpecDir `
      --hidden-import amqtt.plugins.logging_amqtt `
      --name agv-no-car-feedback-simulator `
      .\bin\agv_no_car_feedback_simulator.py
  }

  Copy-Item .\config -Destination $OutputDir -Recurse -Force
  Copy-Item .\docs -Destination $OutputDir -Recurse -Force
  Copy-Item .\web -Destination $OutputDir -Recurse -Force
  Copy-Item .\README.md -Destination $OutputDir -Force
  Copy-Item .\requirements.txt -Destination $OutputDir -Force
  Copy-Item .\start_adapter.bat -Destination $OutputDir -Force
  Copy-Item .\start_config_web.bat -Destination $OutputDir -Force
  Copy-Item .\status_adapter.bat -Destination $OutputDir -Force
  Copy-Item .\status_config_web.bat -Destination $OutputDir -Force
  Copy-Item .\stop_adapter.ps1 -Destination $OutputDir -Force
  Copy-Item .\stop_config_web.ps1 -Destination $OutputDir -Force
  Copy-Item .\install_service_nssm.ps1 -Destination $OutputDir -Force
  Copy-Item .\uninstall_service_nssm.ps1 -Destination $OutputDir -Force

  if ($IncludeNoCarSimulator) {
    Copy-Item .\start_no_car_sim.bat -Destination $OutputDir -Force
    Copy-Item .\status_no_car_sim.bat -Destination $OutputDir -Force
    Copy-Item .\stop_no_car_sim.ps1 -Destination $OutputDir -Force
  }

  New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir "logs") | Out-Null
  New-Item -ItemType Directory -Force -Path (Join-Path $OutputDir "tools") | Out-Null

  Write-Host "build output: $OutputDir"
}
finally {
  Pop-Location
}
