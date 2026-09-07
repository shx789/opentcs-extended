@echo off
setlocal
for /f "tokens=5" %%P in ('netstat -ano ^| findstr /R /C:":8091 .*LISTENING"') do (
  echo stopping config web process %%P
  taskkill /PID %%P /F
)
