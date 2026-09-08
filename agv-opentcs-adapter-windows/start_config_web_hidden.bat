@echo off
setlocal
cd /d %~dp0
if not exist logs mkdir logs

if exist agv-config-web.exe (
  start "AGV Config Web Hidden" /D "%CD%" agv-config-web.exe
  echo config web started in background at http://127.0.0.1:8091
  exit /b 0
)

set "PYTHONW_CMD=pythonw.exe"
if defined PYTHONW_EXE set "PYTHONW_CMD=%PYTHONW_EXE%"

start "AGV Config Web Hidden" /D "%CD%" "%PYTHONW_CMD%" "bin\run_config_web_hidden.py"
echo config web started in background at http://127.0.0.1:8091
