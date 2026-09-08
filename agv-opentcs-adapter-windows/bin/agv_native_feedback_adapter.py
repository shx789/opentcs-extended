#!/usr/bin/env python3
"""Bridge native AGV task_feedback MQTT messages to openTCS vehicle positions.

This adapter is intentionally outside the third-party AGV/ROS codebase. It subscribes to the
AGV's existing MQTT topic `task_feedback`, resolves the AGV feedback to a mapped openTCS point,
and updates the openTCS vehicle position through the HTTP endpoint.
"""
import asyncio
import json
import math
import os
import re
import signal
import time
from datetime import datetime, timezone
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import sys

try:
    from amqtt.client import MQTTClient
    from amqtt.plugins.logging_amqtt import PacketLoggerPlugin as _PacketLoggerPlugin
except Exception as exc:  # pragma: no cover - runtime dependency check
    MQTTClient = None
    MQTT_IMPORT_ERROR = exc
else:
    MQTT_IMPORT_ERROR = None
    _AMQTT_CLIENT_PLUGIN_IMPORTS = (_PacketLoggerPlugin,)

if getattr(sys, 'frozen', False):
    ROOT_DIR = Path(sys.executable).resolve().parent
else:
    ROOT_DIR = Path(__file__).resolve().parents[1]
ROOT = Path(os.environ.get('AGV_ADAPTER_ROOT', str(ROOT_DIR)))
DEFAULT_MAPPING_FILE = str(ROOT / 'config' / 'agv_opentcs_business_topology_draft.json')
DEFAULT_STATUS_FILE = str(ROOT / 'logs' / 'agv_native_feedback_adapter_status.json')
DEFAULT_AGV_POINT_FILE = str(ROOT / 'config' / 'interest_point.sample.json')
RUNTIME_CONFIG_FILE = Path(os.environ.get(
    'AGV_ADAPTER_RUNTIME_CONFIG_FILE',
    str(ROOT / 'config' / 'runtime_config.json'),
))
ADAPTER_CLIENT_ID = f'agv-native-feedback-adapter-{os.getpid()}'


def env_value(name: str, default: str) -> str:
    return os.environ.get(name, default)


def env_bool(name: str, default: bool) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() in {'1', 'true', 'yes', 'y', 'on'}


def env_float(name: str, default: float) -> float:
    try:
        return float(os.environ.get(name, str(default)))
    except (TypeError, ValueError):
        return default


def resolve_path(raw_path: str, config_file: Optional[Path] = None) -> Path:
    path = Path(raw_path).expanduser()
    if path.is_absolute():
        return path
    if config_file is not None:
        return (config_file.parent / path).resolve()
    return (ROOT_DIR / path).resolve()


@dataclass(frozen=True)
class AdapterConfig:
    mapping_file: Path
    open_tcs_base_url: str
    rcs_base_url: str
    mqtt_uri: str
    source_topic: str
    command_topic: str
    forward_command_topic: str
    vehicle_name: str
    agv_id: str
    distance_tolerance_m: float
    publish_rcs_event: bool
    rcs_event_topic_template: str
    status_file: Path
    agv_point_file: Path
    point_id_map: str
    runtime_config_file: Path
    runtime_config_mtime: Optional[float]

    def mqtt_signature(self) -> Tuple[str, str, str, str]:
        return (
            self.mqtt_uri,
            self.source_topic,
            self.command_topic,
            self.forward_command_topic,
        )


def runtime_config_mtime(path: Path = RUNTIME_CONFIG_FILE) -> Optional[float]:
    try:
        return path.stat().st_mtime
    except FileNotFoundError:
        return None


def runtime_value(raw: Dict[str, Any], env_name: str, json_name: str, default: str) -> str:
    value = raw.get(json_name)
    if value is None:
        value = raw.get(env_name)
    if value is None:
        return env_value(env_name, default)
    return str(value)


def runtime_bool(raw: Dict[str, Any], env_name: str, json_name: str, default: bool) -> bool:
    value = raw.get(json_name)
    if value is None:
        value = raw.get(env_name)
    if value is None:
        return env_bool(env_name, default)
    if isinstance(value, bool):
        return value
    return str(value).strip().lower() in {'1', 'true', 'yes', 'y', 'on'}


def runtime_float(raw: Dict[str, Any], env_name: str, json_name: str, default: float) -> float:
    value = raw.get(json_name)
    if value is None:
        value = raw.get(env_name)
    if value is None:
        return env_float(env_name, default)
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def read_runtime_config(path: Path = RUNTIME_CONFIG_FILE) -> Tuple[Dict[str, Any], Optional[float], Optional[str]]:
    mtime = runtime_config_mtime(path)
    if mtime is None:
        return {}, None, None
    try:
        return json.loads(path.read_text(encoding='utf-8')), mtime, None
    except Exception as exc:
        return {}, mtime, repr(exc)


def load_config(previous: Optional[AdapterConfig] = None) -> Tuple[AdapterConfig, Optional[str]]:
    raw, mtime, error = read_runtime_config()
    if error and previous is not None:
        return previous, error
    config_file = RUNTIME_CONFIG_FILE if RUNTIME_CONFIG_FILE.exists() else None
    config = AdapterConfig(
        mapping_file=resolve_path(runtime_value(raw, 'AGV_ADAPTER_MAPPING_FILE', 'mapping_file', DEFAULT_MAPPING_FILE), config_file),
        open_tcs_base_url=runtime_value(raw, 'AGV_ADAPTER_OPENTCS_URL', 'open_tcs_base_url', 'http://127.0.0.1:55200'),
        rcs_base_url=runtime_value(raw, 'AGV_ADAPTER_RCS_URL', 'rcs_base_url', 'http://127.0.0.1:8090'),
        mqtt_uri=runtime_value(raw, 'AGV_ADAPTER_MQTT_URI', 'mqtt_uri', 'mqtt://127.0.0.1:1883/'),
        source_topic=runtime_value(raw, 'AGV_ADAPTER_SOURCE_TOPIC', 'source_topic', 'task_feedback'),
        command_topic=runtime_value(raw, 'AGV_ADAPTER_COMMAND_TOPIC', 'command_topic', 'robot_control'),
        forward_command_topic=runtime_value(raw, 'AGV_ADAPTER_FORWARD_COMMAND_TOPIC', 'forward_command_topic', ''),
        vehicle_name=runtime_value(raw, 'AGV_ADAPTER_VEHICLE_NAME', 'vehicle_name', 'Vehicle-01'),
        agv_id=runtime_value(raw, 'AGV_ADAPTER_AGV_ID', 'agv_id', 'AGV_01'),
        distance_tolerance_m=runtime_float(raw, 'AGV_ADAPTER_DISTANCE_TOLERANCE_M', 'distance_tolerance_m', 1.2),
        publish_rcs_event=runtime_bool(raw, 'AGV_ADAPTER_PUBLISH_RCS_EVENT', 'publish_rcs_event', True),
        rcs_event_topic_template=runtime_value(raw, 'AGV_ADAPTER_RCS_EVENT_TOPIC_TEMPLATE', 'rcs_event_topic_template', 'agv/{agv_id}/task/events'),
        status_file=resolve_path(runtime_value(raw, 'AGV_ADAPTER_STATUS_FILE', 'status_file', DEFAULT_STATUS_FILE), config_file),
        agv_point_file=resolve_path(runtime_value(raw, 'AGV_ADAPTER_AGV_POINT_FILE', 'agv_point_file', DEFAULT_AGV_POINT_FILE), config_file),
        point_id_map=runtime_value(raw, 'AGV_ADAPTER_POINT_ID_MAP', 'point_id_map', '0=WAIT_WEST,1=WAIT_EAST'),
        runtime_config_file=RUNTIME_CONFIG_FILE,
        runtime_config_mtime=mtime,
    )
    return config, error


def apply_config(config: AdapterConfig) -> None:
    global MAPPING_FILE, OPENTCS_BASE_URL, RCS_BASE_URL, MQTT_URI, SOURCE_TOPIC, COMMAND_TOPIC
    global FORWARD_COMMAND_TOPIC, VEHICLE_NAME, AGV_ID, DISTANCE_TOLERANCE_M, PUBLISH_RCS_EVENT
    global RCS_EVENT_TOPIC_TEMPLATE, STATUS_FILE, AGV_POINT_FILE, POINT_ID_MAP
    MAPPING_FILE = config.mapping_file
    OPENTCS_BASE_URL = config.open_tcs_base_url
    RCS_BASE_URL = config.rcs_base_url
    MQTT_URI = config.mqtt_uri
    SOURCE_TOPIC = config.source_topic
    COMMAND_TOPIC = config.command_topic
    FORWARD_COMMAND_TOPIC = config.forward_command_topic
    VEHICLE_NAME = config.vehicle_name
    AGV_ID = config.agv_id
    DISTANCE_TOLERANCE_M = config.distance_tolerance_m
    PUBLISH_RCS_EVENT = config.publish_rcs_event
    RCS_EVENT_TOPIC_TEMPLATE = config.rcs_event_topic_template
    STATUS_FILE = config.status_file
    AGV_POINT_FILE = config.agv_point_file
    POINT_ID_MAP = config.point_id_map


CONFIG, CONFIG_ERROR = load_config()
apply_config(CONFIG)

STOP = False


@dataclass(frozen=True)
class PointMapping:
    name: str
    x: float
    y: float
    yaw: float
    ordinal: int


def now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def load_mappings() -> List[PointMapping]:
    raw = json.loads(MAPPING_FILE.read_text(encoding='utf-8'))
    mappings: List[PointMapping] = []
    for index, point in enumerate(raw.get('points') or []):
        name = point.get('name')
        pose = point.get('ros_pose') or {}
        if not name or 'x' not in pose or 'y' not in pose:
            continue
        mappings.append(PointMapping(
            name=name,
            x=float(pose.get('x', 0)),
            y=float(pose.get('y', 0)),
            yaw=float(pose.get('yaw', 0)),
            ordinal=index,
        ))
    if not mappings:
        raise RuntimeError(f'No usable points found in mapping file: {MAPPING_FILE}')
    return mappings


def nearest_mapping(
    mappings: List[PointMapping],
    x: float,
    y: float,
    tolerance: Optional[float] = None,
) -> Tuple[Optional[PointMapping], Optional[float]]:
    if tolerance is None:
        tolerance = DISTANCE_TOLERANCE_M
    nearest = min(mappings, key=lambda point: math.hypot(point.x - x, point.y - y))
    distance = math.hypot(nearest.x - x, nearest.y - y)
    if distance <= tolerance:
        return nearest, distance
    return None, distance


def normalize_point_map_key(raw_key: str) -> Optional[str]:
    key = raw_key.strip().lower()
    if not key:
        return None
    if ':' in key:
        task_type, point_id = key.split(':', 1)
        task_type = task_type.strip()
        if not task_type:
            return None
    else:
        task_type = '*'
        point_id = key
    try:
        return f'{task_type}:{int(point_id.strip())}'
    except ValueError:
        return None


def load_point_id_map(mappings: List[PointMapping]) -> Dict[str, str]:
    result: Dict[str, str] = {}
    for item in POINT_ID_MAP.split(','):
        if not item.strip() or '=' not in item:
            continue
        left, right = item.split('=', 1)
        key = normalize_point_map_key(left)
        if key and right.strip():
            result[key] = right.strip()
    if result:
        return result
    if not AGV_POINT_FILE.exists():
        return result
    try:
        raw = json.loads(AGV_POINT_FILE.read_text(encoding='utf-8'))
        for index, point in enumerate(raw.get('point') or []):
            x = float(point.get('x'))
            y = float(point.get('y'))
            mapping, _ = nearest_mapping(mappings, x, y)
            if mapping is not None:
                result[f'*:{index}'] = mapping.name
    except Exception:
        return result
    return result


def nested(payload: Dict[str, Any], *keys: str) -> Optional[Any]:
    current: Any = payload
    for key in keys:
        if not isinstance(current, dict) or key not in current:
            return None
        current = current[key]
    return current


def text_value(payload: Dict[str, Any], *keys: str) -> Optional[str]:
    for key in keys:
        value = payload.get(key)
        if value is not None and str(value).strip():
            return str(value).strip()
    return None


def float_value(payload: Dict[str, Any], *keys: str) -> Optional[float]:
    for key in keys:
        value = payload.get(key)
        if value is None:
            continue
        try:
            return float(value)
        except (TypeError, ValueError):
            continue
    return None


def status_to_event(status: str, explicit_event: Optional[str] = None) -> Optional[str]:
    if explicit_event:
        return explicit_event.strip().upper()
    normalized = (status or '').strip().lower()
    if normalized in {'process', 'arrived', 'running'}:
        return 'ARRIVED'
    if normalized in {'success', 'completed', 'complete', 'done', 'finished'}:
        return 'COMPLETED'
    if normalized in {'failure', 'failed', 'timeout', 'error'}:
        return 'FAILED'
    return None


def should_update_position(event_type: Optional[str]) -> bool:
    return event_type in {'ARRIVED', 'COMPLETED', 'COMPLETE', 'FINISHED', 'ARRIVED_TO'}


def should_publish_rcs_event(event_type: Optional[str]) -> bool:
    return event_type in {'ARRIVED', 'COMPLETED', 'COMPLETE', 'FINISHED', 'ARRIVED_TO', 'FAILED'}


def coordinates_from(payload: Dict[str, Any]) -> Tuple[Optional[float], Optional[float], Optional[float]]:
    for parent in ('now_pose', 'goal_pose', 'pose', 'current_pose', 'ros_pose'):
        node = payload.get(parent)
        if isinstance(node, dict):
            x = float_value(node, 'x', 'ros_x', 'rosX')
            y = float_value(node, 'y', 'ros_y', 'rosY')
            yaw = float_value(node, 'yaw', 'theta', 'z', 'ros_yaw', 'rosYaw')
            if x is not None and y is not None:
                return x, y, yaw
    return (
        float_value(payload, 'x', 'ros_x', 'rosX'),
        float_value(payload, 'y', 'ros_y', 'rosY'),
        float_value(payload, 'yaw', 'theta', 'z', 'ros_yaw', 'rosYaw'),
    )


def resolve_point(
    payload: Dict[str, Any],
    mappings: List[PointMapping],
    point_id_map: Dict[str, str],
    command_context: Optional[Dict[str, Any]] = None,
) -> Tuple[Optional[PointMapping], str, Optional[float]]:
    by_name = {mapping.name: mapping for mapping in mappings}
    raw_point = text_value(payload, 'point_id', 'pointId', 'current_point', 'currentPoint')
    if raw_point and raw_point in by_name:
        return by_name[raw_point], 'point_id/name', 0.0

    raw_type = (text_value(payload, 'type') or '').lower()
    status = str(payload.get('status', '')).strip().lower()
    raw_id = payload.get('id')
    point_id = -1
    if raw_id is not None:
        try:
            point_id = int(raw_id)
        except (TypeError, ValueError):
            point_id = -1
        candidate_keys = []
        if raw_type:
            candidate_keys.append(f'{raw_type}:{point_id}')
        candidate_keys.append(f'*:{point_id}')
        for key in candidate_keys:
            mapped_name = point_id_map.get(key)
            if mapped_name in by_name:
                reason = 'configured-type-id-map' if key.startswith(f'{raw_type}:') else 'configured-point-id-map'
                return by_name[mapped_name], reason, 0.0

    if command_context and status in {'success', 'completed', 'complete', 'done', 'finished'}:
        to_point = text_value(command_context, 'to_point', 'toPoint')
        if to_point in by_name:
            return by_name[to_point], 'command-context/to-point', 0.0

    x, y, _ = coordinates_from(payload)
    if x is not None and y is not None:
        mapping, distance = nearest_mapping(mappings, x, y)
        if mapping is not None:
            return mapping, 'nearest-coordinate', distance

    if raw_id is not None and 0 <= point_id < len(mappings):
        return mappings[point_id], 'ordinal-id-fallback', 0.0

    if x is None or y is None:
        return None, 'no-coordinate-or-id-map', None
    _, distance = nearest_mapping(mappings, x, y)
    return None, 'coordinate-out-of-tolerance', distance


def http_json(url: str, method: str = 'GET', body: Optional[Dict[str, Any]] = None, timeout: float = 4) -> Dict[str, Any]:
    data = None
    headers = {}
    if body is not None:
        data = json.dumps(body, ensure_ascii=False).encode()
        headers['Content-Type'] = 'application/json'
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode('utf-8', errors='replace')
            parsed = json.loads(raw) if raw else {}
            if isinstance(parsed, dict):
                parsed.setdefault('_ok', True)
                parsed.setdefault('_status', resp.status)
                return parsed
            return {'_ok': True, '_status': resp.status, 'body': parsed}
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode(errors='replace')
        return {'_ok': False, '_status': exc.code, '_error': raw}
    except Exception as exc:
        return {'_ok': False, '_error': repr(exc)}


def mission_sort_key(record: Dict[str, Any], fallback_index: int = 0) -> Tuple[int, str, int]:
    value = mission_no(record) or ''
    numbers = re.findall(r'\d+', value)
    numeric = int(numbers[-1]) if numbers else 0
    return numeric, value, fallback_index


def normalize_payload(raw: str) -> str:
    try:
        return json.dumps(json.loads(raw), sort_keys=True, separators=(',', ':'))
    except Exception:
        return raw.strip()


def mission_no(record: Dict[str, Any]) -> Optional[str]:
    return text_value(record, 'mission_no', 'missionNo')


def task_no(record: Dict[str, Any]) -> Optional[str]:
    return text_value(record, 'task_no', 'taskNo')


def infer_context_from_command(raw_command: str, command: Dict[str, Any]) -> Dict[str, Any]:
    context: Dict[str, Any] = {
        'command': command,
        'commandRaw': raw_command,
        'commandAt': now_iso(),
    }
    command_id = command.get('id')
    if command_id is not None:
        context['commandPointId'] = command_id
    missions = http_json(f'{RCS_BASE_URL}/api/v1/wcs/agv/missions')
    candidates = missions.get('data') if isinstance(missions, dict) else None
    if not isinstance(candidates, list):
        return context
    normalized_command = normalize_payload(raw_command)
    matches = []
    for index, mission in enumerate(candidates[-50:]):
        mission_id = mission_no(mission)
        if not mission_id:
            continue
        commands = http_json(f'{RCS_BASE_URL}/api/v1/wcs/agv/missions/{mission_id}/commands')
        for item in commands.get('data') or []:
            payload_json = item.get('payloadJson') or item.get('payload_json') or ''
            if normalize_payload(payload_json) == normalized_command:
                matches.append((mission, item, index))
    active_matches = [
        pair for pair in matches
        if (text_value(pair[0], 'rcs_status', 'rcsStatus') or '').upper()
        not in {'DONE', 'FAILED', 'CANCELED', 'CANCELLED'}
    ]
    candidate_pool = active_matches or matches
    selected_pair = max(
        candidate_pool,
        key=lambda pair: mission_sort_key(pair[0], pair[2])
    ) if candidate_pool else None
    if selected_pair:
        mission, item, _ = selected_pair
        mission_id = mission_no(mission)
        context.update({
            'mission_no': item.get('missionNo') or item.get('mission_no') or mission_id,
            'task_no': item.get('taskNo') or item.get('task_no') or task_no(mission),
            'from_point': text_value(mission, 'from_point', 'fromPoint'),
            'to_point': text_value(mission, 'to_point', 'toPoint'),
            'matchedBy': 'rcs-command-payload-active-first',
            'candidateMatches': len(matches),
            'activeMatches': len(active_matches),
        })
        return context
    for mission in reversed(candidates[-10:]):
        status = text_value(mission, 'rcs_status', 'rcsStatus') or ''
        if status.upper() in {'DONE', 'FAILED', 'CANCELED', 'CANCELLED'}:
            continue
        context.update({
            'mission_no': mission_no(mission),
            'task_no': task_no(mission),
            'from_point': text_value(mission, 'from_point', 'fromPoint'),
            'to_point': text_value(mission, 'to_point', 'toPoint'),
            'matchedBy': 'latest-active-mission',
        })
        return context
    return context


def update_open_tcs(point_name: str) -> Dict[str, Any]:
    vehicle_name = urllib.parse.quote(VEHICLE_NAME, safe='')
    primary_response = http_json(
        f'{OPENTCS_BASE_URL}/v1/vehicles/{vehicle_name}/position',
        method='PUT',
        body={'pointName': point_name},
    )
    if primary_response.get('_ok') or primary_response.get('_status') != 404:
        return primary_response

    fallback_response = http_json(
        f'{OPENTCS_BASE_URL}/v1/vehicles/{vehicle_name}/commAdapter/message',
        method='POST',
        body={
            'type': 'tcs:virtualVehicle:setPosition',
            'parameters': [
                {'key': 'position', 'value': point_name},
            ],
        },
    )
    fallback_response['fallbackFrom'] = primary_response
    fallback_response['fallbackMethod'] = 'commAdapter/message:setPosition'
    return fallback_response


def build_rcs_event(
    payload: Dict[str, Any],
    point: PointMapping,
    event_type: str,
    sequence: int,
    command_context: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    x, y, yaw = coordinates_from(payload)
    mission = (text_value(payload, 'mission_no', 'missionNo') or text_value(payload, 'mission', 'missionNo'))
    task = text_value(payload, 'task_no', 'taskNo') or text_value(payload, 'task', 'taskNo')
    if command_context:
        mission = mission or text_value(command_context, 'mission_no', 'missionNo')
        task = task or text_value(command_context, 'task_no', 'taskNo')
    rcs_event_type = 'COMPLETED' if event_type == 'COMPLETED' else event_type
    return {
        'message_id': text_value(payload, 'message_id', 'messageId') or f'adapter-{sequence}',
        'agv_id': text_value(payload, 'agv_id', 'agvId', 'vehicle_id', 'vehicleId') or AGV_ID,
        'event_type': rcs_event_type,
        'mission_no': mission,
        'task_no': task,
        'point_id': point.name,
        'x': x,
        'y': y,
        'yaw': yaw,
        'event_time': now_iso(),
    }


def write_status(status: Dict[str, Any]) -> None:
    STATUS_FILE.parent.mkdir(parents=True, exist_ok=True)
    tmp = STATUS_FILE.with_suffix(STATUS_FILE.suffix + '.tmp')
    tmp.write_text(json.dumps(status, ensure_ascii=False, indent=2))
    tmp.replace(STATUS_FILE)


async def publish(client: MQTTClient, topic: str, payload: Dict[str, Any]) -> None:
    await client.publish(topic, json.dumps(payload, ensure_ascii=False).encode(), qos=0)


async def load_runtime_state(
    status: Dict[str, Any],
    previous_config: Optional[AdapterConfig] = None,
) -> Tuple[AdapterConfig, List[PointMapping], Dict[str, str]]:
    config, error = load_config(previous_config or CONFIG)
    apply_config(config)
    mappings = load_mappings()
    point_id_map = load_point_id_map(mappings)
    status.update({
        'sourceTopic': config.source_topic,
        'commandTopic': config.command_topic,
        'forwardCommandTopic': config.forward_command_topic,
        'mqttUri': config.mqtt_uri,
        'mappingFile': str(config.mapping_file),
        'openTcsBaseUrl': config.open_tcs_base_url,
        'rcsBaseUrl': config.rcs_base_url,
        'vehicleName': config.vehicle_name,
        'points': len(mappings),
        'agvPointFile': str(config.agv_point_file),
        'pointIdMap': point_id_map,
        'distanceToleranceM': config.distance_tolerance_m,
        'runtimeConfigFile': str(config.runtime_config_file),
        'runtimeConfigMtime': config.runtime_config_mtime,
        'runtimeConfigError': error,
    })
    return config, mappings, point_id_map


async def maybe_reload_config(
    config: AdapterConfig,
    mappings: List[PointMapping],
    point_id_map: Dict[str, str],
    status: Dict[str, Any],
) -> Tuple[AdapterConfig, List[PointMapping], Dict[str, str], bool]:
    current_mtime = runtime_config_mtime(config.runtime_config_file)
    if current_mtime == config.runtime_config_mtime:
        return config, mappings, point_id_map, False
    previous_signature = config.mqtt_signature()
    try:
        new_config, new_mappings, new_point_id_map = await load_runtime_state(status, config)
    except Exception as exc:
        status.update({'runtimeConfigError': repr(exc), 'lastReloadErrorAt': now_iso()})
        write_status(status)
        return config, mappings, point_id_map, False
    mqtt_changed = new_config.mqtt_signature() != previous_signature
    status.update({
        'lastReloadAt': now_iso(),
        'lastReloadMqttChanged': mqtt_changed,
        'lastReloadReason': 'runtime-config-file-changed',
    })
    write_status(status)
    return new_config, new_mappings, new_point_id_map, mqtt_changed


async def run() -> None:
    if MQTTClient is None:
        raise RuntimeError(f'amqtt is not importable: {MQTT_IMPORT_ERROR!r}')
    sequence = 0
    status: Dict[str, Any] = {
        'running': True,
        'startedAt': now_iso(),
        'processed': 0,
        'updated': 0,
        'ignored': 0,
    }
    config, mappings, point_id_map = await load_runtime_state(status, CONFIG)
    write_status(status)
    while not STOP:
        config, mappings, point_id_map, _ = await maybe_reload_config(
            config, mappings, point_id_map, status
        )
        client = MQTTClient(client_id=ADAPTER_CLIENT_ID)
        try:
            apply_config(config)
            await client.connect(config.mqtt_uri, cleansession=True)
            subscribe_topics = [(config.source_topic, 0)]
            if config.command_topic and config.command_topic != config.source_topic:
                subscribe_topics.append((config.command_topic, 0))
            await client.subscribe(subscribe_topics)
            status.update({
                'connected': True,
                'lastError': None,
                'connectedAt': now_iso(),
                'mqttUri': config.mqtt_uri,
                'sourceTopic': config.source_topic,
                'commandTopic': config.command_topic,
                'clientId': ADAPTER_CLIENT_ID,
            })
            write_status(status)
            deliver_task: Optional[asyncio.Task] = asyncio.create_task(client.deliver_message())
            while not STOP:
                config, mappings, point_id_map, reconnect = await maybe_reload_config(
                    config, mappings, point_id_map, status
                )
                if reconnect:
                    status.update({'connected': False, 'reconnectingAt': now_iso()})
                    write_status(status)
                    if deliver_task is not None and not deliver_task.done():
                        deliver_task.cancel()
                    break
                if deliver_task is None:
                    deliver_task = asyncio.create_task(client.deliver_message())
                done, _ = await asyncio.wait({deliver_task}, timeout=1.0)
                if not done:
                    continue
                message = deliver_task.result()
                deliver_task = asyncio.create_task(client.deliver_message())
                packet = message.publish_packet
                topic = packet.variable_header.topic_name
                raw = packet.payload.data.decode(errors='replace')
                sequence += 1
                status['processed'] += 1
                try:
                    payload = json.loads(raw)
                    if topic == config.command_topic:
                        status['lastCommand'] = infer_context_from_command(raw, payload)
                        if config.forward_command_topic and config.forward_command_topic != config.command_topic:
                            await publish(client, config.forward_command_topic, payload)
                            status['lastCommand']['forwardedTo'] = config.forward_command_topic
                        write_status(status)
                        continue
                    cmd_type = str(payload.get('cmd_type') or '').strip().lower()
                    if cmd_type and cmd_type != 'task_feedback':
                        status['ignored'] += 1
                        status['lastIgnoredReason'] = 'cmd_type is not task_feedback'
                        write_status(status)
                        continue
                    event_type = status_to_event(
                        str(payload.get('status', '')),
                        text_value(payload, 'event_type', 'eventType'),
                    )
                    point, reason, distance = resolve_point(payload, mappings, point_id_map, status.get('lastCommand'))
                    if point is None or not should_publish_rcs_event(event_type):
                        status['ignored'] += 1
                        status['lastIgnoredReason'] = reason if point is None else f'event {event_type} ignored'
                        status['lastPayload'] = payload
                        status['lastDistance'] = distance
                        write_status(status)
                        continue
                    open_tcs_response = None
                    if should_update_position(event_type):
                        open_tcs_response = update_open_tcs(point.name)
                    rcs_event = None
                    if config.publish_rcs_event:
                        rcs_event = build_rcs_event(payload, point, event_type or 'COMPLETED', sequence, status.get('lastCommand'))
                        await publish(
                            client,
                            config.rcs_event_topic_template.format(agv_id=rcs_event['agv_id']),
                            rcs_event,
                        )
                    status['updated'] += 1
                    status['lastUpdate'] = {
                        'time': now_iso(),
                        'topic': topic,
                        'eventType': event_type,
                        'sourceStatus': payload.get('status'),
                        'sourceType': payload.get('type'),
                        'sourceId': payload.get('id'),
                        'resolvedPoint': point.name,
                        'resolveReason': reason,
                        'distance': distance,
                        'openTcsResponse': open_tcs_response,
                        'publishedRcsEvent': rcs_event,
                    }
                    write_status(status)
                    print(json.dumps(status['lastUpdate'], ensure_ascii=False), flush=True)
                except Exception as exc:
                    status['ignored'] += 1
                    status['lastError'] = repr(exc)
                    status['lastRawPayload'] = raw[:2000]
                    write_status(status)
        except Exception as exc:
            status.update({'connected': False, 'lastError': repr(exc), 'lastErrorAt': now_iso()})
            write_status(status)
            await asyncio.sleep(2)
        finally:
            try:
                await client.disconnect()
            except Exception:
                pass



def stop(*_: Any) -> None:
    global STOP
    STOP = True


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    asyncio.run(run())
