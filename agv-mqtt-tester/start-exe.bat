@echo off
setlocal
cd /d "%~dp0"

if exist "agv-mqtt-tester.exe" (
  start "" "agv-mqtt-tester.exe" --host 127.0.0.1 --port 8093
) else if exist "bin\agv-mqtt-tester.exe" (
  start "" "bin\agv-mqtt-tester.exe" --host 127.0.0.1 --port 8093
) else (
  echo agv-mqtt-tester.exe not found.
  echo Please use the offline release zip, or run start.bat on a machine with Python.
  pause
  exit /b 1
)

timeout /t 2 >nul
start "" "http://127.0.0.1:8093/"
