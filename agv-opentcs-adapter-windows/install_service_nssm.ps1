param(
  [string]$PythonExe = "python",
  [string]$NssmExe = ".\\tools\\nssm.exe",
  [switch]$UseExe,
  [switch]$InstallConfigWeb,
  [switch]$InstallNoCarSimulator
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$AdapterScript = Join-Path $Root "bin\\agv_native_feedback_adapter.py"
$SimulatorScript = Join-Path $Root "bin\\agv_no_car_feedback_simulator.py"
$ConfigWebScript = Join-Path $Root "bin\\agv_config_web.py"
$AdapterExe = Join-Path $Root "agv-native-feedback-adapter.exe"
$SimulatorExe = Join-Path $Root "agv-no-car-feedback-simulator.exe"
$ConfigWebExe = Join-Path $Root "agv-config-web.exe"
$LogsDir = Join-Path $Root "logs"
New-Item -ItemType Directory -Force -Path $LogsDir | Out-Null

if ($UseExe) {
  & $NssmExe install AgvOpenTcsAdapter $AdapterExe
}
else {
  & $NssmExe install AgvOpenTcsAdapter $PythonExe "-u `"$AdapterScript`""
}
& $NssmExe set AgvOpenTcsAdapter AppDirectory $Root
& $NssmExe set AgvOpenTcsAdapter AppStdout (Join-Path $LogsDir "agv_native_feedback_adapter.log")
& $NssmExe set AgvOpenTcsAdapter AppStderr (Join-Path $LogsDir "agv_native_feedback_adapter.log")
& $NssmExe set AgvOpenTcsAdapter Start SERVICE_AUTO_START

if ($InstallNoCarSimulator) {
  if ($UseExe) {
    & $NssmExe install AgvNoCarSimulator $SimulatorExe
  }
  else {
    & $NssmExe install AgvNoCarSimulator $PythonExe "-u `"$SimulatorScript`""
  }
  & $NssmExe set AgvNoCarSimulator AppDirectory $Root
  & $NssmExe set AgvNoCarSimulator AppStdout (Join-Path $LogsDir "agv_no_car_feedback_simulator.log")
  & $NssmExe set AgvNoCarSimulator AppStderr (Join-Path $LogsDir "agv_no_car_feedback_simulator.log")
  & $NssmExe set AgvNoCarSimulator Start SERVICE_AUTO_START
}

if ($InstallConfigWeb) {
  if ($UseExe) {
    & $NssmExe install AgvConfigWeb $ConfigWebExe
  }
  else {
    & $NssmExe install AgvConfigWeb $PythonExe "-u `"$ConfigWebScript`""
  }
  & $NssmExe set AgvConfigWeb AppDirectory $Root
  & $NssmExe set AgvConfigWeb AppStdout (Join-Path $LogsDir "agv_config_web.log")
  & $NssmExe set AgvConfigWeb AppStderr (Join-Path $LogsDir "agv_config_web.log")
  & $NssmExe set AgvConfigWeb Start SERVICE_AUTO_START
}

Write-Host "services installed"
