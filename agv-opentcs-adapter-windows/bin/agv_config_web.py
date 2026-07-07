#!/usr/bin/env python3
import json
import os
import sys
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict
from urllib.parse import parse_qs, urlparse


if getattr(sys, "frozen", False):
    ROOT_DIR = Path(sys.executable).resolve().parent
else:
    ROOT_DIR = Path(__file__).resolve().parents[1]

WEB_DIR = ROOT_DIR / "web"
CONFIG_DIR = ROOT_DIR / "config"
LOG_DIR = ROOT_DIR / "logs"
RUNTIME_CONFIG_FILE = CONFIG_DIR / "runtime_config.json"
ADAPTER_STATUS_FILE = LOG_DIR / "agv_native_feedback_adapter_status.json"
SIM_STATUS_FILE = LOG_DIR / "agv_no_car_feedback_simulator_status.json"
WEB_STATUS_FILE = LOG_DIR / "agv_config_web_status.json"
HOST = os.environ.get("AGV_CONFIG_WEB_HOST", "127.0.0.1")
PORT = int(os.environ.get("AGV_CONFIG_WEB_PORT", "8091"))


def read_json(path: Path, default: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def write_json_atomic(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temp.replace(path)


def runtime_templates() -> Dict[str, Path]:
    return {
        "default": CONFIG_DIR / "runtime_config.example.json",
        "real_car": CONFIG_DIR / "runtime_config.real_car.example.json",
        "no_car": CONFIG_DIR / "runtime_config.no_car.example.json",
    }


def validate_runtime_config_payload(raw: Dict[str, Any]) -> Dict[str, Any]:
    errors = []
    warnings = []

    mqtt_uri = str(raw.get("mqtt_uri") or "").strip()
    open_tcs_base_url = str(raw.get("open_tcs_base_url") or "").strip()
    rcs_base_url = str(raw.get("rcs_base_url") or "").strip()
    source_topic = str(raw.get("source_topic") or "").strip()
    command_topic = str(raw.get("command_topic") or "").strip()
    mapping_file = str(raw.get("mapping_file") or "").strip()
    status_file = str(raw.get("status_file") or "").strip()
    agv_point_file = str(raw.get("agv_point_file") or "").strip()

    if not mqtt_uri.startswith(("mqtt://", "tcp://")):
        errors.append("mqtt_uri 必须以 mqtt:// 或 tcp:// 开头")
    if not open_tcs_base_url.startswith(("http://", "https://")):
        errors.append("open_tcs_base_url 必须以 http:// 或 https:// 开头")
    if not rcs_base_url.startswith(("http://", "https://")):
        errors.append("rcs_base_url 必须以 http:// 或 https:// 开头")
    if not source_topic:
        errors.append("source_topic 不能为空")
    if not command_topic:
        errors.append("command_topic 不能为空")
    if not mapping_file:
        errors.append("mapping_file 不能为空")
    if not status_file:
        errors.append("status_file 不能为空")
    if not agv_point_file:
        warnings.append("agv_point_file 为空，坐标回退可能不可用")

    mapping_path = (CONFIG_DIR / mapping_file).resolve() if mapping_file else None
    if mapping_path and not mapping_path.exists():
        errors.append(f"mapping_file 不存在: {mapping_path}")
    if agv_point_file:
        agv_point_path = (CONFIG_DIR / agv_point_file).resolve()
        if not agv_point_path.exists():
            warnings.append(f"agv_point_file 不存在: {agv_point_path}")

    return {
        "ok": not errors,
        "errors": errors,
        "warnings": warnings,
    }


def update_web_status(last_action: str, extra: Dict[str, Any] | None = None) -> None:
    payload = {
        "running": True,
        "host": HOST,
        "port": PORT,
        "rootDir": str(ROOT_DIR),
        "runtimeConfigFile": str(RUNTIME_CONFIG_FILE),
        "lastAction": last_action,
    }
    if extra:
        payload.update(extra)
    write_json_atomic(WEB_STATUS_FILE, payload)


class Handler(BaseHTTPRequestHandler):
    server_version = "AgvConfigWeb/1.0"

    def log_message(self, format: str, *args: Any) -> None:
        return

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/api/config":
            return self.respond_json(
                200,
                {
                    "config": read_json(RUNTIME_CONFIG_FILE, {}),
                    "validation": validate_runtime_config_payload(read_json(RUNTIME_CONFIG_FILE, {})),
                    "templates": list(runtime_templates().keys()),
                },
            )
        if parsed.path == "/api/status":
            return self.respond_json(
                200,
                {
                    "adapter": read_json(ADAPTER_STATUS_FILE, {}),
                    "simulator": read_json(SIM_STATUS_FILE, {}),
                    "web": read_json(WEB_STATUS_FILE, {}),
                },
            )
        if parsed.path == "/api/template":
            query = parse_qs(parsed.query)
            name = (query.get("name") or ["default"])[0]
            template_file = runtime_templates().get(name)
            if template_file is None:
                return self.respond_json(404, {"error": f"template not found: {name}"})
            return self.respond_json(200, {"name": name, "config": read_json(template_file, {})})
        self.serve_static(parsed.path)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path != "/api/config":
            return self.respond_json(404, {"error": "not found"})
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(content_length).decode("utf-8")
            payload = json.loads(body)
        except Exception as exc:
            return self.respond_json(400, {"error": f"invalid json: {exc!r}"})
        if not isinstance(payload, dict):
            return self.respond_json(400, {"error": "payload must be object"})
        validation = validate_runtime_config_payload(payload)
        if not validation["ok"]:
            return self.respond_json(400, {"error": "validation failed", "validation": validation})
        write_json_atomic(RUNTIME_CONFIG_FILE, payload)
        update_web_status("config_saved", {"lastSavedConfig": payload})
        return self.respond_json(200, {"ok": True, "validation": validation})

    def respond_json(self, status: int, payload: Dict[str, Any]) -> None:
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def serve_static(self, path: str) -> None:
        candidate = "index.html" if path in {"", "/"} else path.lstrip("/")
        file_path = (WEB_DIR / candidate).resolve()
        if not str(file_path).startswith(str(WEB_DIR.resolve())) or not file_path.exists() or file_path.is_dir():
            self.send_error(HTTPStatus.NOT_FOUND, "Not Found")
            return
        content_type = "text/plain; charset=utf-8"
        if file_path.suffix == ".html":
            content_type = "text/html; charset=utf-8"
        elif file_path.suffix == ".css":
            content_type = "text/css; charset=utf-8"
        elif file_path.suffix == ".js":
            content_type = "application/javascript; charset=utf-8"
        raw = file_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


def main() -> int:
    WEB_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    update_web_status("started")
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        update_web_status("stopped")
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
