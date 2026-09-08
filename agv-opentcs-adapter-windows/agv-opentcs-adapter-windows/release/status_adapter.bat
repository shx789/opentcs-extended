@echo off
setlocal
cd /d %~dp0
if exist logs\agv_native_feedback_adapter_status.json (
  type logs\agv_native_feedback_adapter_status.json
) else (
  echo status file not found: logs\agv_native_feedback_adapter_status.json
)
