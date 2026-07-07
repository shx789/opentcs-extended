@echo off
setlocal
cd /d %~dp0
if exist validate-runtime-config.exe (
  validate-runtime-config.exe %*
) else (
  if defined PYTHON_EXE (
    set "PY_CMD=%PYTHON_EXE%"
  ) else (
    set "PY_CMD=python"
  )
  %PY_CMD% bin\validate_runtime_config.py %*
)
