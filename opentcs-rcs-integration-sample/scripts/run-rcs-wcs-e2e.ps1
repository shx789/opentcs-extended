param(
  [Parameter(Mandatory = $true)]
  [string]$ProjectRoot,
  [int]$RcsPort = 8090,
  [switch]$StartRcsSample,
  [string]$OpenTcsBaseUrl = "",
  [int]$ReadyTimeoutSeconds = 120,
  [int]$DoneTimeoutSeconds = 20
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Step {
  param([string]$Message)
  Write-Host "[E2E] $Message"
}

function Assert-Eq {
  param(
    [object]$Actual,
    [object]$Expected,
    [string]$Label
  )
  if ("$Actual" -ne "$Expected") {
    throw "$Label expected '$Expected' but got '$Actual'."
  }
}

function Assert-True {
  param(
    [bool]$Condition,
    [string]$Message
  )
  if (-not $Condition) {
    throw $Message
  }
}

function Invoke-Api {
  param(
    [ValidateSet("GET", "POST")]
    [string]$Method,
    [string]$Url,
    [object]$Body = $null,
    [hashtable]$Headers = @{}
  )
  $params = @{
    Method  = $Method
    Uri     = $Url
    Headers = $Headers
  }
  if ($null -ne $Body) {
    $jsonBody = if ($Body -is [string]) {
      $Body
    }
    else {
      $Body | ConvertTo-Json -Depth 10 -Compress
    }
    $params["ContentType"] = "application/json"
    $params["Body"] = $jsonBody
  }

  $params["UseBasicParsing"] = $true
  $resp = Invoke-WebRequest @params
  $json = $null
  if (-not [string]::IsNullOrWhiteSpace($resp.Content)) {
    $json = $resp.Content | ConvertFrom-Json
  }

  return [PSCustomObject]@{
    StatusCode = [int]$resp.StatusCode
    Json       = $json
    RawContent = $resp.Content
  }
}

function Wait-HttpReady {
  param(
    [string]$Url,
    [int]$TimeoutSeconds
  )
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      Invoke-WebRequest -UseBasicParsing -Method Get -Uri $Url -TimeoutSec 2 | Out-Null
      return
    }
    catch {
      Start-Sleep -Milliseconds 500
    }
  }
  throw "Timeout waiting for service ready: $Url"
}

function Wait-TaskDone {
  param(
    [string]$BaseUrl,
    [string]$BizTaskNo,
    [int]$TimeoutSeconds,
    [hashtable]$Headers
  )
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  $lastStatus = "<unknown>"
  while ((Get-Date) -lt $deadline) {
    $queryResp = Invoke-Api -Method GET -Url "$BaseUrl/api/v1/wcs/tasks/$BizTaskNo" -Headers $Headers
    $lastStatus = [string]$queryResp.Json.data.rcs_status
    if ($lastStatus -eq "DONE") {
      return $queryResp
    }
    Start-Sleep -Milliseconds 500
  }
  throw "Task did not reach DONE in $TimeoutSeconds seconds. Last status: $lastStatus"
}

function Test-TcpPortInUse {
  param([int]$Port)
  $listener = $null
  try {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
    $listener.Start()
    return $false
  }
  catch {
    return $true
  }
  finally {
    if ($listener -ne $null) {
      $listener.Stop()
    }
  }
}

function Get-FreeTcpPort {
  $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
  try {
    $listener.Start()
    return ([int]($listener.LocalEndpoint).Port)
  }
  finally {
    $listener.Stop()
  }
}

$projectRootResolved = (Resolve-Path $ProjectRoot).Path
$demoDir = Join-Path $projectRootResolved "build\demo"
New-Item -ItemType Directory -Path $demoDir -Force | Out-Null
$gradleUserHome = Join-Path $projectRootResolved ".gradle-demo-home"
New-Item -ItemType Directory -Path $gradleUserHome -Force | Out-Null

$runTag = Get-Date -Format "yyyyMMddHHmmssfff"
$rcsStdLog = Join-Path $demoDir "rcs-e2e-$runTag.out.log"
$rcsErrLog = Join-Path $demoDir "rcs-e2e-$runTag.err.log"
$timestamp = Get-Date -Format "yyyyMMddHHmmss"
$bizTaskNo = "BIZ-OUT-${timestamp}E2E"
$missionNo = "M${timestamp}E2EOUT"
$taskNo = "T${timestamp}E2EOUT"
$traceId = "trace-e2e-$([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())"
$requestId = "req-e2e-$(([Guid]::NewGuid().ToString('N')).Substring(0, 12))"
$commonHeaders = @{
  "Accept"       = "application/json"
  "X-Trace-Id"   = $traceId
  "X-Request-Id" = $requestId
}

$rcsProc = $null

try {
  if ($StartRcsSample -and (Test-TcpPortInUse -Port $RcsPort)) {
    $newPort = Get-FreeTcpPort
    Write-Step "Port $RcsPort is in use, switching to free port $newPort"
    $RcsPort = $newPort
  }
  $baseUrl = "http://127.0.0.1:$RcsPort"

  if ($StartRcsSample) {
    $modeLabel = if ([string]::IsNullOrWhiteSpace($OpenTcsBaseUrl)) {
      "simulation"
    }
    else {
      "live-openTCS"
    }
    Write-Step "Starting RCS sample (port=$RcsPort, mode=$modeLabel)"
    Write-Step "RCS stdout log: $rcsStdLog"
    Write-Step "RCS stderr log: $rcsErrLog"
    $openTcsArg = if ([string]::IsNullOrWhiteSpace($OpenTcsBaseUrl)) {
      ""
    }
    else {
      " -Drcs.openTcs.baseUrl=$OpenTcsBaseUrl"
    }
    $gradleCmd = "set `"USERPROFILE=C:\Users\admin`" && set `"HOME=C:\Users\admin`" && set `"GRADLE_USER_HOME=$gradleUserHome`" && set `"RCS_WMS_BASE_URL=$baseUrl`" && .\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=$RcsPort$openTcsArg"
    $rcsProc = Start-Process -FilePath "cmd.exe" `
      -ArgumentList @("/c", $gradleCmd) `
      -WorkingDirectory $projectRootResolved `
      -WindowStyle Hidden `
      -RedirectStandardOutput $rcsStdLog `
      -RedirectStandardError $rcsErrLog `
      -PassThru
  }

  Write-Step "Waiting for endpoint ready: $baseUrl/demo/wcs"
  Wait-HttpReady -Url "$baseUrl/demo/wcs" -TimeoutSeconds $ReadyTimeoutSeconds

  Write-Step "Clearing callback inbox"
  $clearResp = Invoke-Api -Method POST -Url "$baseUrl/demo/wcs/callbacks/clear" -Headers $commonHeaders
  Assert-Eq -Actual $clearResp.StatusCode -Expected 200 -Label "Clear callbacks HTTP"
  Assert-Eq -Actual $clearResp.Json.code -Expected "0" -Label "Clear callbacks code"

  Write-Step "Creating outbound task $bizTaskNo"
  $createBody = @{
    biz_task_no  = $bizTaskNo
    mission_no   = $missionNo
    task_no      = $taskNo
    from_point   = "P_WAIT_OUT_01"
    to_point     = "ST_OUT_01"
    pallet_no    = "PLT000000123"
    priority     = 80
    callback_url = "$baseUrl/demo/wcs/callback"
  }
  $createResp = Invoke-Api `
    -Method POST `
    -Url "$baseUrl/api/v1/wcs/outbound/tasks" `
    -Body $createBody `
    -Headers $commonHeaders
  Assert-Eq -Actual $createResp.StatusCode -Expected 200 -Label "Create task HTTP"
  Assert-Eq -Actual $createResp.Json.code -Expected "0" -Label "Create task code"
  Assert-Eq -Actual $createResp.Json.data.biz_task_no -Expected $bizTaskNo -Label "Create biz_task_no"
  Assert-Eq -Actual $createResp.Json.data.mission_no -Expected $missionNo -Label "Create mission_no"
  Assert-Eq -Actual $createResp.Json.data.task_no -Expected $taskNo -Label "Create task_no"
  Assert-Eq -Actual $createResp.Json.data.rcs_status -Expected "RECEIVED" -Label "Create status"

  Write-Step "Injecting FINISHED event for $missionNo"
  $eventBody = @{
    eventTime          = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    currentObjectState = @{
      name                   = $missionNo
      state                  = "FINISHED"
      currentDriveOrderIndex = 1
      processingVehicle      = "AGV_01"
      properties             = @{
        task_no = $taskNo
      }
    }
  }
  $eventResp = Invoke-Api `
    -Method POST `
    -Url "$baseUrl/api/v1/opentcs/events/transport-orders" `
    -Body $eventBody `
    -Headers $commonHeaders
  Assert-Eq -Actual $eventResp.StatusCode -Expected 202 -Label "Inject event HTTP"
  Assert-True -Condition ([bool]$eventResp.Json.accepted) -Message "Inject event response.accepted should be true."
  Assert-True -Condition ([bool]$eventResp.Json.mapped) -Message "Inject event response.mapped should be true."
  Assert-Eq -Actual $eventResp.Json.missionNo -Expected $missionNo -Label "Inject event missionNo"

  Write-Step "Waiting task status DONE"
  $doneResp = Wait-TaskDone -BaseUrl $baseUrl -BizTaskNo $bizTaskNo -TimeoutSeconds $DoneTimeoutSeconds -Headers $commonHeaders
  Assert-Eq -Actual $doneResp.Json.code -Expected "0" -Label "Query task code"
  Assert-Eq -Actual $doneResp.Json.data.rcs_status -Expected "DONE" -Label "Task final status"

  Write-Step "Validating mission callback + WMS result callback payloads"
  $callbacksResp = Invoke-Api -Method GET -Url "$baseUrl/demo/wcs/callbacks" -Headers $commonHeaders
  Assert-Eq -Actual $callbacksResp.StatusCode -Expected 200 -Label "List callbacks HTTP"
  Assert-Eq -Actual $callbacksResp.Json.code -Expected "0" -Label "List callbacks code"

  $records = @($callbacksResp.Json.data)
  Assert-True -Condition ($records.Count -gt 0) -Message "Callback inbox is empty."

  $missionPayload = $null
  $resultPayload = $null
  foreach ($record in $records) {
    if ($null -eq $record.payload) {
      continue
    }
    try {
      $payloadObj = $record.payload | ConvertFrom-Json
    }
    catch {
      continue
    }
    $hasMissionNo = $payloadObj.PSObject.Properties.Name -contains "mission_no"
    $hasEventType = $payloadObj.PSObject.Properties.Name -contains "event_type"
    $hasBizTaskNo = $payloadObj.PSObject.Properties.Name -contains "biz_task_no"
    $hasResultType = $payloadObj.PSObject.Properties.Name -contains "result_type"

    if ($hasMissionNo -and $hasEventType -and $payloadObj.mission_no -eq $missionNo -and $payloadObj.event_type -eq "DROPPED") {
      $missionPayload = $payloadObj
    }
    if ($hasBizTaskNo -and $hasResultType -and $payloadObj.biz_task_no -eq $bizTaskNo -and $payloadObj.result_type -eq "DONE") {
      $resultPayload = $payloadObj
    }
  }

  Assert-True -Condition ($null -ne $missionPayload) -Message "No mission callback payload found for mission $missionNo."
  Assert-Eq -Actual $missionPayload.event_type -Expected "DROPPED" -Label "Mission callback event_type"
  Assert-Eq -Actual $missionPayload.trace_id -Expected $traceId -Label "Mission callback trace_id"
  Assert-Eq -Actual $missionPayload.request_id -Expected $requestId -Label "Mission callback request_id"

  Assert-True -Condition ($null -ne $resultPayload) -Message "No WMS result callback payload found for task $bizTaskNo."
  Assert-Eq -Actual $resultPayload.result_type -Expected "DONE" -Label "WMS result_type"
  Assert-Eq -Actual $resultPayload.task_type -Expected "OUTBOUND" -Label "WMS task_type"
  Assert-Eq -Actual $resultPayload.trace_id -Expected $traceId -Label "WMS trace_id"
  Assert-Eq -Actual $resultPayload.request_id -Expected $requestId -Label "WMS request_id"

  Write-Step "PASS"
  [PSCustomObject]@{
    rcs_port          = $RcsPort
    biz_task_no       = $bizTaskNo
    mission_no      = $missionNo
    task_no         = $taskNo
    task_status      = $doneResp.Json.data.rcs_status
    mission_event    = $missionPayload.event_type
    result_type      = $resultPayload.result_type
    callback_trace   = $resultPayload.trace_id
    callback_request = $resultPayload.request_id
  } | Format-List | Out-String | Write-Host
}
finally {
  if ($rcsProc -and -not $rcsProc.HasExited) {
    Stop-Process -Id $rcsProc.Id -Force
  }
}
