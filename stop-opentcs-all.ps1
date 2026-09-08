param(
  [switch]$KeepWindows
)

$ErrorActionPreference = "Stop"

$targetMainClasses = @(
  "org.opentcs.kernel.RunKernel",
  "org.opentcs.kernelcontrolcenter.RunKernelControlCenter",
  "org.opentcs.modeleditor.RunModelEditor",
  "org.opentcs.operationsdesk.RunOperationsDesk"
)

$javaProcessNames = @("java.exe", "javaw.exe")

Write-Host "Searching openTCS Java processes..."

$processes = Get-CimInstance Win32_Process | Where-Object {
  $javaProcessNames -contains $_.Name -and
  -not [string]::IsNullOrWhiteSpace($_.CommandLine)
}

$matched = foreach ($p in $processes) {
  foreach ($mainClass in $targetMainClasses) {
    if ($p.CommandLine -like "*$mainClass*") {
      [PSCustomObject]@{
        ProcessId = [int]$p.ProcessId
        Name = $p.Name
        MainClass = $mainClass
      }
      break
    }
  }
}

if (-not $matched -or $matched.Count -eq 0) {
  Write-Host "No running openTCS target Java processes found."
}
else {
  foreach ($proc in $matched | Sort-Object ProcessId -Unique) {
    try {
      Stop-Process -Id $proc.ProcessId -Force -ErrorAction Stop
      Write-Host ("Stopped PID {0} ({1})" -f $proc.ProcessId, $proc.MainClass)
    }
    catch {
      Write-Warning ("Failed to stop PID {0}: {1}" -f $proc.ProcessId, $_.Exception.Message)
    }
  }
}

if (-not $KeepWindows) {
  Write-Host "Closing helper PowerShell windows (title starts with 'openTCS')..."
  $psWindows = Get-Process powershell -ErrorAction SilentlyContinue | Where-Object {
    $_.MainWindowTitle -like "openTCS*"
  }

  foreach ($w in $psWindows) {
    try {
      Stop-Process -Id $w.Id -Force -ErrorAction Stop
    }
    catch {
      Write-Warning ("Failed to close PowerShell window PID {0}: {1}" -f $w.Id, $_.Exception.Message)
    }
  }
}

Write-Host "Done."
