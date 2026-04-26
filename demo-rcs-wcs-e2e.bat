@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%\opentcs-rcs-integration-sample\scripts\run-rcs-wcs-e2e.ps1" -ProjectRoot "%PROJECT_ROOT%" %*
exit /b %errorlevel%
