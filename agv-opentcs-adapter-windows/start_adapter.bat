@echo off
setlocal
cd /d %~dp0
if not exist logs mkdir logs
if exist agv-native-feedback-adapter.exe (
  start "agv-native-feedback-adapter" /b agv-native-feedback-adapter.exe 1>>logs\agv_native_feedback_adapter.log 2>>&1
) else (
  if defined PYTHON_EXE (
    set "PY_CMD=%PYTHON_EXE%"
  ) else (
    set "PY_CMD=python"
  )
  start "agv-native-feedback-adapter" /b %PY_CMD% -u bin\agv_native_feedback_adapter.py 1>>logs\agv_native_feedback_adapter.log 2>>&1
)
echo adapter started
