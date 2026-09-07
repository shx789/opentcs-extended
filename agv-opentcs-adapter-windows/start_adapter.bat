@echo off
setlocal EnableDelayedExpansion
cd /d %~dp0
if not exist logs mkdir logs
set "APP_CMD=python -u bin\agv_native_feedback_adapter.py"
if exist agv-native-feedback-adapter.exe (
  set "APP_CMD=agv-native-feedback-adapter.exe"
)
if defined PYTHON_EXE if not exist agv-native-feedback-adapter.exe set "APP_CMD=""%PYTHON_EXE%"" -u bin\agv_native_feedback_adapter.py"
start "AGV Adapter" /min /D "%CD%" cmd /c "!APP_CMD! ^> logs\agv_native_feedback_adapter.out.log 2^> logs\agv_native_feedback_adapter.err.log"
echo adapter started
