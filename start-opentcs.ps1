# OpenTCS Start Script
# Save as: start-opentcs.ps1

$projectPath = "E:\workspace\OpenTCS\opentcs"
$gradleCmd = ".\gradlew.bat"

Write-Host "=== OpenTCS Launcher ===" -ForegroundColor Cyan

# Check directory
if (!(Test-Path "$projectPath\gradlew.bat")) {
    Write-Host "Error: gradlew.bat not found in $projectPath" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Set-Location $projectPath

# Check existing Java processes
$javaProcs = Get-Process -Name "java" -ErrorAction SilentlyContinue
if ($javaProcs) {
    Write-Host "Warning: Java processes already running!" -ForegroundColor Yellow
    $resp = Read-Host "Stop them first? (y/n)"
    if ($resp -eq "y") {
        & $gradleCmd --stop
        taskkill /F /IM java.exe 2>$null
        Start-Sleep -Seconds 2
    }
}

Write-Host "`n[1/3] Starting Kernel..." -ForegroundColor Green
Start-Process powershell -ArgumentList "-NoExit","-Command","cd '$projectPath'; Write-Host '[Kernel] Starting...' -ForegroundColor Green; & '$gradleCmd' :opentcs-kernel:run"

Start-Sleep -Seconds 6

Write-Host "[2/3] Starting OperationsDesk..." -ForegroundColor Green
Start-Process powershell -ArgumentList "-NoExit","-Command","cd '$projectPath'; Write-Host '[OperationsDesk] Starting...' -ForegroundColor Cyan; & '$gradleCmd' :opentcs-operationsdesk:run"

Start-Sleep -Seconds 2

Write-Host "[3/3] Starting KernelControlCenter..." -ForegroundColor Green
Start-Process powershell -ArgumentList "-NoExit","-Command","cd '$projectPath'; Write-Host '[ControlCenter] Starting...' -ForegroundColor Yellow; & '$gradleCmd' :opentcs-kernelcontrolcenter:run"

Write-Host "`n=== All components started! ===" -ForegroundColor Green
Write-Host "Close individual windows to stop"
Read-Host "Press Enter to close this window"