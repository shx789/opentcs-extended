@echo off
setlocal EnableDelayedExpansion
cd /d %~dp0
if not exist logs mkdir logs
set "APP_CMD=python -u bin\agv_no_car_feedback_simulator.py"
if exist agv-no-car-feedback-simulator.exe (
  set "APP_CMD=agv-no-car-feedback-simulator.exe"
)
if defined PYTHON_EXE if not exist agv-no-car-feedback-simulator.exe set "APP_CMD=""%PYTHON_EXE%"" -u bin\agv_no_car_feedback_simulator.py"
start "AGV No-Car Simulator" /min /D "%CD%" cmd /c "!APP_CMD! ^> logs\agv_no_car_feedback_simulator.out.log 2^> logs\agv_no_car_feedback_simulator.err.log"
echo no-car simulator started
