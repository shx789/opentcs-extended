$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (!(Test-Path ".venv\Scripts\python.exe")) {
  Write-Host "Creating Python virtual environment..."
  py -3 -m venv .venv
}

Write-Host "Installing dependencies..."
.\.venv\Scripts\python.exe -m pip install -r requirements.txt

Write-Host "Starting AGV MQTT tester..."
.\.venv\Scripts\python.exe app.py --host 127.0.0.1 --port 8093
