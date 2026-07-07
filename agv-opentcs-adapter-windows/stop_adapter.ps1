$ErrorActionPreference = "Stop"
$process = Get-CimInstance Win32_Process | Where-Object {
  $_.CommandLine -like "*agv_native_feedback_adapter.py*" -or
  $_.Name -eq "agv-native-feedback-adapter.exe"
}
if ($process) {
  $process | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  Write-Host "adapter stopped"
}
else {
  Write-Host "adapter process not found"
}
