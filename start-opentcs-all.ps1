param(
  [string]$JavaHome = "E:\openjdk-21",
  [int]$KernelWaitSeconds = 15
)

$ErrorActionPreference = "Stop"

$projectRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
Set-Location -LiteralPath $projectRoot

$javaExe = Join-Path $JavaHome "bin\java.exe"
if (-not (Test-Path -LiteralPath $javaExe)) {
  throw "java.exe not found: $javaExe"
}

if (-not (Test-Path -LiteralPath (Join-Path $projectRoot "gradlew.bat"))) {
  throw "gradlew.bat not found. Run this script from the openTCS project root."
}

$env:JAVA_HOME = $JavaHome
$env:Path = "$JavaHome\bin;$env:Path"

Write-Host "JAVA_HOME = $env:JAVA_HOME"
Write-Host "Project   = $projectRoot"
Write-Host ""
Write-Host "1/4 Building installDist first (first run may take longer)..."

& .\gradlew.bat :opentcs-kernel:installDist :opentcs-kernelcontrolcenter:installDist :opentcs-modeleditor:installDist :opentcs-operationsdesk:installDist

$kernelDir = Join-Path $projectRoot "opentcs-kernel\build\install\opentcs-kernel"
$kccDir = Join-Path $projectRoot "opentcs-kernelcontrolcenter\build\install\opentcs-kernelcontrolcenter"
$modelDir = Join-Path $projectRoot "opentcs-modeleditor\build\install\opentcs-modeleditor"
$opsDir = Join-Path $projectRoot "opentcs-operationsdesk\build\install\opentcs-operationsdesk"

$kernelBat = Join-Path $kernelDir "startKernel.bat"
$kccBat = Join-Path $kccDir "startKernelControlCenter.bat"
$modelBat = Join-Path $modelDir "startModelEditor.bat"
$opsBat = Join-Path $opsDir "startOperationsDesk.bat"

foreach ($p in @($kernelBat, $kccBat, $modelBat, $opsBat)) {
  if (-not (Test-Path -LiteralPath $p)) {
    throw "Start script missing: $p"
  }
}

function Start-OpenTcsWindow {
  param(
    [string]$Title,
    [string]$WorkingDir,
    [string]$BatFile,
    [string]$JavaHomeValue
  )

  $psTemplate = '$Host.UI.RawUI.WindowTitle = ''{0}''; $env:JAVA_HOME = ''{1}''; $env:Path = $env:JAVA_HOME + ''\bin;'' + $env:Path; Set-Location -LiteralPath ''{2}''; & ''.\{3}'''
  $psCommand = $psTemplate -f $Title, $JavaHomeValue, $WorkingDir, $BatFile

  Start-Process -FilePath "powershell.exe" -ArgumentList "-NoExit", "-Command", $psCommand
}

Write-Host "2/4 Launching Kernel window..."
Start-OpenTcsWindow -Title "openTCS Kernel" -WorkingDir $kernelDir -BatFile "startKernel.bat" -JavaHomeValue $JavaHome

Write-Host "Waiting $KernelWaitSeconds seconds for Kernel warm-up..."
Start-Sleep -Seconds $KernelWaitSeconds

Write-Host "3/4 Launching Kernel Control Center..."
Start-OpenTcsWindow -Title "openTCS Kernel Control Center" -WorkingDir $kccDir -BatFile "startKernelControlCenter.bat" -JavaHomeValue $JavaHome
Start-Sleep -Seconds 2

Write-Host "4/4 Launching Model Editor and Operations Desk..."
Start-OpenTcsWindow -Title "openTCS Model Editor" -WorkingDir $modelDir -BatFile "startModelEditor.bat" -JavaHomeValue $JavaHome
Start-Sleep -Seconds 2
Start-OpenTcsWindow -Title "openTCS Operations Desk" -WorkingDir $opsDir -BatFile "startOperationsDesk.bat" -JavaHomeValue $JavaHome

Write-Host ""
Write-Host "All four processes were launched. Close Model Editor/Operations Desk/KCC first, then Kernel."
