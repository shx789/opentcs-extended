@echo off
setlocal
cd /d %~dp0
if not exist logs mkdir logs
if exist agv-config-web.exe (
  start "agv-config-web" /b agv-config-web.exe 1>>logs\agv_config_web.log 2>>&1
) else (
  if defined PYTHON_EXE (
    set "PY_CMD=%PYTHON_EXE%"
  ) else (
    set "PY_CMD=python"
  )
  start "agv-config-web" /b %PY_CMD% -u bin\agv_config_web.py 1>>logs\agv_config_web.log 2>>&1
)
echo config web started at http://127.0.0.1:8091
