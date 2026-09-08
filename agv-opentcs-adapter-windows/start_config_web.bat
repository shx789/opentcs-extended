@echo off
setlocal EnableDelayedExpansion
cd /d %~dp0
if not exist logs mkdir logs
set "APP_CMD=python -u bin\agv_config_web.py"
if exist agv-config-web.exe (
  set "APP_CMD=agv-config-web.exe"
)
if defined PYTHON_EXE if not exist agv-config-web.exe set "APP_CMD=""%PYTHON_EXE%"" -u bin\agv_config_web.py"
start "AGV Config Web" /min /D "%CD%" cmd /c "!APP_CMD! ^> logs\agv_config_web.out.log 2^> logs\agv_config_web.err.log"
echo config web started at http://127.0.0.1:8091
