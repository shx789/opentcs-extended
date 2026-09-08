@echo off
setlocal
cd /d %~dp0
if not exist config\generated mkdir config\generated
if exist generate-map-topology.exe (
  generate-map-topology.exe %*
) else (
  if defined PYTHON_EXE (
    set "PY_CMD=%PYTHON_EXE%"
  ) else (
    set "PY_CMD=python"
  )
  %PY_CMD% bin\generate_map_topology.py %*
)
