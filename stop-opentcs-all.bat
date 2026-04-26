@echo off
setlocal

echo Stopping openTCS Java processes...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$classes=@('org.opentcs.kernel.RunKernel','org.opentcs.kernelcontrolcenter.RunKernelControlCenter','org.opentcs.modeleditor.RunModelEditor','org.opentcs.operationsdesk.RunOperationsDesk');" ^
  "$procs=Get-CimInstance Win32_Process | Where-Object {($_.Name -eq 'java.exe' -or $_.Name -eq 'javaw.exe') -and $_.CommandLine};" ^
  "foreach($p in $procs){ foreach($c in $classes){ if($p.CommandLine -like ('*'+$c+'*')){ try{ Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop; Write-Host ('Stopped PID '+$p.ProcessId+' ('+$c+')') } catch { Write-Host ('Failed PID '+$p.ProcessId+': '+$_.Exception.Message) } ; break } } }"

echo Closing helper windows with title prefix "openTCS"...
taskkill /F /FI "IMAGENAME eq cmd.exe" /FI "WINDOWTITLE eq openTCS*" >nul 2>&1
taskkill /F /FI "IMAGENAME eq powershell.exe" /FI "WINDOWTITLE eq openTCS*" >nul 2>&1

echo Done.
exit /b 0
