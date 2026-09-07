#!/usr/bin/env python3
"""Run the config web server without a console window and keep logs on disk."""
import sys
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
LOG_DIR = ROOT_DIR / "logs"
LOG_DIR.mkdir(parents=True, exist_ok=True)

sys.stdout = (LOG_DIR / "agv_config_web.out.log").open("a", encoding="utf-8", buffering=1)
sys.stderr = (LOG_DIR / "agv_config_web.err.log").open("a", encoding="utf-8", buffering=1)

from agv_config_web import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main())
