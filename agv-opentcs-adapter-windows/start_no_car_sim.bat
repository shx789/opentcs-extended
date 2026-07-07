@echo off
setlocal
cd /d %~dp0
if not exist logs mkdir logs
if exist agv-no-car-feedback-simulator.exe (
  start "agv-no-car-feedback-simulator" /b agv-no-car-feedback-simulator.exe 1>>logs\agv_no_car_feedback_simulator.log 2>>&1
) else (
  if defined PYTHON_EXE (
    set "PY_CMD=%PYTHON_EXE%"
  ) else (
    set "PY_CMD=python"
  )
  start "agv-no-car-feedback-simulator" /b %PY_CMD% -u bin\agv_no_car_feedback_simulator.py 1>>logs\agv_no_car_feedback_simulator.log 2>>&1
)
echo no-car simulator started
