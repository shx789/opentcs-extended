param(
  [Parameter(Mandatory = $true)]
  [string]$ProjectRoot,
  [int]$RcsPort = 8090,
  [int]$WcsPort = 18081
)

$ErrorActionPreference = "Stop"

function Wait-HttpReady {
  param(
    [string]$Url,
    [int]$TimeoutSeconds = 90
  )
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      Invoke-RestMethod -Method Get -Uri $Url -TimeoutSec 2 | Out-Null
      return
    }
    catch {
      if ($_.Exception.Response -ne $null) {
        return
      }
      Start-Sleep -Milliseconds 500
    }
  }
  throw "Timeout waiting for service ready: $Url"
}

function ConvertTo-PrettyJson {
  param([object]$Value)
  return ($Value | ConvertTo-Json -Depth 8)
}

$projectRootResolved = (Resolve-Path $ProjectRoot).Path
$demoDir = Join-Path $projectRootResolved "build\demo"
New-Item -ItemType Directory -Path $demoDir -Force | Out-Null

$callbackLog = Join-Path $demoDir "wcs-callbacks.log"
$wcsStdLog = Join-Path $demoDir "wcs-mock.out.log"
$wcsErrLog = Join-Path $demoDir "wcs-mock.err.log"
$rcsStdLog = Join-Path $demoDir "rcs-sample.out.log"
$rcsErrLog = Join-Path $demoDir "rcs-sample.err.log"

if (Test-Path $callbackLog) { Remove-Item $callbackLog -Force }
if (Test-Path $wcsStdLog) { Remove-Item $wcsStdLog -Force }
if (Test-Path $wcsErrLog) { Remove-Item $wcsErrLog -Force }
if (Test-Path $rcsStdLog) { Remove-Item $rcsStdLog -Force }
if (Test-Path $rcsErrLog) { Remove-Item $rcsErrLog -Force }

$wcsJavaSource = Join-Path $projectRootResolved "opentcs-rcs-integration-sample\scripts\WcsMockServer.java"

$wcsProc = $null
$rcsProc = $null

try {
  Write-Host "[1/6] Starting mock WCS callback server on port $WcsPort ..."
  $wcsProc = Start-Process -FilePath "java.exe" `
    -ArgumentList @(
      $wcsJavaSource,
      "$WcsPort",
      $callbackLog
    ) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $wcsStdLog `
    -RedirectStandardError $wcsErrLog `
    -PassThru

  Wait-HttpReady -Url "http://127.0.0.1:$WcsPort/health" -TimeoutSeconds 20

  Write-Host "[2/6] Starting RCS sample on port $RcsPort ..."
  $gradleCmd = "set `"USERPROFILE=C:\Users\admin`" && set `"HOME=C:\Users\admin`" && set `"GRADLE_USER_HOME=C:\Users\admin\.gradle`" && .\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=$RcsPort -Drcs.callback.baseUrl=http://127.0.0.1:$WcsPort"
  $rcsProc = Start-Process -FilePath "cmd.exe" `
    -ArgumentList @("/c", $gradleCmd) `
    -WorkingDirectory $projectRootResolved `
    -WindowStyle Hidden `
    -RedirectStandardOutput $rcsStdLog `
    -RedirectStandardError $rcsErrLog `
    -PassThru

  Wait-HttpReady -Url "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions/READY-CHECK" -TimeoutSeconds 120

  $timestamp = Get-Date -Format "yyyyMMddHHmmss"
  $missionA = "M${timestamp}A"
  $taskA = "T${timestamp}A"
  $missionB = "M${timestamp}B"
  $taskB = "T${timestamp}B"

  Write-Host "[3/6] Mission A: create -> query -> cancel -> query"
  $missionABody = @{
    mission_no = $missionA
    task_no = $taskA
    from_point = "P_WAIT_IN_01"
    to_point = "ST_IN_01"
    pallet_no = "PLT000000123"
    priority = 30
    callback_url = "/api/v1/wcs/agv/events"
  } | ConvertTo-Json -Depth 4

  $createA = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions" -ContentType "application/json" -Body $missionABody
  $queryA1 = Invoke-RestMethod -Method Get -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions/$missionA"
  $cancelA = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions/$missionA/cancel" -ContentType "application/json" -Body ""
  $queryA2 = Invoke-RestMethod -Method Get -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions/$missionA"

  Write-Host "Create A:" (ConvertTo-PrettyJson $createA)
  Write-Host "Query A before cancel:" (ConvertTo-PrettyJson $queryA1)
  Write-Host "Cancel A:" (ConvertTo-PrettyJson $cancelA)
  Write-Host "Query A after cancel:" (ConvertTo-PrettyJson $queryA2)

  Write-Host "[4/6] Mission B: create -> ingest openTCS FINISHED event -> query"
  $missionBBody = @{
    mission_no = $missionB
    task_no = $taskB
    from_point = "P_WAIT_OUT_01"
    to_point = "ST_OUT_01"
    pallet_no = "PLT000000456"
    priority = 30
    callback_url = "/api/v1/wcs/agv/events"
  } | ConvertTo-Json -Depth 4
  $createB = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions" -ContentType "application/json" -Body $missionBBody

  $eventBody = @{
    eventTime = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    currentObjectState = @{
      name = $missionB
      state = "FINISHED"
      currentDriveOrderIndex = 1
      processingVehicle = "AGV_01"
      properties = @{
        task_no = $taskB
      }
    }
  } | ConvertTo-Json -Depth 6

  $eventResp = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$RcsPort/api/v1/opentcs/events/transport-orders" -ContentType "application/json" -Body $eventBody
  Start-Sleep -Seconds 2
  $queryB = Invoke-RestMethod -Method Get -Uri "http://127.0.0.1:$RcsPort/api/v1/wcs/agv/missions/$missionB"

  Write-Host "Create B:" (ConvertTo-PrettyJson $createB)
  Write-Host "Ingest event response:" (ConvertTo-PrettyJson $eventResp)
  Write-Host "Query B after event:" (ConvertTo-PrettyJson $queryB)

  Write-Host "[5/6] Verifying callback arrived at mock WCS ..."
  if (-not (Test-Path $callbackLog)) {
    throw "Callback log file not found: $callbackLog"
  }
  $callbackLines = Get-Content -Path $callbackLog -ErrorAction SilentlyContinue
  if ($null -eq $callbackLines -or $callbackLines.Count -eq 0) {
    throw "No callback payload received by mock WCS."
  }
  $joinedCallbacks = $callbackLines -join "`n"
  if ($joinedCallbacks -notmatch $missionB) {
    throw "Callback received, but mission $missionB not found in callback log."
  }
  Write-Host "Callback log:" 
  $callbackLines | ForEach-Object { Write-Host $_ }

  Write-Host "[6/6] Demo succeeded."
  Write-Host "Artifacts:"
  Write-Host " - Callback log: $callbackLog"
  Write-Host " - WCS mock logs: $wcsStdLog / $wcsErrLog"
  Write-Host " - RCS sample logs: $rcsStdLog / $rcsErrLog"
}
finally {
  if ($rcsProc -and -not $rcsProc.HasExited) {
    Stop-Process -Id $rcsProc.Id -Force
  }
  if ($wcsProc -and -not $wcsProc.HasExited) {
    Stop-Process -Id $wcsProc.Id -Force
  }
}
