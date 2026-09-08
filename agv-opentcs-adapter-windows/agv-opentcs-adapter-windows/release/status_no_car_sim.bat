@echo off
setlocal
cd /d %~dp0
if exist logs\agv_no_car_feedback_simulator_status.json (
  type logs\agv_no_car_feedback_simulator_status.json
) else (
  echo status file not found: logs\agv_no_car_feedback_simulator_status.json
)
