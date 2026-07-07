param(
  [string]$NssmExe = ".\\tools\\nssm.exe",
  [switch]$RemoveConfigWeb,
  [switch]$RemoveNoCarSimulator
)

$ErrorActionPreference = "Continue"
& $NssmExe remove AgvOpenTcsAdapter confirm
if ($RemoveConfigWeb) {
  & $NssmExe remove AgvConfigWeb confirm
}
if ($RemoveNoCarSimulator) {
  & $NssmExe remove AgvNoCarSimulator confirm
}
Write-Host "services removed"
