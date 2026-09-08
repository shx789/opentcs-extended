#!/usr/bin/env python3
import json
import os
import socket
import subprocess
import sys
from datetime import datetime
from email.parser import BytesParser
from email.policy import default as email_policy
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple
from urllib.parse import parse_qs, urlparse
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from xml.sax.saxutils import escape


if getattr(sys, "frozen", False):
    ROOT_DIR = Path(sys.executable).resolve().parent
else:
    ROOT_DIR = Path(__file__).resolve().parents[1]

WEB_DIR = ROOT_DIR / "web"
CONFIG_DIR = ROOT_DIR / "config"
LOG_DIR = ROOT_DIR / "logs"
GENERATED_DIR = CONFIG_DIR / "generated"
UPLOAD_DIR = CONFIG_DIR / "uploaded-maps"
RUNTIME_CONFIG_FILE = CONFIG_DIR / "runtime_config.json"
ADAPTER_STATUS_FILE = LOG_DIR / "agv_native_feedback_adapter_status.json"
SIM_STATUS_FILE = LOG_DIR / "agv_no_car_feedback_simulator_status.json"
WEB_STATUS_FILE = CONFIG_DIR / "agv_config_web_status.json"
TOPOLOGY_STATUS_FILE = GENERATED_DIR / "generation_status.json"
BUSINESS_MAPPING_FILE_NAME = "agv_opentcs_business_mapping.json"
BUSINESS_PLANT_JSON_FILE_NAME = "opentcs_plant_model_business.json"
TOPOLOGY_VERSION_DIR = CONFIG_DIR / "topology-versions"
ALLOWED_UPLOAD_SUFFIXES = {".yaml", ".yml", ".pgm"}
HOST = os.environ.get("AGV_CONFIG_WEB_HOST", "127.0.0.1")
PORT = int(os.environ.get("AGV_CONFIG_WEB_PORT", "8091"))
TASK_FEEDBACK_TYPES = ("track", "nav", "charge", "point")


def read_json(path: Path, default: Any) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def write_json_atomic(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    temp = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    try:
        temp.write_text(raw, encoding="utf-8")
        temp.replace(path)
    except PermissionError:
        path.write_text(raw, encoding="utf-8")


def runtime_templates() -> Dict[str, Path]:
    return {
        "default": CONFIG_DIR / "runtime_config.example.json",
        "real_car": CONFIG_DIR / "runtime_config.real_car.example.json",
        "no_car": CONFIG_DIR / "runtime_config.no_car.example.json",
    }


def resolve_root_path(raw_path: str, default: Path) -> Path:
    if not raw_path:
        return default.resolve()
    path = Path(raw_path).expanduser()
    if path.is_absolute():
        return path.resolve()
    return (ROOT_DIR / path).resolve()


def topology_status() -> Dict[str, Any]:
    status = read_json(TOPOLOGY_STATUS_FILE, {})
    status.setdefault("outputDir", str(GENERATED_DIR))
    status.setdefault("previewFile", str(GENERATED_DIR / "preview.png"))
    return status


def config_relative_path(path: Path) -> str:
    resolved = path.resolve()
    try:
        return str(resolved.relative_to(CONFIG_DIR.resolve()))
    except ValueError:
        return str(resolved)


def current_topology_file(payload: Optional[Dict[str, Any]], key: str, default_name: str) -> Path:
    status = topology_status()
    raw = ""
    if payload:
        raw = str(payload.get(key) or "").strip()
    raw = raw or str(status.get(key) or "").strip()
    if raw:
        return resolve_root_path(raw, GENERATED_DIR / default_name)
    return GENERATED_DIR / default_name


def is_valid_point_name(name: str) -> bool:
    if not name or len(name) > 80:
        return False
    return all(ch.isalnum() or ch in {"_", "-", "."} for ch in name)


def load_topology_points(payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    mapping_file = current_topology_file(payload, "mappingFile", "agv_opentcs_mapping.json")
    plant_json_file = current_topology_file(payload, "plantJsonFile", "opentcs_plant_model_candidate.json")
    if not mapping_file.exists():
     return {"ok": False, "error": f"mapping file 不存在：{mapping_file}"}
    mapping = read_json(mapping_file, {})
    points = mapping.get("points") or []
    paths = mapping.get("paths") or []
    return {
        "ok": True,
        "mappingFile": str(mapping_file),
        "plantJsonFile": str(plant_json_file),
        "businessMappingFile": str(mapping_file.parent / BUSINESS_MAPPING_FILE_NAME),
        "businessPlantJsonFile": str(mapping_file.parent / BUSINESS_PLANT_JSON_FILE_NAME),
        "points": points,
        "paths": paths,
        "pointCount": len(points),
        "pathCount": len(paths),
    }


def rename_path_name(path: Dict[str, Any], src: str, dest: str, reverse_suffix: bool = False) -> str:
    suffix = "_REV" if reverse_suffix else ""
    original = str(path.get("name") or "")
    if original.endswith("_REV"):
        suffix = "_REV"
    return f"PATH_{src}_TO_{dest}{suffix}"


def point_position_mm(point: Dict[str, Any]) -> Dict[str, int]:
    position = point.get("opentcs_position") or {}
    ros_pose = point.get("ros_pose") or {}
    x = position.get("x")
    y = position.get("y")
    if x is None:
        x = round(float(ros_pose.get("x") or 0) * 1000)
    if y is None:
        y = round(float(ros_pose.get("y") or 0) * 1000)
    return {"x": int(x), "y": int(y), "z": int(position.get("z") or 0)}


def distance_mm(a: Dict[str, Any], b: Dict[str, Any]) -> int:
    apos = point_position_mm(a)
    bpos = point_position_mm(b)
    return max(int(round(((apos["x"] - bpos["x"]) ** 2 + (apos["y"] - bpos["y"]) ** 2) ** 0.5)), 1)


def normalize_edited_topology(payload: Dict[str, Any]) -> Tuple[Dict[str, Any], Dict[str, Any]]:
    mapping_file = current_topology_file(payload, "mappingFile", "agv_opentcs_mapping.json")
    base_mapping = read_json(mapping_file, {}) if mapping_file.exists() else {}
    points_by_original = {str(point.get("name") or ""): point for point in base_mapping.get("points") or []}
    edited_points = payload.get("points") or []
    edited_paths = payload.get("paths") or []
    points: List[Dict[str, Any]] = []
    for item in edited_points:
        if not isinstance(item, dict):
            continue
        original_name = str(item.get("originalName") or item.get("name") or "").strip()
        base = dict(points_by_original.get(original_name) or {})
        name = str(item.get("name") or base.get("name") or "").strip()
        ros_pose = dict(base.get("ros_pose") or {})
        ros_x = item.get("rosX", item.get("ros_x", ros_pose.get("x", 0)))
        ros_y = item.get("rosY", item.get("ros_y", ros_pose.get("y", 0)))
        yaw = item.get("yaw", ros_pose.get("yaw", 0.0))
        try:
            ros_pose = {"x": round(float(ros_x), 4), "y": round(float(ros_y), 4), "yaw": round(float(yaw), 4)}
        except Exception:
            ros_pose = {"x": 0.0, "y": 0.0, "yaw": 0.0}
        pixel = dict(base.get("pixel") or {})
        try:
            pixel = {
                "x": int(round(float(item.get("pixelX", item.get("pixel_x", pixel.get("x", 0)))))),
                "y": int(round(float(item.get("pixelY", item.get("pixel_y", pixel.get("y", 0)))))),
            }
        except Exception:
            pixel = {"x": 0, "y": 0}
        point = {
            **base,
            "name": name,
            "kind": str(item.get("kind") or base.get("kind") or "manual").strip(),
            "pixel": pixel,
            "ros_pose": ros_pose,
            "opentcs_position": {
                "x": int(round(ros_pose["x"] * 1000)),
                "y": int(round(ros_pose["y"] * 1000)),
                "z": int((base.get("opentcs_position") or {}).get("z") or 0),
            },
        }
        points.append(point)

    point_by_name = {point["name"]: point for point in points}
    paths: List[Dict[str, Any]] = []
    seen: Set[Tuple[str, str]] = set()
    for item in edited_paths:
        if not isinstance(item, dict):
            continue
        src = str(item.get("srcPointName") or item.get("src") or "").strip()
        dest = str(item.get("destPointName") or item.get("dest") or "").strip()
        if not src or not dest:
            continue
        bidirectional = bool(item.get("bidirectional", True))
        key = (src, dest, "2" if bidirectional else "1")
        if key in seen:
            continue
        seen.add(key)
        src_point = point_by_name.get(src)
        dest_point = point_by_name.get(dest)
        length = item.get("length_mm")
        try:
            length_mm = int(float(length)) if length not in (None, "") else 0
        except Exception:
            length_mm = 0
        if length_mm <= 0 and src_point and dest_point:
            length_mm = distance_mm(src_point, dest_point)
        paths.append({
            "name": str(item.get("name") or f"PATH_{src}_TO_{dest}").strip(),
            "srcPointName": src,
            "destPointName": dest,
            "length_mm": max(length_mm, 1),
            "bidirectional": bidirectional,
        })

    mapping = dict(base_mapping)
    mapping["method"] = "manual edited topology"
    mapping["points"] = points
    mapping["paths"] = paths
    mapping["source_mapping_file"] = str(mapping_file)
    mapping["edited_at"] = datetime.now().isoformat(timespec="seconds")
    mapping["revision_note"] = str(payload.get("revisionNote") or "").strip()
    return mapping, base_mapping


def validate_topology_mapping(mapping: Dict[str, Any]) -> Dict[str, Any]:
    errors: List[str] = []
    warnings: List[str] = []
    points = mapping.get("points") or []
    paths = mapping.get("paths") or []
    names = [str(point.get("name") or "").strip() for point in points]
    if not points:
        errors.append("at least one point is required")
    if not paths:
        warnings.append("当前没有路径，openTCS 无法派车行驶")
    for name in names:
        if not is_valid_point_name(name):
            errors.append(f"点位名无效：{name or '<空>'}")
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        errors.append(f"点位名重复：{', '.join(duplicates)}")
    name_set = set(names)
    directed_edges: Dict[str, Set[str]] = {name: set() for name in name_set}
    weak_edges: Dict[str, Set[str]] = {name: set() for name in name_set}
    seen_edges: Set[Tuple[str, str]] = set()
    for path in paths:
        src = str(path.get("srcPointName") or "").strip()
        dest = str(path.get("destPointName") or "").strip()
        if src == dest:
            errors.append(f"路径不能连接到自身：{src}")
            continue
        if src not in name_set or dest not in name_set:
            errors.append(f"路径引用了不存在的点: {src} -> {dest}")
            continue
        directed_key = (src, dest)
        if directed_key in seen_edges:
            warnings.append(f"重复路径: {src} -> {dest}")
        seen_edges.add(directed_key)
        directed_edges.setdefault(src, set()).add(dest)
        if path.get("bidirectional", True):
            directed_edges.setdefault(dest, set()).add(src)
        weak_edges.setdefault(src, set()).add(dest)
        weak_edges.setdefault(dest, set()).add(src)
    isolated = sorted(name for name in name_set if not weak_edges.get(name))
    if isolated:
        warnings.append(f"孤立点位: {', '.join(isolated[:20])}")
    weak_reachable = reachable_from(names[0], weak_edges) if names else set()
    if names and len(weak_reachable) < len(name_set):
        missing = sorted(name_set - weak_reachable)
        warnings.append(f"拓扑不是一个连通图，无法从 {names[0]} 到达: {', '.join(missing[:20])}")
    directed_reachable = reachable_from(names[0], directed_edges) if names else set()
    if names and len(directed_reachable) < len(name_set):
        missing = sorted(name_set - directed_reachable)
        warnings.append(f"按单向规则从 {names[0]} 不可达：{', '.join(missing[:20])}")
    return {
        "ok": not errors,
        "errors": errors,
        "warnings": warnings,
        "pointCount": len(points),
        "pathCount": len(paths),
        "isolatedPoints": isolated,
    }


def reachable_from(start: str, graph: Dict[str, Set[str]]) -> Set[str]:
    visited = set()
    stack = [start]
    while stack:
        current = stack.pop()
        if current in visited:
            continue
        visited.add(current)
        stack.extend(sorted(graph.get(current, set()) - visited))
    return visited


def build_plant_model(mapping: Dict[str, Any], model_name: str) -> Dict[str, Any]:
    plant_points = []
    for point in mapping.get("points") or []:
        pos = point_position_mm(point)
        ros_pose = point.get("ros_pose") or {}
        plant_points.append({
            "name": point["name"],
            "position": {"x": pos["x"], "y": pos["y"], "z": 0},
            "vehicleOrientationAngle": ros_pose.get("yaw", 0.0),
            "type": "HALT_POSITION",
            "layout": {"position": {"x": pos["x"], "y": pos["y"]}, "labelOffset": {"x": 0, "y": 0}, "layerId": 0},
            "vehicleEnvelopes": [],
            "maxVehicleBoundingBox": {"length": 1000, "width": 800, "height": 600, "referenceOffset": {"x": 0, "y": 0}},
            "properties": [
                {"name": "ros.x", "value": str(ros_pose.get("x", 0))},
                {"name": "ros.y", "value": str(ros_pose.get("y", 0))},
                {"name": "ros.yaw", "value": str(ros_pose.get("yaw", 0.0))},
                {"name": "mapping.kind", "value": point.get("kind", "")},
            ],
        })
    location_types = [{
        "name": "AGV_TEST_TRANSFER_STATION",
        "allowedOperations": ["PICK", "DROP"],
        "allowedPeripheralOperations": [],
        "layout": {"locationRepresentation": "LOAD_TRANSFER_GENERIC"},
        "properties": [],
    }]
    locations = []
    for point in plant_points:
        pos = point["position"]
        locations.append({
            "name": f"LOC_{point['name']}",
            "typeName": "AGV_TEST_TRANSFER_STATION",
            "position": {"x": pos["x"], "y": pos["y"], "z": pos.get("z", 0)},
            "links": [{"pointName": point["name"], "allowedOperations": ["PICK", "DROP"]}],
            "locked": False,
            "layout": {
                "position": {"x": pos["x"], "y": pos["y"]},
                "labelOffset": {"x": 0, "y": 0},
                "locationRepresentation": "LOAD_TRANSFER_GENERIC",
                "layerId": 0,
            },
            "properties": [{"name": "mapping.kind", "value": "test-transfer-station"}],
        })
    plant_paths = []
    for path in mapping.get("paths") or []:
        pairs = [("", path["srcPointName"], path["destPointName"])]
        if path.get("bidirectional", True):
            pairs.append(("_REV", path["destPointName"], path["srcPointName"]))
        for suffix, src, dest in pairs:
            plant_paths.append({
                "name": f"PATH_{src}_TO_{dest}{suffix}",
                "srcPointName": src,
                "destPointName": dest,
                "length": max(int(path.get("length_mm") or 1), 1),
                "maxVelocity": 1000,
                "maxReverseVelocity": 1000,
                "peripheralOperations": [],
                "locked": False,
                "layout": {"connectionType": "DIRECT", "controlPoints": [], "layerId": 0},
                "vehicleEnvelopes": [],
                "properties": [],
            })
    return {
        "name": model_name,
        "points": plant_points,
        "paths": plant_paths,
        "locationTypes": location_types,
        "locations": locations,
        "blocks": [],
        "vehicles": [default_vehicle("Vehicle-01")],
        "visualLayout": {
            "name": "VLayout",
            "scaleX": 50.0,
            "scaleY": 50.0,
            "layers": [{"id": 0, "ordinal": 0, "visible": True, "name": "Default layer", "groupId": 0}],
            "layerGroups": [{"id": 0, "name": "Default layer group", "visible": True}],
            "properties": [],
        },
        "properties": [],
    }


def plant_model_xml(plant: Dict[str, Any]) -> str:
    lines = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>']
    lines.append(f'<model version="7.0.0" name="{xml_attr(plant["name"])}">')
    outgoing_by_point: Dict[str, List[str]] = {point["name"]: [] for point in plant["points"]}
    for path in plant["paths"]:
        outgoing_by_point.setdefault(path["srcPointName"], []).append(path["name"])
    for point in plant["points"]:
        pos = point["position"]
        lines.append(
            f'    <point name="{xml_attr(point["name"])}" positionX="{pos["x"]}" '
            f'positionY="{pos["y"]}" positionZ="0" vehicleOrientationAngle="{point["vehicleOrientationAngle"]}" '
            f'type="{xml_attr(point["type"])}">'
        )
        box = point["maxVehicleBoundingBox"]
        ref = box["referenceOffset"]
        lines.append(
            f'        <maxVehicleBoundingBox length="{box["length"]}" width="{box["width"]}" '
            f'height="{box["height"]}" referenceOffsetX="{ref["x"]}" referenceOffsetY="{ref["y"]}"/>'
        )
        for path_name in outgoing_by_point.get(point["name"], []):
            lines.append(f'        <outgoingPath name="{xml_attr(path_name)}"/>')
        for prop in point.get("properties", []):
            lines.append(f'        <property name="{xml_attr(prop["name"])}" value="{xml_attr(prop["value"])}"/>')
        layout = point["layout"]
        offset = layout["labelOffset"]
        lines.append(
            f'        <pointLayout labelOffsetX="{offset["x"]}" labelOffsetY="{offset["y"]}" '
            f'layerId="{layout["layerId"]}"/>'
        )
        lines.append("    </point>")
    for path in plant["paths"]:
        lines.append(
            f'    <path name="{xml_attr(path["name"])}" sourcePoint="{xml_attr(path["srcPointName"])}" '
            f'destinationPoint="{xml_attr(path["destPointName"])}" length="{path["length"]}" '
            f'maxVelocity="{path["maxVelocity"]}" maxReverseVelocity="{path["maxReverseVelocity"]}" '
            f'locked="{str(path["locked"]).lower()}">'
        )
        layout = path["layout"]
        lines.append(
            f'        <pathLayout connectionType="{xml_attr(layout["connectionType"])}" '
            f'layerId="{layout["layerId"]}"/>'
        )
        lines.append("    </path>")
    for vehicle in plant["vehicles"]:
        lines.append(
            f'    <vehicle name="{xml_attr(vehicle["name"])}" energyLevelCritical="{vehicle["energyLevelCritical"]}" '
            f'energyLevelGood="{vehicle["energyLevelGood"]}" '
            f'energyLevelFullyRecharged="{vehicle["energyLevelFullyRecharged"]}" '
            f'energyLevelSufficientlyRecharged="{vehicle["energyLevelSufficientlyRecharged"]}" '
            f'maxVelocity="{vehicle["maxVelocity"]}" maxReverseVelocity="{vehicle["maxReverseVelocity"]}">'
        )
        box = vehicle["boundingBox"]
        ref = box["referenceOffset"]
        lines.append(
            f'        <boundingBox length="{box["length"]}" width="{box["width"]}" '
            f'height="{box["height"]}" referenceOffsetX="{ref["x"]}" referenceOffsetY="{ref["y"]}"/>'
        )
        color = vehicle.get("layout", {}).get("routeColor", "#00FF00")
        lines.append(f'        <vehicleLayout color="{xml_attr(color)}"/>')
        lines.append("    </vehicle>")
    visual = plant["visualLayout"]
    lines.append(f'    <visualLayout name="{xml_attr(visual["name"])}" scaleX="{visual["scaleX"]}" scaleY="{visual["scaleY"]}">')
    for layer in visual["layers"]:
        lines.append(
            f'        <layer id="{layer["id"]}" ordinal="{layer["ordinal"]}" '
            f'visible="{str(layer["visible"]).lower()}" name="{xml_attr(layer["name"])}" '
            f'groupId="{layer["groupId"]}"/>'
        )
    for group in visual["layerGroups"]:
        lines.append(
            f'        <layerGroup id="{group["id"]}" name="{xml_attr(group["name"])}" '
            f'visible="{str(group["visible"]).lower()}"/>'
        )
    lines.append("    </visualLayout>")
    lines.append("</model>")
    return "\n".join(lines) + "\n"


def xml_attr(value: Any) -> str:
    return escape(str(value), {'"': "&quot;"})


def save_edited_topology(payload: Dict[str, Any]) -> Dict[str, Any]:
    mapping, _ = normalize_edited_topology(payload)
    validation = validate_topology_mapping(mapping)
    if not validation["ok"]:
        return {"ok": False, "error": "拓扑校验失败", "validation": validation}

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    output_dir = TOPOLOGY_VERSION_DIR / timestamp
    output_dir.mkdir(parents=True, exist_ok=False)
    model_name = str(payload.get("modelName") or topology_status().get("modelName") or "agv-map-edited").strip()
    plant = build_plant_model(mapping, model_name)
    mapping_file = output_dir / "agv_opentcs_mapping.json"
    plant_json_file = output_dir / "opentcs_plant_model_candidate.json"
    plant_xml_file = output_dir / "plant_model.xml"
    business_mapping_file = output_dir / BUSINESS_MAPPING_FILE_NAME
    business_plant_file = output_dir / BUSINESS_PLANT_JSON_FILE_NAME
    status_file = output_dir / "generation_status.json"
    write_json_atomic(mapping_file, mapping)
    write_json_atomic(plant_json_file, plant)
    plant_xml_file.write_text(plant_model_xml(plant), encoding="utf-8")
    write_json_atomic(business_mapping_file, mapping)
    write_json_atomic(business_plant_file, plant)
    status = {
        "ok": True,
        "mode": "manual-edited",
        "modelName": model_name,
        "outputDir": str(output_dir),
        "mappingFile": str(mapping_file),
        "plantJsonFile": str(plant_json_file),
        "plantXmlFile": str(plant_xml_file),
        "businessMappingFile": str(business_mapping_file),
        "businessPlantJsonFile": str(business_plant_file),
        "points": len(mapping.get("points") or []),
        "paths": len(mapping.get("paths") or []),
        "plantPathsIncludingReverse": len(plant.get("paths") or []),
        "validation": validation,
        "savedAt": datetime.now().isoformat(timespec="seconds"),
    }
    write_json_atomic(status_file, status)
    write_json_atomic(TOPOLOGY_STATUS_FILE, status)
    update_web_status("edited_topology_saved", {"lastEditedTopology": status})
    return status


def validate_edited_topology(payload: Dict[str, Any]) -> Dict[str, Any]:
    mapping, _ = normalize_edited_topology(payload)
    validation = validate_topology_mapping(mapping)
    return {"ok": validation["ok"], "validation": validation}


def rename_location_name(location: Dict[str, Any], name_map: Dict[str, str]) -> str:
    old_name = str(location.get("name") or "")
    prefix = "LOC_"
    if old_name.startswith(prefix):
        point_name = old_name[len(prefix):]
        return prefix + name_map.get(point_name, point_name)
    return old_name

def save_business_topology(payload: Dict[str, Any]) -> Dict[str, Any]:
    mapping_file = current_topology_file(payload, "mappingFile", "agv_opentcs_mapping.json")
    plant_json_file = current_topology_file(payload, "plantJsonFile", "opentcs_plant_model_candidate.json")
    if not mapping_file.exists():
        return {"ok": False, "error": f"mapping file 不存在：{mapping_file}"}
    if not plant_json_file.exists():
        return {"ok": False, "error": f"plant model JSON 不存在：{plant_json_file}"}

    renames = payload.get("renames") or {}
    if not isinstance(renames, dict):
        return {"ok": False, "error": "renames must be an object"}

    mapping = read_json(mapping_file, {})
    plant = read_json(plant_json_file, {})
    original_names = [str(point.get("name") or "") for point in mapping.get("points") or []]
    name_map: Dict[str, str] = {}
    for original in original_names:
        requested = str(renames.get(original) or original).strip()
        if not is_valid_point_name(requested):
            return {"ok": False, "error": f"点位名无效：{requested or original}"}
        name_map[original] = requested
    duplicates = sorted({name for name in name_map.values() if list(name_map.values()).count(name) > 1})
    if duplicates:
        return {"ok": False, "error": f"点位名重复：{', '.join(duplicates)}"}

    business_mapping = dict(mapping)
    business_points = []
    for point in mapping.get("points") or []:
        next_point = dict(point)
        old_name = str(point.get("name") or "")
        next_point["name"] = name_map.get(old_name, old_name)
        business_points.append(next_point)
    business_mapping["points"] = business_points
    business_paths = []
    for path in mapping.get("paths") or []:
        next_path = dict(path)
        src = name_map.get(str(path.get("srcPointName") or ""), str(path.get("srcPointName") or ""))
        dest = name_map.get(str(path.get("destPointName") or ""), str(path.get("destPointName") or ""))
        next_path["srcPointName"] = src
        next_path["destPointName"] = dest
        next_path["name"] = rename_path_name(path, src, dest)
        business_paths.append(next_path)
    business_mapping["paths"] = business_paths
    business_mapping["business_point_renames"] = name_map
    business_mapping["source_mapping_file"] = str(mapping_file)

    business_plant = dict(plant)
    plant_points = []
    for point in plant.get("points") or []:
        next_point = dict(point)
        old_name = str(point.get("name") or "")
        next_point["name"] = name_map.get(old_name, old_name)
        plant_points.append(next_point)
    business_plant["points"] = plant_points
    plant_paths = []
    for path in plant.get("paths") or []:
        next_path = dict(path)
        old_src = str(path.get("srcPointName") or "")
        old_dest = str(path.get("destPointName") or "")
        src = name_map.get(old_src, old_src)
        dest = name_map.get(old_dest, old_dest)
        next_path["srcPointName"] = src
        next_path["destPointName"] = dest
        next_path["name"] = rename_path_name(path, src, dest)
        plant_paths.append(next_path)
    business_plant["paths"] = plant_paths

    # Keep location names and point links consistent with renamed points.
    business_locations = []
    for location in plant.get("locations") or []:
        next_location = dict(location)
        next_location["name"] = rename_location_name(location, name_map)
        next_links = []
        for link in location.get("links") or []:
            next_link = dict(link)
            old_point_name = str(link.get("pointName") or "")
            next_link["pointName"] = name_map.get(old_point_name, old_point_name)
            next_links.append(next_link)
        next_location["links"] = next_links
        business_locations.append(next_location)
    business_plant["locations"] = business_locations

    business_mapping_file = mapping_file.parent / BUSINESS_MAPPING_FILE_NAME
    business_plant_file = plant_json_file.parent / BUSINESS_PLANT_JSON_FILE_NAME
    write_json_atomic(business_mapping_file, business_mapping)
    write_json_atomic(business_plant_file, business_plant)

    status = topology_status()
    status.update({
        "businessMappingFile": str(business_mapping_file),
        "businessPlantJsonFile": str(business_plant_file),
        "businessPointRenames": name_map,
    })
    write_json_atomic(TOPOLOGY_STATUS_FILE, status)
    update_web_status("business_topology_saved", {
        "lastBusinessTopology": {
            "businessMappingFile": str(business_mapping_file),
            "businessPlantJsonFile": str(business_plant_file),
            "renamedPoints": sum(1 for old, new in name_map.items() if old != new),
        },
    })
    return {
        "ok": True,
        "mappingFile": str(mapping_file),
        "plantJsonFile": str(plant_json_file),
        "businessMappingFile": str(business_mapping_file),
        "businessPlantJsonFile": str(business_plant_file),
        "renamedPoints": sum(1 for old, new in name_map.items() if old != new),
        "points": len(business_mapping.get("points") or []),
        "paths": len(business_mapping.get("paths") or []),
    }


def apply_topology_to_runtime(payload: Dict[str, Any]) -> Dict[str, Any]:
    mapping_file = str(
        payload.get("businessMappingFile")
        or payload.get("mappingFile")
        or topology_status().get("businessMappingFile")
        or topology_status().get("mappingFile")
        or ""
    ).strip()
    if not mapping_file:
        return {"ok": False, "error": "没有可应用的 mapping file"}
    mapping_path = resolve_root_path(mapping_file, GENERATED_DIR / BUSINESS_MAPPING_FILE_NAME)
    if not mapping_path.exists():
        return {"ok": False, "error": f"mapping file 不存在：{mapping_path}"}

    runtime_config = read_json(RUNTIME_CONFIG_FILE, {})
    runtime_config["mapping_file"] = config_relative_path(mapping_path)
    for payload_key, config_key in {
        "openTcsBaseUrl": "open_tcs_base_url",
        "rcsBaseUrl": "rcs_base_url",
        "mqttUri": "mqtt_uri",
        "sourceTopic": "source_topic",
        "commandTopic": "command_topic",
        "vehicleName": "vehicle_name",
        "agvId": "agv_id",
        "pointIdMap": "point_id_map",
    }.items():
        value = payload.get(payload_key)
        if value not in (None, ""):
            runtime_config[config_key] = value
    validation = validate_runtime_config_payload(runtime_config)
    if not validation["ok"]:
        return {"ok": False, "error": "runtime apply validation failed", "validation": validation}
    write_json_atomic(RUNTIME_CONFIG_FILE, runtime_config)
    update_web_status("topology_applied_to_runtime", {
        "lastAppliedRuntimeMapping": str(mapping_path),
        "lastAppliedRuntimeConfig": runtime_config,
    })
    return {
        "ok": True,
        "runtimeConfigFile": str(RUNTIME_CONFIG_FILE),
        "mappingFile": str(mapping_path),
        "runtimeMappingFile": runtime_config["mapping_file"],
        "validation": validation,
        "config": runtime_config,
    }


def test_tcp_endpoint(url_value: str, timeout: float = 3) -> Dict[str, Any]:
    parsed = urlparse(url_value)
    host = parsed.hostname
    port = parsed.port
    if not host or port is None:
        return {"ok": False, "error": f"地址缺少 host/port: {url_value}"}
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return {"ok": True, "host": host, "port": port}
    except Exception as exc:
        return {"ok": False, "host": host, "port": port, "error": repr(exc)}


def test_http_endpoint(url_value: str, path: str, timeout: int = 4) -> Dict[str, Any]:
    base_url = str(url_value or "").rstrip("/")
    if not base_url.startswith(("http://", "https://")):
        return {"ok": False, "error": f"地址必须以 http:// 或 https:// 开头：{url_value}"}
    request = Request(f"{base_url}{path}", method="GET")
    try:
        with urlopen(request, timeout=timeout) as response:
            body = response.read(1000).decode("utf-8", errors="replace")
            return {"ok": 200 <= response.status < 300, "status": response.status, "body": body}
    except Exception as exc:
        return {"ok": False, "error": repr(exc)}


def connection_test(payload: Dict[str, Any]) -> Dict[str, Any]:
    runtime_config = read_json(RUNTIME_CONFIG_FILE, {})
    mqtt_uri = str(payload.get("mqttUri") or runtime_config.get("mqtt_uri") or "").strip()
    open_tcs_base_url = str(payload.get("openTcsBaseUrl") or runtime_config.get("open_tcs_base_url") or "").strip()
    rcs_base_url = str(payload.get("rcsBaseUrl") or runtime_config.get("rcs_base_url") or "").strip()
    return {
        "ok": True,
        "mqtt": test_tcp_endpoint(mqtt_uri) if mqtt_uri else {"ok": False, "error": "mqtt_uri 为空"},
        "openTcs": test_http_endpoint(open_tcs_base_url, "/v1/kernel/version") if open_tcs_base_url else {"ok": False, "error": "open_tcs_base_url 为空"},
        "rcs": test_http_endpoint(rcs_base_url, "/api/v1/wcs/agv/missions") if rcs_base_url else {"ok": False, "error": "rcs_base_url 为空"},
    }


def generate_topology(payload: Dict[str, Any]) -> Dict[str, Any]:
    map_yaml = str(payload.get("mapYaml") or payload.get("map_yaml") or "").strip()
    if not map_yaml:
        return {"ok": False, "error": "mapYaml 不能为空"}
    map_yaml_path = resolve_root_path(map_yaml, ROOT_DIR / map_yaml)
    if not map_yaml_path.exists():
        return {"ok": False, "error": f"map.yaml 不存在：{map_yaml_path}"}

    mode = str(payload.get("mode") or "blackline-skeleton").strip()
    output_dir = resolve_root_path(str(payload.get("outputDir") or ""), GENERATED_DIR)
    output_dir.mkdir(parents=True, exist_ok=True)
    generator_exe = ROOT_DIR / "generate-map-topology.exe"
    if generator_exe.exists():
        command = [str(generator_exe)]
    else:
        command = [
            os.environ.get("PYTHON_EXE") or sys.executable,
            str(ROOT_DIR / "bin" / "generate_map_topology.py"),
        ]
    command.extend([
        "--map-yaml",
        str(map_yaml_path),
        "--output-dir",
        str(output_dir),
        "--mode",
        mode,
        "--json",
    ])
    model_name = str(payload.get("modelName") or payload.get("model_name") or "").strip()
    if model_name:
        command.extend(["--model-name", model_name])
    for json_key, arg_name in {
        "freeThreshold": "--free-threshold",
        "darkThreshold": "--dark-threshold",
        "minComponentArea": "--min-component-area",
        "maxNodes": "--max-nodes",
        "sampleSpacingPx": "--sample-spacing-px",
        "clusterRadiusPx": "--cluster-radius-px",
        "minNodeDistancePx": "--min-node-distance-px",
        "maxNeighborDistancePx": "--max-neighbor-distance-px",
        "maxPathPx": "--max-path-px",
        "houghThreshold": "--hough-threshold",
        "houghMinLinePx": "--hough-min-line-px",
        "houghMaxGapPx": "--hough-max-gap-px",
        "houghMergeAngleDeg": "--hough-merge-angle-deg",
        "houghMergeDistancePx": "--hough-merge-distance-px",
        "houghMergeGapPx": "--hough-merge-gap-px",
        "maxSegments": "--max-segments",
        "loopClosureEdges": "--loop-closure-edges",
        "loopClosureDistancePx": "--loop-closure-distance-px",
        "collinearAngleDeg": "--collinear-angle-deg",
    }.items():
        value = payload.get(json_key)
        if value not in (None, ""):
            command.extend([arg_name, str(value)])

    try:
        completed = subprocess.run(
            command,
            cwd=str(ROOT_DIR),
            text=True,
            capture_output=True,
            timeout=int(os.environ.get("AGV_CONFIG_WEB_GENERATE_TIMEOUT_SEC", "180")),
            check=False,
        )
    except Exception as exc:
        return {"ok": False, "error": repr(exc)}

    stdout = completed.stdout.strip()
    stderr = completed.stderr.strip()
    try:
        result = json.loads(stdout.splitlines()[-1]) if stdout else {}
    except Exception:
        result = {}
    result.setdefault("ok", completed.returncode == 0)
    result["returnCode"] = completed.returncode
    if stdout:
        result["stdout"] = stdout[-4000:]
    if stderr:
        result["stderr"] = stderr[-4000:]
    if completed.returncode != 0 and "error" not in result:
        result["error"] = stderr or stdout or "generate_map_topology failed"
    update_web_status("topology_generated" if result.get("ok") else "topology_generate_failed", {
        "lastTopologyGeneration": result,
    })
    return result


def safe_upload_name(filename: str) -> str:
    name = Path(filename or "").name.strip()
    if not name:
        raise ValueError("uploaded filename cannot be empty")
    suffix = Path(name).suffix.lower()
    if suffix not in ALLOWED_UPLOAD_SUFFIXES:
        raise ValueError(f"不支持的文件类型: {name}")
    return name


def upload_topology_files(handler: BaseHTTPRequestHandler) -> Dict[str, Any]:
    content_type = handler.headers.get("Content-Type", "")
    if "multipart/form-data" not in content_type:
        return {"ok": False, "error": "Content-Type 必须为 multipart/form-data"}

    upload_dir = UPLOAD_DIR / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    upload_dir.mkdir(parents=True, exist_ok=True)
    try:
        content_length = int(handler.headers.get("Content-Length", "0"))
    except ValueError:
        return {"ok": False, "error": "Content-Length 无效"}
    body = handler.rfile.read(content_length)
    message = BytesParser(policy=email_policy).parsebytes(
        b"Content-Type: " + content_type.encode("utf-8") + b"\r\n"
        b"MIME-Version: 1.0\r\n\r\n" + body
    )
    if not message.is_multipart():
        return {"ok": False, "error": "上传请求不是 multipart 内容"}

    saved = []
    map_yaml = None
    for field in message.iter_parts():
        if field.get_param("name", header="content-disposition") != "files":
            continue
        upload_name = field.get_filename()
        if not upload_name:
            continue
        try:
            filename = safe_upload_name(upload_name)
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        target = upload_dir / filename
        data = field.get_payload(decode=True) or b""
        target.write_bytes(data)
        saved.append(str(target))
        if target.suffix.lower() in {".yaml", ".yml"} and map_yaml is None:
            map_yaml = target

    if not saved:
        return {"ok": False, "error": "没有收到上传文件"}
    if map_yaml is None:
        return {
            "ok": False,
            "error": "map.yaml and PGM are required for reliable openTCS coordinates",
            "savedFiles": saved,
            "uploadDir": str(upload_dir),
        }

    update_web_status("topology_files_uploaded", {
        "lastUploadedMap": {
            "uploadDir": str(upload_dir),
            "mapYaml": str(map_yaml),
            "savedFiles": saved,
        },
    })
    return {
        "ok": True,
        "uploadDir": str(upload_dir),
        "mapYaml": str(map_yaml),
        "savedFiles": saved,
    }


def default_vehicle(vehicle_name: str) -> Dict[str, Any]:
    return {
        "name": vehicle_name,
        "boundingBox": {
            "length": 1000,
            "width": 800,
            "height": 600,
            "referenceOffset": {"x": 0, "y": 0},
        },
        "energyLevelCritical": 0,
        "energyLevelGood": 90,
        "energyLevelFullyRecharged": 90,
        "energyLevelSufficientlyRecharged": 0,
        "maxVelocity": 1000,
        "maxReverseVelocity": 1000,
        "layout": {"routeColor": "#00FF00"},
        "properties": [],
    }


def load_topology_to_opentcs(payload: Dict[str, Any]) -> Dict[str, Any]:
    plant_json = str(
        payload.get("businessPlantJsonFile")
        or payload.get("plantJsonFile")
        or payload.get("plant_json_file")
        or topology_status().get("businessPlantJsonFile")
        or topology_status().get("plantJsonFile")
        or ""
    ).strip()
    plant_json_path = resolve_root_path(plant_json, GENERATED_DIR / "opentcs_plant_model_candidate.json")
    if not plant_json_path.exists():
        return {"ok": False, "error": f"plant model JSON 不存在：{plant_json_path}"}

    candidate_path = plant_json_path.parent / "opentcs_plant_model_candidate.json"
    selected_model_reason = "requested"
    if plant_json_path.name == BUSINESS_PLANT_JSON_FILE_NAME and candidate_path.exists():
        if candidate_path.stat().st_mtime > plant_json_path.stat().st_mtime:
            plant_json_path = candidate_path
            selected_model_reason = "business model is older than latest candidate"
        else:
            selected_model_reason = "business model is newer than candidate"
    runtime_config = read_json(RUNTIME_CONFIG_FILE, {})
    open_tcs_base_url = str(
        payload.get("openTcsBaseUrl")
        or payload.get("open_tcs_base_url")
        or runtime_config.get("open_tcs_base_url")
        or "http://127.0.0.1:55200"
    ).rstrip("/")
    for suffix in ("/v1/plantModel", "/v1", "/plantModel"):
        if open_tcs_base_url.endswith(suffix):
            open_tcs_base_url = open_tcs_base_url[: -len(suffix)].rstrip("/")
            break
    vehicle_name = str(
        payload.get("vehicleName")
        or payload.get("vehicle_name")
        or runtime_config.get("vehicle_name")
        or "Vehicle-01"
    ).strip()

    try:
        plant = json.loads(plant_json_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return {"ok": False, "error": f"plant model JSON 解析失败: {exc!r}"}

    vehicles = plant.get("vehicles") or []
    vehicle = dict(vehicles[0]) if vehicles else default_vehicle(vehicle_name)
    vehicle["name"] = vehicle_name
    plant["vehicles"] = [vehicle]

    body = json.dumps(plant, ensure_ascii=False).encode("utf-8")
    health_url = f"{open_tcs_base_url}/v1/kernel/version"
    try:
        with urlopen(Request(health_url, method="GET"), timeout=5) as health_response:
            health_body = health_response.read().decode("utf-8", errors="replace")
            health_status = health_response.status
    except HTTPError as exc:
        health_body = exc.read().decode("utf-8", errors="replace")
        return {"ok": False, "status": exc.code, "url": health_url, "error": f"openTCS health check HTTP {exc.code} {exc.reason}", "body": health_body[:4000], "hint": "Use the openTCS Kernel base URL, for example http://127.0.0.1:55200, without /v1."}
    except URLError as exc:
        return {"ok": False, "url": health_url, "error": f"Cannot connect to openTCS health check: {exc.reason}"}
    except Exception as exc:
        return {"ok": False, "url": health_url, "error": repr(exc)}
    if not 200 <= health_status < 300:
        return {"ok": False, "status": health_status, "url": health_url, "error": f"openTCS health check failed, HTTP {health_status}", "body": health_body[:4000]}
    request_url = f"{open_tcs_base_url}/v1/plantModel"
    request = Request(
        request_url,
        data=body,
        method="PUT",
        headers={
            "Content-Type": "application/json; charset=utf-8",
            "Accept": "application/json",
        },
    )
    try:
        with urlopen(request, timeout=20) as response:
            response_body = response.read().decode("utf-8", errors="replace")
            status = response.status
    except HTTPError as exc:
        response_body = exc.read().decode("utf-8", errors="replace")
        return {
            "ok": False,
            "status": exc.code,
            "url": request_url,
            "error": f"openTCS HTTP {exc.code} {exc.reason}",
            "body": response_body[:4000],
            "hint": "请确认使用 openTCS Kernel 基础地址（例如 http://127.0.0.1:55200，不要填写 /v1），并确认当前模型中没有旧点位或旧车辆引用。",
        }
    except URLError as exc:
        return {
            "ok": False,
            "url": request_url,
            "error": f"无法连接 openTCS: {exc.reason}",
        }
    except Exception as exc:
        return {"ok": False, "url": request_url, "error": repr(exc)}

    update_web_status("topology_loaded_to_opentcs", {
        "lastLoadedPlantModel": {
            "plantJsonFile": str(plant_json_path),
            "openTcsBaseUrl": open_tcs_base_url,
            "vehicleName": vehicle_name,
        "selectedModelReason": selected_model_reason,
            "points": len(plant.get("points") or []),
            "paths": len(plant.get("paths") or []),
            "vehicles": len(plant.get("vehicles") or []),
            "status": status,
        },
    })
    return {
        "ok": 200 <= status < 300,
        "status": status,
        "body": response_body[:2000],
        "plantJsonFile": str(plant_json_path),
        "openTcsBaseUrl": open_tcs_base_url,
        "modelName": plant.get("name"),
        "points": len(plant.get("points") or []),
        "paths": len(plant.get("paths") or []),
        "vehicles": len(plant.get("vehicles") or []),
        "vehicleName": vehicle_name,
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
        errors.append("mqtt_uri must start with mqtt:// or tcp://")
    if not open_tcs_base_url.startswith(("http://", "https://")):
        errors.append("open_tcs_base_url must start with http:// or https://")
    if not rcs_base_url.startswith(("http://", "https://")):
        errors.append("rcs_base_url must start with http:// or https://")
    if not source_topic:
        errors.append("source_topic 不能为空")
    if not command_topic:
        errors.append("command_topic 不能为空")
    if not mapping_file:
        errors.append("mapping_file 不能为空")
    if not status_file:
        errors.append("status_file 不能为空")
    if not agv_point_file:
        warnings.append("agv_point_file is empty; coordinate fallback may be unavailable")

    mapping_path = (CONFIG_DIR / mapping_file).resolve() if mapping_file else None
    if mapping_path and not mapping_path.exists():
        errors.append(f"mapping_file 不存在：{mapping_path}")
    if agv_point_file:
        agv_point_path = (CONFIG_DIR / agv_point_file).resolve()
        if not agv_point_path.exists():
            warnings.append(f"agv_point_file 不存在：{agv_point_path}")

    return {
        "ok": not errors,
        "errors": errors,
        "warnings": warnings,
    }


def runtime_mapping_file(runtime_config: Dict[str, Any]) -> Path:
    raw_mapping = str(runtime_config.get("mapping_file") or "").strip()
    if raw_mapping:
        path = Path(raw_mapping).expanduser()
        if path.is_absolute():
            return path.resolve()
        return (CONFIG_DIR / path).resolve()
    return (GENERATED_DIR / BUSINESS_MAPPING_FILE_NAME).resolve()


def normalize_feedback_types(raw_types: Any) -> List[str]:
    if isinstance(raw_types, str):
        candidates = raw_types.split(",")
    elif isinstance(raw_types, list):
        candidates = raw_types
    else:
        candidates = ["nav", "point"]

    feedback_types: List[str] = []
    seen: Set[str] = set()
    for item in candidates:
        feedback_type = str(item or "").strip().lower()
        if feedback_type not in TASK_FEEDBACK_TYPES or feedback_type in seen:
            continue
        feedback_types.append(feedback_type)
        seen.add(feedback_type)
    return feedback_types or ["nav", "point"]


def generate_point_id_map_payload(
    apply: bool = False,
    raw_types: Any = None,
    start_id: int = 0,
) -> Dict[str, Any]:
    runtime_config = read_json(RUNTIME_CONFIG_FILE, {})
    mapping_file = runtime_mapping_file(runtime_config)
    if not mapping_file.exists():
        fallback = GENERATED_DIR / BUSINESS_MAPPING_FILE_NAME
        if fallback.exists():
            mapping_file = fallback
        else:
            return {"ok": False, "error": f"mapping file not found: {mapping_file}"}
    mapping = read_json(mapping_file, {})
    points = mapping.get("points") or []
    names: List[str] = []
    seen: Set[str] = set()
    for point in points:
        if not isinstance(point, dict):
            continue
        name = str(point.get("name") or "").strip()
        if not name or name in seen:
            continue
        names.append(name)
        seen.add(name)
    if not names:
        return {"ok": False, "error": f"no points found in mapping file: {mapping_file}"}
    feedback_types = normalize_feedback_types(raw_types)
    pairs = []
    for index, name in enumerate(names):
        point_id = start_id + index
        for feedback_type in feedback_types:
            pairs.append(f"{feedback_type}:{point_id}={name}")
    point_id_map = ",".join(pairs)
    result = {
        "ok": True,
        "mappingFile": str(mapping_file),
        "pointCount": len(names),
        "types": feedback_types,
        "startId": start_id,
        "point_id_map": point_id_map,
        "sample": pairs[:12],
        "applied": False,
        "note": "Generated IDs are topology-order IDs. For a real AGV, replace them with the robot's actual task_feedback type/id values.",
    }
    if apply:
        runtime_config["point_id_map"] = point_id_map
        validation = validate_runtime_config_payload(runtime_config)
        if not validation["ok"]:
            return {"ok": False, "error": "generated config validation failed", "validation": validation}
        write_json_atomic(RUNTIME_CONFIG_FILE, runtime_config)
        update_web_status("point_id_map_generated", {
            "lastGeneratedPointIdMap": {
                "mappingFile": str(mapping_file),
                "pointCount": len(names),
                "sample": pairs[:12],
            }
        })
        result["applied"] = True
        result["config"] = runtime_config
        result["validation"] = validation
    return result


def update_web_status(last_action: str, extra: Optional[Dict[str, Any]] = None) -> None:
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
        if parsed.path == "/api/topology":
            return self.respond_json(200, topology_status())
        if parsed.path == "/api/topology/points":
            result = load_topology_points()
            return self.respond_json(200 if result.get("ok") else 404, result)
        if parsed.path == "/api/topology/preview":
            query = parse_qs(parsed.query)
            requested = (query.get("file") or [""])[0]
            file_path = resolve_root_path(requested, GENERATED_DIR / "preview.png")
            return self.serve_generated_file(file_path, "image/png")
        self.serve_static(parsed.path)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/api/topology/upload":
            result = upload_topology_files(self)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path not in {
            "/api/config",
            "/api/point-id-map/generate",
            "/api/topology/generate",
            "/api/topology/load-opentcs",
            "/api/topology/business",
            "/api/topology/validate-edits",
            "/api/topology/save-edits",
            "/api/topology/apply-runtime",
            "/api/connection-test",
        }:
            return self.respond_json(404, {"error": "not found"})
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(content_length).decode("utf-8")
            payload = json.loads(body)
        except Exception as exc:
            return self.respond_json(400, {"error": f"invalid json: {exc!r}"})
        if not isinstance(payload, dict):
            return self.respond_json(400, {"error": "payload must be object"})
        if parsed.path == "/api/point-id-map/generate":
            try:
                start_id = int(payload.get("startId", 0))
            except (TypeError, ValueError):
                return self.respond_json(400, {"error": "startId must be integer"})
            result = generate_point_id_map_payload(
                bool(payload.get("apply", False)),
                payload.get("types"),
                start_id,
            )
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/generate":
            result = generate_topology(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/business":
            result = save_business_topology(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/validate-edits":
            result = validate_edited_topology(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/save-edits":
            result = save_edited_topology(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/apply-runtime":
            result = apply_topology_to_runtime(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/topology/load-opentcs":
            result = load_topology_to_opentcs(payload)
            return self.respond_json(200 if result.get("ok") else 400, result)
        if parsed.path == "/api/connection-test":
            result = connection_test(payload)
            return self.respond_json(200, result)
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

    def serve_generated_file(self, file_path: Path, content_type: str) -> None:
        resolved = file_path.resolve()
        if (
            not str(resolved).startswith(str(ROOT_DIR.resolve()))
            or resolved.suffix.lower() != ".png"
            or not resolved.exists()
            or resolved.is_dir()
        ):
            return self.respond_json(404, {"error": "generated file not found"})
        raw = resolved.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
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
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
    TOPOLOGY_VERSION_DIR.mkdir(parents=True, exist_ok=True)
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
