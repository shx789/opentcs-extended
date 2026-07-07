#!/usr/bin/env python3
import asyncio
import json
import os
import signal
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

try:
    from amqtt.client import MQTTClient
except Exception as exc:
    MQTTClient = None
    MQTT_IMPORT_ERROR = exc
else:
    MQTT_IMPORT_ERROR = None

if getattr(sys, "frozen", False):
    ROOT_DIR = Path(sys.executable).resolve().parent
else:
    ROOT_DIR = Path(__file__).resolve().parents[1]
RUNTIME_CONFIG_FILE = Path(
    os.environ.get(
        "AGV_ADAPTER_RUNTIME_CONFIG_FILE",
        str(ROOT_DIR / "config" / "runtime_config.json"),
    )
)
DEFAULT_POINT_FILE = ROOT_DIR / "config" / "interest_point.sample.json"
STATUS_FILE = Path(
    os.environ.get(
        "AGV_NO_CAR_STATUS_FILE",
        str(ROOT_DIR / "logs" / "agv_no_car_feedback_simulator_status.json"),
    )
)
START_DELAY_SEC = float(os.environ.get("AGV_NO_CAR_START_DELAY_SEC", "0.2"))
SUCCESS_DELAY_SEC = float(os.environ.get("AGV_NO_CAR_SUCCESS_DELAY_SEC", "2.0"))
FAIL_IF_POINT_MISSING = os.environ.get("AGV_NO_CAR_FAIL_IF_POINT_MISSING", "false").lower() in {
    "1",
    "true",
    "yes",
    "on",
}

STOP = False


def resolve_path(raw_path: str) -> Path:
    path = Path(raw_path).expanduser()
    if path.is_absolute():
        return path
    if RUNTIME_CONFIG_FILE.exists():
        return (RUNTIME_CONFIG_FILE.parent / path).resolve()
    return (ROOT_DIR / path).resolve()


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def write_status(status: Dict[str, Any]) -> None:
    STATUS_FILE.parent.mkdir(parents=True, exist_ok=True)
    temp_file = STATUS_FILE.with_suffix(STATUS_FILE.suffix + ".tmp")
    temp_file.write_text(json.dumps(status, ensure_ascii=False, indent=2))
    temp_file.replace(STATUS_FILE)


def load_runtime_config() -> Dict[str, Any]:
    if not RUNTIME_CONFIG_FILE.exists():
        return {}
    return json.loads(RUNTIME_CONFIG_FILE.read_text())


def load_interest_points(point_file: Path) -> Dict[int, Dict[str, float]]:
    if not point_file.exists():
        return {}
    raw = json.loads(point_file.read_text())
    result: Dict[int, Dict[str, float]] = {}
    for index, point in enumerate(raw.get("point") or []):
        try:
            result[index] = {
                "x": float(point.get("x", 0.0)),
                "y": float(point.get("y", 0.0)),
                "yaw": float(point.get("z", 0.0)),
            }
        except (TypeError, ValueError):
            continue
    return result


def feedback_payload(
    point_id: int,
    status: str,
    point_pose: Optional[Dict[str, float]],
    current_pose: Optional[Dict[str, float]],
) -> Dict[str, Any]:
    goal_pose = point_pose or {"x": 0.0, "y": 0.0, "yaw": 0.0}
    now_pose = current_pose or {"x": 0.0, "y": 0.0, "yaw": 0.0}
    return {
        "cmd_type": "task_feedback",
        "type": "nav",
        "id": point_id,
        "dir": "",
        "status": status,
        "goal_pose": goal_pose,
        "now_pose": now_pose,
    }


async def publish_json(client: MQTTClient, topic: str, payload: Dict[str, Any]) -> None:
    await client.publish(topic, json.dumps(payload, ensure_ascii=False).encode(), qos=0)


async def delayed_publish(
    client: MQTTClient,
    topic: str,
    payload: Dict[str, Any],
    delay_sec: float,
    status: Dict[str, Any],
) -> None:
    await asyncio.sleep(delay_sec)
    if STOP:
        return
    await publish_json(client, topic, payload)
    status["publishedCount"] = status.get("publishedCount", 0) + 1
    status["lastPublishedAt"] = now_iso()
    status["lastPublishedTopic"] = topic
    status["lastPublishedPayload"] = payload
    write_status(status)


def stop(*_: Any) -> None:
    global STOP
    STOP = True


async def run() -> None:
    if MQTTClient is None:
        raise RuntimeError(f"amqtt is not importable: {MQTT_IMPORT_ERROR!r}")

    config = load_runtime_config()
    mqtt_uri = str(config.get("mqtt_uri") or "mqtt://127.0.0.1:1883/")
    command_topic = str(config.get("command_topic") or "robot_control")
    source_topic = str(config.get("source_topic") or "task_feedback")
    point_file = resolve_path(str(config.get("agv_point_file") or str(DEFAULT_POINT_FILE)))
    client_id = f"agv-no-car-feedback-sim-{os.getpid()}"

    status: Dict[str, Any] = {
        "running": True,
        "startedAt": now_iso(),
        "mqttUri": mqtt_uri,
        "commandTopic": command_topic,
        "sourceTopic": source_topic,
        "pointFile": str(point_file),
        "clientId": client_id,
        "receivedCount": 0,
        "publishedCount": 0,
        "startDelaySec": START_DELAY_SEC,
        "successDelaySec": SUCCESS_DELAY_SEC,
    }
    write_status(status)

    client = MQTTClient(client_id=client_id)
    last_pose: Optional[Dict[str, float]] = None
    pending_success_task: Optional[asyncio.Task] = None

    await client.connect(mqtt_uri, cleansession=True)
    await client.subscribe([(command_topic, 0)])
    status["connected"] = True
    status["connectedAt"] = now_iso()
    write_status(status)

    deliver_task: Optional[asyncio.Task] = asyncio.create_task(client.deliver_message())
    try:
        while not STOP:
            if deliver_task is None:
                deliver_task = asyncio.create_task(client.deliver_message())
            done, _ = await asyncio.wait({deliver_task}, timeout=1.0)
            if not done:
                continue
            message = deliver_task.result()
            deliver_task = asyncio.create_task(client.deliver_message())
            packet = message.publish_packet
            raw = packet.payload.data.decode(errors="replace")
            status["receivedCount"] += 1
            status["lastCommandAt"] = now_iso()
            status["lastCommandRaw"] = raw
            try:
                payload = json.loads(raw)
            except Exception as exc:
                status["lastError"] = repr(exc)
                write_status(status)
                continue

            if payload.get("cmd_type") != "interest_point_control":
                status["lastIgnoredReason"] = "cmd_type is not interest_point_control"
                write_status(status)
                continue

            command = str(payload.get("cmd") or "").strip().lower()
            point_id = int(payload.get("id", 0))
            interest_points = load_interest_points(point_file)
            point_pose = interest_points.get(point_id)
            status["lastCommand"] = payload

            if command == "stop":
                if pending_success_task is not None and not pending_success_task.done():
                    pending_success_task.cancel()
                stop_payload = feedback_payload(point_id, "stop", point_pose, last_pose)
                await publish_json(client, source_topic, stop_payload)
                status["publishedCount"] += 1
                status["lastPublishedAt"] = now_iso()
                status["lastPublishedTopic"] = source_topic
                status["lastPublishedPayload"] = stop_payload
                write_status(status)
                continue

            if command != "start":
                status["lastIgnoredReason"] = f"unsupported command: {command}"
                write_status(status)
                continue

            if point_pose is None and FAIL_IF_POINT_MISSING:
                failure_payload = feedback_payload(point_id, "failure", None, last_pose)
                await publish_json(client, source_topic, failure_payload)
                status["publishedCount"] += 1
                status["lastPublishedAt"] = now_iso()
                status["lastPublishedTopic"] = source_topic
                status["lastPublishedPayload"] = failure_payload
                write_status(status)
                continue

            start_payload = feedback_payload(point_id, "start", point_pose, last_pose)
            await delayed_publish(client, source_topic, start_payload, START_DELAY_SEC, status)

            if pending_success_task is not None and not pending_success_task.done():
                pending_success_task.cancel()
            success_payload = feedback_payload(point_id, "success", point_pose, point_pose or last_pose)
            pending_success_task = asyncio.create_task(
                delayed_publish(client, source_topic, success_payload, SUCCESS_DELAY_SEC, status)
            )
            last_pose = point_pose or last_pose
            write_status(status)
    finally:
        if deliver_task is not None and not deliver_task.done():
            deliver_task.cancel()
        status["running"] = False
        status["stoppedAt"] = now_iso()
        write_status(status)
        try:
            await client.disconnect()
        except Exception:
            pass


async def main() -> None:
    while not STOP:
        try:
            await run()
        except asyncio.TimeoutError:
            continue
        except Exception as exc:
            write_status(
                {
                    "running": False,
                    "startedAt": now_iso(),
                    "lastError": repr(exc),
                    "runtimeConfigFile": str(RUNTIME_CONFIG_FILE),
                }
            )
            await asyncio.sleep(2)


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    asyncio.run(main())
