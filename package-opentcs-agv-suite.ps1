param(
  [string]$ReleaseRoot = ".\release",
  [string]$SuiteName = "opentcs-agv-suite",
  [switch]$SkipPythonExeBuild,
  [switch]$SkipRcsBuild,
  [switch]$SkipOpenTcsBuild
)

$ErrorActionPreference = "Stop"
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$ReleaseRoot = [System.IO.Path]::GetFullPath((Join-Path $Root $ReleaseRoot))
$SuiteDir = Join-Path $ReleaseRoot $SuiteName
$ZipPath = Join-Path $ReleaseRoot "$SuiteName.zip"
$OpenTcsSource = Join-Path $Root "opentcs-7.3.0-SNAPSHOT-windows-portable-cac08887\opentcs-7.3.0-SNAPSHOT-bin"
$AdapterRoot = Join-Path $Root "agv-opentcs-adapter-windows"
$AdapterRelease = Join-Path $AdapterRoot "release"
$DashboardSource = Join-Path $Root "map_mapping_output\map_mapping_output"
$DashboardRelease = Join-Path $Root "map_mapping_output\release"
$RcsLibDir = Join-Path $Root "opentcs-rcs-integration-sample\build\libs"
$OpenTcsKernelBuildLibDir = Join-Path $Root "opentcs-kernel\build\install\opentcs-kernel\lib"

function Copy-CleanDir {
  param([string]$Source, [string]$Destination)
  if (!(Test-Path -LiteralPath $Source)) {
    throw "Missing source directory: $Source"
  }
  if (Test-Path -LiteralPath $Destination) {
    Remove-Item -LiteralPath $Destination -Recurse -Force
  }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
  Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Write-Utf8NoBom {
  param([string]$Path, [string]$Content)
  $dir = Split-Path -Parent $Path
  if ($dir) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
  }
  $encoding = New-Object System.Text.UTF8Encoding($false)
  [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

if (!(Test-Path -LiteralPath $OpenTcsSource)) {
  throw "openTCS portable directory not found: $OpenTcsSource"
}

if (!$SkipPythonExeBuild) {
  & (Join-Path $AdapterRoot "build_windows_exe.ps1") -OutputDir $AdapterRelease -IncludeNoCarSimulator

  New-Item -ItemType Directory -Force -Path $DashboardRelease | Out-Null
  & python -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --distpath $DashboardRelease `
    --workpath (Join-Path $Root "map_mapping_output\build\pyinstaller-work") `
    --specpath (Join-Path $Root "map_mapping_output\build\pyinstaller-spec") `
    --hidden-import amqtt.plugins.logging_amqtt `
    --name agv-opentcs-dashboard `
    (Join-Path $DashboardSource "agv_opentcs_dashboard_server.py")
}

if (!$SkipRcsBuild) {
  & (Join-Path $Root "gradlew.bat") :opentcs-rcs-integration-sample:shadowJar
}

if (!$SkipOpenTcsBuild) {
  & (Join-Path $Root "gradlew.bat") :opentcs-commadapter-mqtt:build :opentcs-kernel:installDist
}

if (!(Test-Path -LiteralPath $AdapterRelease)) {
  throw "adapter release not found: $AdapterRelease"
}
if (!(Test-Path -LiteralPath (Join-Path $DashboardRelease "agv-opentcs-dashboard.exe"))) {
  throw "dashboard exe not found: $(Join-Path $DashboardRelease "agv-opentcs-dashboard.exe")"
}
$RcsJar = Get-ChildItem -LiteralPath $RcsLibDir -Filter "*-all.jar" -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if ($null -eq $RcsJar) {
  throw "RCS fat jar not found in: $RcsLibDir"
}

if (Test-Path -LiteralPath $SuiteDir) {
  Remove-Item -LiteralPath $SuiteDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $SuiteDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $SuiteDir "rcs") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $SuiteDir "dashboard") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $SuiteDir "logs") | Out-Null

Copy-CleanDir -Source $OpenTcsSource -Destination (Join-Path $SuiteDir "opentcs")
$PackagedOpenTcsKernelLib = Join-Path $SuiteDir "opentcs\opentcs-kernel\lib"
$PackagedOpenTcsClientLibs = @(
  $PackagedOpenTcsKernelLib,
  (Join-Path $SuiteDir "opentcs\opentcs-kernelcontrolcenter\lib"),
  (Join-Path $SuiteDir "opentcs\opentcs-operationsdesk\lib")
)
foreach ($libDir in $PackagedOpenTcsClientLibs) {
  if (!(Test-Path -LiteralPath $libDir)) {
    throw "packaged openTCS lib not found: $libDir"
  }
}
foreach ($filter in @(
  "opentcs-commadapter-mqtt-*.jar",
  "org.eclipse.paho.client.mqttv3-*.jar"
)) {
  $jars = Get-ChildItem -LiteralPath $OpenTcsKernelBuildLibDir -Filter $filter -ErrorAction SilentlyContinue
  if ($null -eq $jars -or $jars.Count -eq 0) {
    throw "Required MQTT adapter runtime jar not found: $filter in $OpenTcsKernelBuildLibDir"
  }
  foreach ($libDir in $PackagedOpenTcsClientLibs) {
    Get-ChildItem -LiteralPath $libDir -Filter $filter -ErrorAction SilentlyContinue |
      Remove-Item -Force
    foreach ($jar in $jars) {
      Copy-Item -LiteralPath $jar.FullName -Destination $libDir -Force
    }
  }
}
$PackagedKernelStart = Join-Path $SuiteDir "opentcs\opentcs-kernel\startKernel.bat"
if (Test-Path -LiteralPath $PackagedKernelStart) {
  $kernelStartContent = Get-Content -LiteralPath $PackagedKernelStart -Raw
  if ($kernelStartContent -notmatch "OPENTCS_MQTT_JAVA_OPTS") {
    $kernelStartContent = $kernelStartContent.Replace(
      '    -Djava.util.logging.config.file="%OPENTCS_CONFIGDIR%\logging.config" ^',
      "    -Djava.util.logging.config.file=`"%OPENTCS_CONFIGDIR%\logging.config`" ^`r`n    %OPENTCS_MQTT_JAVA_OPTS% ^"
    )
    Write-Utf8NoBom -Path $PackagedKernelStart -Content $kernelStartContent
  }
}
Copy-CleanDir -Source $AdapterRelease -Destination (Join-Path $SuiteDir "adapter")
$PackagedAdapterConfig = Join-Path $SuiteDir "adapter\config"
if (Test-Path -LiteralPath $PackagedAdapterConfig) {
  Get-ChildItem -LiteralPath $PackagedAdapterConfig -Directory -Filter "generated-*" -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force
  foreach ($name in @(
    "agv_config_web_output.txt",
    "agv_config_web_status.json",
    "web.err.log",
    "web.out.log"
  )) {
    $path = Join-Path $PackagedAdapterConfig $name
    if (Test-Path -LiteralPath $path) {
      Remove-Item -LiteralPath $path -Force
    }
  }
}
Copy-Item -LiteralPath (Join-Path $DashboardRelease "agv-opentcs-dashboard.exe") -Destination (Join-Path $SuiteDir "dashboard") -Force
foreach ($name in @(
  "agv_opentcs_business_topology_draft.json",
  "opentcs_plant_model_business_topology_draft.json",
  "agv_business_topology_preview.png"
)) {
  $source = Join-Path $DashboardSource $name
  if (Test-Path -LiteralPath $source) {
    Copy-Item -LiteralPath $source -Destination (Join-Path $SuiteDir "dashboard") -Force
  }
}
Copy-Item -LiteralPath $RcsJar.FullName -Destination (Join-Path $SuiteDir "rcs\opentcs-rcs-integration-sample-all.jar") -Force

$startAll = @'
@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-suite.ps1" -KeepAlive
pause
'@

$startSuite = @'
param(
  [switch]$DryRun,
  [switch]$KeepAlive
)

$ErrorActionPreference = "Stop"

$Base = Split-Path -Parent $MyInvocation.MyCommand.Path
$AdapterDir = Join-Path $Base "adapter"
$DashboardDir = Join-Path $Base "dashboard"
$OpenTcsDir = Join-Path $Base "opentcs"
$RcsDir = Join-Path $Base "rcs"
$LogDir = Join-Path $Base "logs"
$PidFile = Join-Path $LogDir "suite-pids.txt"
$StopFlag = Join-Path $LogDir "stop-requested.flag"
$JavaExe = Join-Path $OpenTcsDir "runtime\bin\java.exe"
$ConfigPath = Join-Path $AdapterDir "config\runtime_config.json"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
if (Test-Path -LiteralPath $StopFlag) {
  Remove-Item -LiteralPath $StopFlag -Force
}
$TranscriptStarted = $false
try {
  $StartLog = Join-Path $LogDir ("suite-start-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".log")
  Start-Transcript -Path $StartLog -Append | Out-Null
  $TranscriptStarted = $true
}
catch {
  Write-Host "Could not start transcript: $($_.Exception.Message)"
}
trap {
  Write-Host "ERROR: $($_.Exception.Message)"
  if ($TranscriptStarted) {
    try { Stop-Transcript | Out-Null } catch {}
  }
  throw
}

function Get-ConfigValue {
  param($Config, [string]$Name, [string]$Default)
  if ($null -ne $Config -and $Config.PSObject.Properties.Name -contains $Name) {
    $value = [string]$Config.$Name
    if (![string]::IsNullOrWhiteSpace($value)) {
      return $value.Trim()
    }
  }
  return $Default
}

function Convert-ToTcpMqttUri {
  param([string]$Uri)
  if ([string]::IsNullOrWhiteSpace($Uri)) {
    return "tcp://127.0.0.1:1883"
  }
  $value = $Uri.Trim().TrimEnd("/")
  if ($value.StartsWith("mqtt://", [System.StringComparison]::OrdinalIgnoreCase)) {
    return "tcp://" + $value.Substring(7)
  }
  if ($value.StartsWith("mqtts://", [System.StringComparison]::OrdinalIgnoreCase)) {
    return "ssl://" + $value.Substring(8)
  }
  if ($value -notmatch "^[a-zA-Z][a-zA-Z0-9+.-]*://") {
    return "tcp://" + $value
  }
  return $value
}

function Test-LocalBrokerUri {
  param([string]$Uri)
  try {
    $hostName = ([System.Uri]$Uri).Host
  }
  catch {
    return $false
  }
  return @("127.0.0.1", "localhost", "::1") -contains $hostName.ToLowerInvariant()
}

function Convert-EventTopicTemplate {
  param([string]$Template)
  if ([string]::IsNullOrWhiteSpace($Template)) {
    return "agv/+/task/events"
  }
  return $Template.Trim().Replace("{agv_id}", "+")
}

function Convert-PointIdMapForRcs {
  param([string]$PointIdMap)
  $pairs = @()
  foreach ($item in ($PointIdMap -split ",")) {
    $part = $item.Trim()
    if ([string]::IsNullOrWhiteSpace($part) -or $part -notmatch "=") {
      continue
    }
    $tokens = $part -split "=", 2
    $agvPointId = $tokens[0].Trim()
    $openTcsPoint = $tokens[1].Trim()
    if ($agvPointId -match "^[^:]+:(.+)$") {
      $agvPointId = $Matches[1]
    }
    if (![string]::IsNullOrWhiteSpace($agvPointId) -and ![string]::IsNullOrWhiteSpace($openTcsPoint)) {
      $pairs += "$openTcsPoint=$agvPointId"
    }
  }
  $uniquePairs = $pairs | Select-Object -Unique
  if ($uniquePairs.Count -eq 0) {
    return "AGV_E_01=1,AGV_E_04=4"
  }
  return ($uniquePairs -join ",")
}

function Add-SuitePid {
  param([int]$ProcessId)
  if ($ProcessId -gt 0) {
    Add-Content -LiteralPath $PidFile -Value $ProcessId
  }
}

function Start-CmdWindow {
  param([string]$Title, [string]$WorkingDirectory, [string]$Command)
  $safeTitle = $Title.Replace('"', '')
  $cmdLine = "start `"$safeTitle`" /D `"$WorkingDirectory`" cmd.exe /k `"title $safeTitle && $Command`""
  $proc = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", $cmdLine) -PassThru -WindowStyle Hidden
  Add-SuitePid -ProcessId $proc.Id
  return $proc
}

function Start-TrackedProcess {
  param([string]$FilePath, [string]$WorkingDirectory)
  $title = "AGV Suite - " + [System.IO.Path]::GetFileNameWithoutExtension($FilePath)
  return Start-CmdWindow -Title $title -WorkingDirectory $WorkingDirectory -Command "call `"$FilePath`""
}

function Wait-HttpOk {
  param([string]$Url, [int]$TimeoutSeconds)
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      $response = Invoke-WebRequest -UseBasicParsing -Uri $Url -TimeoutSec 3
      if ($response.StatusCode -ge 200 -and $response.StatusCode -lt 500) {
        return $true
      }
    }
    catch {
      Start-Sleep -Seconds 2
    }
  }
  return $false
}

function Get-ListeningPids {
  param([int[]]$Ports)
  $wanted = @{}
  foreach ($port in $Ports) {
    $wanted[[string]$port] = $true
  }
  $pids = New-Object System.Collections.Generic.HashSet[int]
  $lines = & netstat -ano -p tcp 2>$null
  foreach ($line in $lines) {
    if ($line -notmatch "LISTENING") {
      continue
    }
    if ($line -match "^\s*TCP\s+\S+:(\d+)\s+\S+\s+LISTENING\s+(\d+)\s*$") {
      $port = $Matches[1]
      $pidValue = [int]$Matches[2]
      if ($wanted.ContainsKey($port) -and $pidValue -gt 0) {
        [void]$pids.Add($pidValue)
      }
    }
  }
  return @($pids)
}

function Stop-ListeningPorts {
  param([int[]]$Ports)
  $pids = Get-ListeningPids -Ports $Ports
  foreach ($pidValue in $pids) {
    if ($pidValue -eq $PID) {
      continue
    }
    try {
      $proc = Get-Process -Id $pidValue -ErrorAction Stop
      Write-Host "Stopping old service on suite port: pid=$pidValue process=$($proc.ProcessName)"
      Stop-Process -Id $pidValue -Force -ErrorAction SilentlyContinue
    }
    catch {
      Write-Host "Could not stop pid=${pidValue}: $($_.Exception.Message)"
    }
  }
}

if (!(Test-Path -LiteralPath $JavaExe)) {
  throw "Missing Java runtime: $JavaExe"
}
if (!(Test-Path -LiteralPath $ConfigPath)) {
  throw "Missing runtime config: $ConfigPath"
}

$Config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$MqttBrokerUri = Convert-ToTcpMqttUri (Get-ConfigValue $Config "mqtt_uri" "mqtt://127.0.0.1:1883/")
$CommandTopic = Get-ConfigValue $Config "command_topic" "robot_control"
$FeedbackTopic = Get-ConfigValue $Config "source_topic" "task_feedback"
$EventTopic = Convert-EventTopicTemplate (Get-ConfigValue $Config "rcs_event_topic_template" "agv/{agv_id}/task/events")
$OpenTcsBaseUrl = Get-ConfigValue $Config "open_tcs_base_url" "http://127.0.0.1:55200"
$PointIdMap = if (![string]::IsNullOrWhiteSpace($env:RCS_POINT_ID_MAP)) {
  $env:RCS_POINT_ID_MAP
}
else {
  Convert-PointIdMapForRcs (Get-ConfigValue $Config "point_id_map" "")
}
$AgvCommandPathMode = Get-ConfigValue $Config "agv_command_path_mode" "2"
$AgvCommandCirculates = Get-ConfigValue $Config "agv_command_circulates" "0"
$AgvCommandQos = Get-ConfigValue $Config "agv_command_qos" "0"
$StartExternalAdapter = if (![string]::IsNullOrWhiteSpace($env:AGV_START_EXTERNAL_ADAPTER)) {
  @("1", "true", "yes", "on") -contains $env:AGV_START_EXTERNAL_ADAPTER.Trim().ToLowerInvariant()
}
else {
  @("1", "true", "yes", "on") -contains (Get-ConfigValue $Config "start_external_feedback_adapter" "false").ToLowerInvariant()
}
$EnableRcsMqttStatusBridge = if (![string]::IsNullOrWhiteSpace($env:RCS_AGV_MQTT_ENABLED)) {
  @("1", "true", "yes", "on") -contains $env:RCS_AGV_MQTT_ENABLED.Trim().ToLowerInvariant()
}
else {
  @("1", "true", "yes", "on") -contains (Get-ConfigValue $Config "enable_rcs_mqtt_status_bridge" "false").ToLowerInvariant()
}
$EnableRcsDirectCommandBridge = if (![string]::IsNullOrWhiteSpace($env:RCS_AGV_COMMAND_ENABLED)) {
  @("1", "true", "yes", "on") -contains $env:RCS_AGV_COMMAND_ENABLED.Trim().ToLowerInvariant()
}
else {
  @("1", "true", "yes", "on") -contains (Get-ConfigValue $Config "enable_rcs_direct_command_bridge" "false").ToLowerInvariant()
}

Write-Host "Runtime config: $ConfigPath"
Write-Host "MQTT broker:    $MqttBrokerUri"
Write-Host "Command topic:  $CommandTopic"
Write-Host "Feedback topic: $FeedbackTopic"
Write-Host "Event topic:    $EventTopic"
Write-Host "Point map:      $PointIdMap"
Write-Host "AGV path_mode:  $AgvCommandPathMode"
Write-Host "AGV circulates: $AgvCommandCirculates"
Write-Host "AGV command QoS: $AgvCommandQos"
Write-Host "External adapter: $StartExternalAdapter"
Write-Host "RCS MQTT status bridge: $EnableRcsMqttStatusBridge"
Write-Host "RCS direct command bridge: $EnableRcsDirectCommandBridge"
Write-Host ""

if ($DryRun) {
  Write-Host "Dry run only. No services were started."
  return
}

$PortsToClear = @(8090, 8091, 8092, 55200)
if (Test-LocalBrokerUri $MqttBrokerUri) {
  $PortsToClear += 1883
}
Write-Host "Clearing old suite services on ports: $($PortsToClear -join ', ')"
if (Test-Path -LiteralPath $PidFile) {
  Remove-Item -LiteralPath $PidFile -Force
}
Stop-ListeningPorts -Ports $PortsToClear
Start-Sleep -Seconds 2

if (Test-LocalBrokerUri $MqttBrokerUri) {
  Write-Host "Starting local MQTT broker..."
  $BrokerExe = Join-Path $AdapterDir "local-mqtt-broker.exe"
  $BrokerOut = Join-Path $LogDir "local-mqtt-broker.out.log"
  $BrokerErr = Join-Path $LogDir "local-mqtt-broker.err.log"
  Start-CmdWindow `
    -Title "AGV Suite - MQTT Broker" `
    -WorkingDirectory $AdapterDir `
    -Command "`"$BrokerExe`" >> `"$BrokerOut`" 2>> `"$BrokerErr`"" | Out-Null
  Start-Sleep -Seconds 2
}
else {
  Write-Host "Using external AGV MQTT broker. Local broker will not be started."
}

Write-Host "Starting openTCS..."
$env:OPENTCS_MQTT_JAVA_OPTS = @(
  "-Dopentcs.mqtt.brokerUri=`"$MqttBrokerUri`"",
  "-Dopentcs.mqtt.commandTopic=`"$CommandTopic`"",
  "-Dopentcs.mqtt.feedbackTopic=`"$FeedbackTopic`"",
  "-Dopentcs.mqtt.pointIdMap=`"$PointIdMap`""
) -join " "
Start-TrackedProcess -FilePath (Join-Path $OpenTcsDir "start-all.bat") -WorkingDirectory $OpenTcsDir | Out-Null
if (!(Wait-HttpOk -Url "$OpenTcsBaseUrl/v1/kernel/version" -TimeoutSeconds 60)) {
  throw "openTCS kernel did not become reachable: $OpenTcsBaseUrl"
}
try {
  $vehiclesJson = (Invoke-WebRequest -UseBasicParsing -Uri "$OpenTcsBaseUrl/v1/vehicles" -TimeoutSec 5).Content
  $vehicles = $vehiclesJson | ConvertFrom-Json
  $vehicleCount = @($vehicles).Count
  Write-Host "openTCS vehicles: $vehicleCount"
}
catch {
  Write-Host "Could not read openTCS vehicles: $($_.Exception.Message)"
}

Write-Host "Starting RCS on 8090..."
$RcsOut = Join-Path $LogDir "rcs.out.log"
$RcsErr = Join-Path $LogDir "rcs.err.log"
$RcsCommand = @(
  "set RCS_OPENTCS_BASE_URL=$OpenTcsBaseUrl",
  "set RCS_OPENTCS_SSE_ENABLED=true",
  "set RCS_AGV_MQTT_ENABLED=$($EnableRcsMqttStatusBridge.ToString().ToLowerInvariant())",
  "set RCS_AGV_MQTT_BROKER_URI=$MqttBrokerUri",
  "set RCS_AGV_MQTT_TOPIC=$EventTopic",
  "set RCS_AGV_COMMAND_ENABLED=$($EnableRcsDirectCommandBridge.ToString().ToLowerInvariant())",
  "set RCS_AGV_COMMAND_BROKER_URI=$MqttBrokerUri",
  "set RCS_AGV_COMMAND_TOPIC=$CommandTopic",
  "set RCS_AGV_COMMAND_QOS=$AgvCommandQos",
  "set RCS_AGV_POINT_ID_MAP=$PointIdMap",
  "set RCS_AGV_PATH_MODE=$AgvCommandPathMode",
  "set RCS_AGV_CIRCULATES=$AgvCommandCirculates",
  "`"$JavaExe`" -jar opentcs-rcs-integration-sample-all.jar 8090 >> `"$RcsOut`" 2>> `"$RcsErr`""
) -join " && "
Start-CmdWindow -Title "AGV Suite - RCS" -WorkingDirectory $RcsDir -Command $RcsCommand | Out-Null

if ($StartExternalAdapter) {
  Write-Host "Starting external feedback adapter..."
  Start-TrackedProcess -FilePath (Join-Path $AdapterDir "start_adapter.bat") -WorkingDirectory $AdapterDir | Out-Null
}
else {
  Write-Host "Skipping external feedback adapter. MQTT feedback is handled by openTCS MqttCommAdapter."
}

Write-Host "Starting config web on 8091..."
Start-TrackedProcess -FilePath (Join-Path $AdapterDir "start_config_web.bat") -WorkingDirectory $AdapterDir | Out-Null

Write-Host "Starting dashboard on 8092..."
$DashboardOut = Join-Path $LogDir "dashboard.out.log"
$DashboardErr = Join-Path $LogDir "dashboard.err.log"
$DashboardCommand = "set AGV_ADAPTER_ROOT=$AdapterDir && set AGV_DASHBOARD_ROOT=$DashboardDir && set AGV_DASHBOARD_PORT=8092 && agv-opentcs-dashboard.exe >> `"$DashboardOut`" 2>> `"$DashboardErr`""
Start-CmdWindow -Title "AGV Suite - Dashboard" -WorkingDirectory $DashboardDir -Command $DashboardCommand | Out-Null

Write-Host ""
Write-Host "Started. Open:"
Write-Host "  Dashboard: http://127.0.0.1:8092/"
Write-Host "  Config:    http://127.0.0.1:8091/"
Write-Host ""
Write-Host "Logs:"
Write-Host "  Suite:     $LogDir"
Write-Host "  Adapter:   $(Join-Path $AdapterDir "logs")"
Write-Host ""
Write-Host "For a real AGV, connect to the AGV WiFi first, then set mqtt_uri on the Config page: http://127.0.0.1:8091/"
if ($KeepAlive) {
  Write-Host ""
  Write-Host "Suite supervisor is running. Use stop-all.bat to stop all services."
  while (!(Test-Path -LiteralPath $StopFlag)) {
    Start-Sleep -Seconds 2
  }
  Write-Host "Stop requested. Supervisor exiting."
}
if ($TranscriptStarted) {
  try { Stop-Transcript | Out-Null } catch {}
}
'@

$stopAll = @'
@echo off
setlocal
set "BASE=%~dp0"
if not exist "%BASE%logs" mkdir "%BASE%logs"
set "STOP_LOG=%BASE%logs\suite-stop.log"
set "PID_FILE=%BASE%logs\suite-pids.txt"
set "STOP_FLAG=%BASE%logs\stop-requested.flag"
echo [%date% %time%] Stopping AGV Suite... >> "%STOP_LOG%"
echo stop > "%STOP_FLAG%"
echo Stopping tracked process trees...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$pidFile='%PID_FILE%';" ^
  "if(Test-Path -LiteralPath $pidFile){ $pids=Get-Content -LiteralPath $pidFile | Where-Object {$_ -match '^\d+$'} | Select-Object -Unique; foreach($pidValue in $pids){ if([int]$pidValue -gt 0 -and [int]$pidValue -ne $PID){ Write-Host ('Stopping tracked pid=' + $pidValue); & taskkill /T /F /PID $pidValue *> $null } } Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue }" >> "%STOP_LOG%" 2>&1
echo Stopping suite port owners...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ports=@(1883,8090,8091,8092,55200);" ^
  "$pids=New-Object System.Collections.Generic.HashSet[int];" ^
  "$lines=& netstat -ano -p tcp 2>$null;" ^
  "foreach($line in $lines){ if($line -match 'LISTENING' -and $line -match '^\s*TCP\s+\S+:(\d+)\s+\S+\s+LISTENING\s+(\d+)\s*$'){ if($ports -contains [int]$Matches[1]){ [void]$pids.Add([int]$Matches[2]) } } }" ^
  "foreach($pidValue in $pids){ if($pidValue -gt 0 -and $pidValue -ne $PID){ Write-Host ('Stopping port owner pid=' + $pidValue); Stop-Process -Id $pidValue -Force -ErrorAction SilentlyContinue } }" >> "%STOP_LOG%" 2>&1
echo Stopping AGV service executables...
for %%P in (
  agv-native-feedback-adapter.exe
  agv-config-web.exe
  agv-no-car-feedback-simulator.exe
  agv-opentcs-dashboard.exe
  local-mqtt-broker.exe
) do taskkill /F /IM %%P >> "%STOP_LOG%" 2>&1
echo Stopping suite command windows...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$titles=@('AGV Suite','openTCS Kernel','openTCS Kernel Control Center','openTCS Model Editor','openTCS Operations Desk');" ^
  "$cmds=Get-Process cmd -ErrorAction SilentlyContinue | Where-Object { $title=$_.MainWindowTitle; $title -and ($titles | Where-Object { $title.Contains($_) }) };" ^
  "foreach($p in $cmds){ Write-Host ('Stopping cmd window pid=' + $p.Id + ' title=' + $p.MainWindowTitle); Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }" >> "%STOP_LOG%" 2>&1
echo Stopping openTCS Java processes in this suite directory...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$base=(Resolve-Path '%BASE%').Path;" ^
  "try { $procs=Get-WmiObject Win32_Process -ErrorAction Stop | Where-Object {($_.Name -eq 'java.exe' -or $_.Name -eq 'javaw.exe') -and $_.CommandLine -and $_.CommandLine.Contains($base)} } catch { $procs=@() };" ^
  "foreach($p in $procs){ Write-Host ('Stopping suite java pid=' + $p.ProcessId); Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue }" >> "%STOP_LOG%" 2>&1
echo [%date% %time%] Stop requested. >> "%STOP_LOG%"
echo Done.
pause
'@

$readme = @'
# openTCS AGV Suite

## Start

Double click `start-all.bat`.

Default ports:
- openTCS HTTP: `http://127.0.0.1:55200`
- RCS: `http://127.0.0.1:8090`
- Config UI: `http://127.0.0.1:8091`
- Dashboard: `http://127.0.0.1:8092`
- Local MQTT broker: `127.0.0.1:1883`

No Python or Java installation is required. The package includes the Python service executables and the openTCS Java runtime.

## Real AGV

If the real AGV provides its own MQTT broker, connect the PC to the AGV WiFi first, then change `mqtt_uri` on the Config UI: `http://127.0.0.1:8091/`.

Example:

```text
mqtt://192.168.10.1:1883/
```

`start-all.bat` reads `adapter\config\runtime_config.json` and automatically passes the MQTT broker, command topic, event topic, and point mapping to RCS.

If `mqtt_uri` is `127.0.0.1` or `localhost`, the packaged local broker is started automatically.
If `mqtt_uri` is a robot IP, the packaged local broker is not started.

Adapter `point_id_map` format:

```text
nav:AGV_POINT_ID=openTCS_POINT_NAME
```

RCS command mapping is generated from `point_id_map` when `start-all.bat` runs.
You can still override it before startup with `RCS_POINT_ID_MAP`:

```text
openTCS_POINT_NAME=AGV_POINT_ID
```

Example:

```bat
set RCS_POINT_ID_MAP=AGV_E_01=1,AGV_E_04=4
start-all.bat
```

## Stop

Double click `stop-all.bat`.

## Logs

Main logs:

```text
logs\suite-start-*.log
logs\suite-stop.log
logs\rcs.out.log
logs\rcs.err.log
logs\dashboard.out.log
logs\dashboard.err.log
logs\local-mqtt-broker.out.log
logs\local-mqtt-broker.err.log
```

Adapter and config web logs:

```text
adapter\logs\
```

openTCS also writes logs under its own module directories.
'@

Write-Utf8NoBom -Path (Join-Path $SuiteDir "start-all.bat") -Content $startAll
Write-Utf8NoBom -Path (Join-Path $SuiteDir "start-suite.ps1") -Content $startSuite
Write-Utf8NoBom -Path (Join-Path $SuiteDir "stop-all.bat") -Content $stopAll
Write-Utf8NoBom -Path (Join-Path $SuiteDir "README.md") -Content $readme

if (Test-Path -LiteralPath $ZipPath) {
  Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -Path (Join-Path $SuiteDir "*") -DestinationPath $ZipPath -Force
Write-Host "Suite directory: $SuiteDir"
Write-Host "Suite zip:       $ZipPath"
