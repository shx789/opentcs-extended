@echo off
echo === OpenTCS 完整启动（含 Model Editor）===

cd /d E:\workspace\OpenTCS\opentcs

echo [1/4] 启动 Kernel...
start powershell -NoExit -Command "cd 'E:\workspace\OpenTCS\opentcs'; Write-Host '[Kernel] 启动中...' -ForegroundColor Green; .\gradlew.bat :opentcs-kernel:run"

timeout /t 8 /nobreak >nul

echo [2/4] 启动 Model Editor（地图编辑器）...
start powershell -NoExit -Command "cd 'E:\workspace\OpenTCS\opentcs'; Write-Host '[ModelEditor] 启动中...' -ForegroundColor Magenta; .\gradlew.bat :opentcs-modeleditor:run"

timeout /t 3 /nobreak >nul

echo [3/4] 启动 OperationsDesk（操作台）...
start powershell -NoExit -Command "cd 'E:\workspace\OpenTCS\opentcs'; Write-Host '[OperationsDesk] 启动中...' -ForegroundColor Cyan; .\gradlew.bat :opentcs-operationsdesk:run"

timeout /t 2 /nobreak >nul

echo [4/4] 启动 KernelControlCenter（控制中心）...
start powershell -NoExit -Command "cd 'E:\workspace\OpenTCS\opentcs'; Write-Host '[ControlCenter] 启动中...' -ForegroundColor Yellow; .\gradlew.bat :opentcs-kernelcontrolcenter:run"

echo.
echo === 全部启动完成 ===
pause