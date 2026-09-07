@echo off
setlocal
cd /d %~dp0
if exist config\agv_config_web_status.json (
  type config\agv_config_web_status.json
) else (
  echo status file not found: config\agv_config_web_status.json
)
