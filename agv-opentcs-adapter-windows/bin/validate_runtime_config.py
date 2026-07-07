#!/usr/bin/env python3
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple
from urllib.parse import urlparse


if getattr(sys, "frozen", False):
    ROOT_DIR = Path(sys.executable).resolve().parent
else:
    ROOT_DIR = Path(__file__).resolve().parents[1]

RUNTIME_CONFIG_FILE = ROOT_DIR / "config" / "runtime_config.json"


def resolve_path(raw_path: str, config_file: Path) -> Path:
    path = Path(raw_path).expanduser()
    if path.is_absolute():
        return path
    return (config_file.parent / path).resolve()


def validate_url(name: str, value: str, allowed_schemes: Tuple[str, ...]) -> List[str]:
    errors: List[str] = []
    parsed = urlparse(value)
    if parsed.scheme not in allowed_schemes:
        errors.append(f"{name}: invalid scheme `{parsed.scheme}`")
    if not parsed.netloc:
        errors.append(f"{name}: missing host/port")
    return errors


def validate_runtime_config(config_file: Path) -> int:
    errors: List[str] = []
    warnings: List[str] = []
    try:
      raw: Dict[str, Any] = json.loads(config_file.read_text(encoding="utf-8"))
    except FileNotFoundError:
        print(f"[ERROR] config not found: {config_file}")
        return 2
    except Exception as exc:
        print(f"[ERROR] failed to parse config: {exc!r}")
        return 2

    mqtt_uri = str(raw.get("mqtt_uri") or "")
    open_tcs_base_url = str(raw.get("open_tcs_base_url") or "")
    rcs_base_url = str(raw.get("rcs_base_url") or "")
    source_topic = str(raw.get("source_topic") or "").strip()
    command_topic = str(raw.get("command_topic") or "").strip()
    mapping_file = resolve_path(str(raw.get("mapping_file") or ""), config_file)
    status_file = resolve_path(str(raw.get("status_file") or ""), config_file)
    agv_point_file = resolve_path(str(raw.get("agv_point_file") or ""), config_file)

    errors.extend(validate_url("mqtt_uri", mqtt_uri, ("mqtt", "tcp")))
    errors.extend(validate_url("open_tcs_base_url", open_tcs_base_url, ("http", "https")))
    errors.extend(validate_url("rcs_base_url", rcs_base_url, ("http", "https")))

    if not source_topic:
        errors.append("source_topic: empty")
    if not command_topic:
        errors.append("command_topic: empty")
    if not mapping_file.exists():
        errors.append(f"mapping_file not found: {mapping_file}")
    if status_file.parent.exists() is False:
        warnings.append(f"status_file parent will be created at runtime: {status_file.parent}")
    if not agv_point_file.exists():
        warnings.append(f"agv_point_file not found: {agv_point_file}")

    print(f"config_file={config_file}")
    print(f"mqtt_uri={mqtt_uri}")
    print(f"open_tcs_base_url={open_tcs_base_url}")
    print(f"rcs_base_url={rcs_base_url}")
    print(f"mapping_file={mapping_file}")
    print(f"status_file={status_file}")
    print(f"agv_point_file={agv_point_file}")

    for warning in warnings:
        print(f"[WARN] {warning}")
    for error in errors:
        print(f"[ERROR] {error}")

    if errors:
        print("validation=FAILED")
        return 1

    print("validation=OK")
    return 0


def main() -> int:
    config_file = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else RUNTIME_CONFIG_FILE
    return validate_runtime_config(config_file)


if __name__ == "__main__":
    raise SystemExit(main())
