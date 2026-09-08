$ErrorActionPreference = "Stop"
$process = Get-CimInstance Win32_Process | Where-Object {
  $_.CommandLine -like "*agv_no_car_feedback_simulator.py*" -or
  $_.Name -eq "agv-no-car-feedback-simulator.exe"
}
if ($process) {
  $process | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Write-Host "no-car simulator stopped"
}
else {
  Write-Host "no-car simulator process not found"
}
