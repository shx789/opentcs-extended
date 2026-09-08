$ErrorActionPreference = "Stop"
$process = Get-CimInstance Win32_Process | Where-Object {
  $_.CommandLine -like "*agv_config_web.py*" -or
  $_.Name -eq "agv-config-web.exe"
}
if ($process) {
  $process | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Write-Host "config web stopped"
}
else {
  Write-Host "config web process not found"
}
