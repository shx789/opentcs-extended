@echo off
setlocal
cd /d "%~dp0"

if not exist "agv-lift-control-tester.exe" (
  echo agv-lift-control-tester.exe not found.
  pause
  exit /b 1
)

start "AGV Lift Control Tester" "agv-lift-control-tester.exe" --host 127.0.0.1 --port 8093
timeout /t 2 >nul
start "" "http://127.0.0.1:8093/"

