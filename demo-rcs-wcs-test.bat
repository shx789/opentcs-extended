@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

cd /d "%PROJECT_ROOT%"
echo [1/2] Running RCS integration tests (includes simulated WCS callback server)...
call .\gradlew :opentcs-rcs-integration-sample:test
if errorlevel 1 (
  echo [ERROR] Test demo failed.
  exit /b 1
)

echo [2/2] Done.
echo HTML report:
echo   %PROJECT_ROOT%\opentcs-rcs-integration-sample\build\reports\tests\test\index.html
exit /b 0

