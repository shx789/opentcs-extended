@echo off
setlocal

set "JAVA_HOME=E:\openjdk-21"
set "KERNEL_WAIT_SECONDS=15"

if not "%~1"=="" set "JAVA_HOME=%~1"
if not "%~2"=="" set "KERNEL_WAIT_SECONDS=%~2"

set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%"

if not exist "%JAVA_HOME%\bin\java.exe" (
  echo [ERROR] java.exe not found: "%JAVA_HOME%\bin\java.exe"
  exit /b 1
)

if not exist "%PROJECT_ROOT%gradlew.bat" (
  echo [ERROR] gradlew.bat not found in project root.
  exit /b 1
)

set "PATH=%JAVA_HOME%\bin;%PATH%"

echo JAVA_HOME = %JAVA_HOME%
echo PROJECT   = %PROJECT_ROOT%
echo.
echo 1/4 Building installDist first (first run may take longer)...
call "%PROJECT_ROOT%gradlew.bat" :opentcs-kernel:installDist :opentcs-kernelcontrolcenter:installDist :opentcs-modeleditor:installDist :opentcs-operationsdesk:installDist
if errorlevel 1 (
  echo [ERROR] Gradle build failed.
  exit /b 1
)

set "KERNEL_DIR=%PROJECT_ROOT%opentcs-kernel\build\install\opentcs-kernel"
set "KCC_DIR=%PROJECT_ROOT%opentcs-kernelcontrolcenter\build\install\opentcs-kernelcontrolcenter"
set "MODEL_DIR=%PROJECT_ROOT%opentcs-modeleditor\build\install\opentcs-modeleditor"
set "OPS_DIR=%PROJECT_ROOT%opentcs-operationsdesk\build\install\opentcs-operationsdesk"

if not exist "%KERNEL_DIR%\startKernel.bat" (
  echo [ERROR] Missing "%KERNEL_DIR%\startKernel.bat"
  exit /b 1
)
if not exist "%KCC_DIR%\startKernelControlCenter.bat" (
  echo [ERROR] Missing "%KCC_DIR%\startKernelControlCenter.bat"
  exit /b 1
)
if not exist "%MODEL_DIR%\startModelEditor.bat" (
  echo [ERROR] Missing "%MODEL_DIR%\startModelEditor.bat"
  exit /b 1
)
if not exist "%OPS_DIR%\startOperationsDesk.bat" (
  echo [ERROR] Missing "%OPS_DIR%\startOperationsDesk.bat"
  exit /b 1
)

echo 2/4 Launching Kernel window...
start "openTCS Kernel" cmd /k "cd /d ""%KERNEL_DIR%"" && set JAVA_HOME=%JAVA_HOME% && set PATH=%JAVA_HOME%\bin;%PATH% && call startKernel.bat"

echo Waiting %KERNEL_WAIT_SECONDS% seconds for Kernel warm-up...
timeout /t %KERNEL_WAIT_SECONDS% /nobreak >nul

echo 3/4 Launching Kernel Control Center...
start "openTCS Kernel Control Center" cmd /k "cd /d ""%KCC_DIR%"" && set JAVA_HOME=%JAVA_HOME% && set PATH=%JAVA_HOME%\bin;%PATH% && call startKernelControlCenter.bat"
timeout /t 2 /nobreak >nul

echo 4/4 Launching Model Editor and Operations Desk...
start "openTCS Model Editor" cmd /k "cd /d ""%MODEL_DIR%"" && set JAVA_HOME=%JAVA_HOME% && set PATH=%JAVA_HOME%\bin;%PATH% && call startModelEditor.bat"
timeout /t 2 /nobreak >nul
start "openTCS Operations Desk" cmd /k "cd /d ""%OPS_DIR%"" && set JAVA_HOME=%JAVA_HOME% && set PATH=%JAVA_HOME%\bin;%PATH% && call startOperationsDesk.bat"

echo.
echo All four processes were launched.
echo Close order suggestion: Model Editor / Operations Desk / KCC, then Kernel.

exit /b 0
