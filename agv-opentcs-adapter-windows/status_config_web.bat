@echo off
setlocal
cd /d %~dp0
if exist logs\agv_config_web_status.json (
  type logs\agv_config_web_status.json
) else (
  echo status file not found: logs\agv_config_web_status.json
)
