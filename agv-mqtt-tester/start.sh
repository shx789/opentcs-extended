#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [[ ! -x ".venv/bin/python" ]]; then
  echo "Creating Python virtual environment..."
  python3 -m venv .venv
fi

echo "Installing dependencies..."
".venv/bin/python" -m pip install -r requirements.txt

echo "Starting AGV MQTT tester..."
".venv/bin/python" app.py --host 127.0.0.1 --port 8093
