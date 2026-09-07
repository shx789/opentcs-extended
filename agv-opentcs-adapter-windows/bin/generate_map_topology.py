#!/usr/bin/env python3
import argparse
import json
import math
import sys
from collections import deque
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Optional, Tuple
from xml.sax.saxutils import escape

try:
    import cv2
    import numpy as np
    import yaml
except Exception as exc:  # pragma: no cover - dependency check for field deployment
    cv2 = None
    np = None
    yaml = None
    IMPORT_ERROR = exc
else:
    IMPORT_ERROR = None


ROOT_DIR = Path(__file__).resolve().parents[1]
DEFAULT_OUT_DIR = ROOT_DIR / "config" / "generated"
NEIGHBORS8 = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]


class MapContext:
    def __init__(self, map_yaml: Path) -> None:
        if IMPORT_ERROR is not None:
            raise RuntimeError(
                "Missing dependency. Install requirements.txt first. "
                f"Import error: {IMPORT_ERROR!r}"
            )
        self.map_yaml = map_yaml.resolve()
        with self.map_yaml.open("r", encoding="utf-8") as stream:
            self.meta = yaml.safe_load(stream)
        image_path = Path(self.meta["image"])
        if not image_path.is_absolute():
            image_path = self.map_yaml.parent / image_path
        if not image_path.exists():
            image_name = PurePosixPath(str(self.meta["image"]).replace("\\", "/")).name
            image_path = self.map_yaml.parent / image_name
        self.image_path = image_path.resolve()
        self.resolution = float(self.meta["resolution"])
        self.origin_x, self.origin_y, self.origin_yaw = [
            float(value) for value in self.meta["origin"]
        ]
        self.image = cv2.imread(str(self.image_path), cv2.IMREAD_GRAYSCALE)
        if self.image is None:
            raise RuntimeError(f"Could not read map image: {self.image_path}")
        self.height, self.width = self.image.shape

    def pixel_to_ros(self, px: float, py: float) -> Tuple[float, float]:
        x = self.origin_x + (px + 0.5) * self.resolution
        y = self.origin_y + (self.height - py - 0.5) * self.resolution
        return x, y


def point_name(index: int, kind: str) -> str:
    prefix = {"junction": "J", "endpoint": "E", "sample": "P", "center": "C"}.get(kind, "P")
    return f"AGV_{prefix}_{index + 1:02d}"


def make_point(ctx: MapContext, name: str, px: float, py: float, kind: str) -> Dict[str, Any]:
    ros_x, ros_y = ctx.pixel_to_ros(px, py)
    return {
        "name": name,
        "kind": kind,
        "pixel": {"x": int(round(px)), "y": int(round(py))},
        "ros_pose": {"x": round(ros_x, 4), "y": round(ros_y, 4), "yaw": 0.0},
        "opentcs_position": {
            "x": int(round(ros_x * 1000)),
            "y": int(round(ros_y * 1000)),
            "z": 0,
        },
    }


def path_length_mm(a: Dict[str, Any], b: Dict[str, Any]) -> int:
    apos = a["opentcs_position"]
    bpos = b["opentcs_position"]
    return max(int(round(math.hypot(apos["x"] - bpos["x"], apos["y"] - bpos["y"]))), 1)


def make_path(src: Dict[str, Any], dest: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "name": f"PATH_{src['name']}_TO_{dest['name']}",
        "srcPointName": src["name"],
        "destPointName": dest["name"],
        "length_mm": path_length_mm(src, dest),
        "bidirectional": True,
    }


def nearest_connected_paths(points: List[Dict[str, Any]], max_distance_mm: int) -> List[Dict[str, Any]]:
    if len(points) < 2:
        return []
    parent = list(range(len(points)))

    def find(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    candidates = []
    for i, src in enumerate(points):
        for j in range(i + 1, len(points)):
            dest = points[j]
            length = path_length_mm(src, dest)
            if length <= max_distance_mm:
                candidates.append((length, i, j))
    paths = []
    for _, i, j in sorted(candidates):
        root_i = find(i)
        root_j = find(j)
        if root_i == root_j:
            continue
        parent[root_j] = root_i
        paths.append(make_path(points[i], points[j]))
        if len(paths) >= len(points) - 1:
            break
    return paths


def add_loop_closure_paths(
    points: List[Dict[str, Any]],
    paths: List[Dict[str, Any]],
    max_distance_mm: int,
    max_extra_edges: int,
    target_min_degree: int = 2,
) -> List[Dict[str, Any]]:
    if len(points) < 3 or max_extra_edges <= 0:
        return paths
    by_name = {point["name"]: point for point in points}
    existing = {
        tuple(sorted((path["srcPointName"], path["destPointName"])))
        for path in paths
    }
    point_index = {point["name"]: index for index, point in enumerate(points)}
    parent = list(range(len(points)))

    def find(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(a: int, b: int) -> None:
        root_a = find(a)
        root_b = find(b)
        if root_a != root_b:
            parent[root_b] = root_a

    degree = {point["name"]: 0 for point in points}
    for path in paths:
        degree[path["srcPointName"]] = degree.get(path["srcPointName"], 0) + 1
        degree[path["destPointName"]] = degree.get(path["destPointName"], 0) + 1
        if path["srcPointName"] in point_index and path["destPointName"] in point_index:
            union(point_index[path["srcPointName"]], point_index[path["destPointName"]])

    candidates = []
    for i, src in enumerate(points):
        for j in range(i + 1, len(points)):
            dest = points[j]
            key = tuple(sorted((src["name"], dest["name"])))
            if key in existing:
                continue
            length = path_length_mm(src, dest)
            if length <= max_distance_mm:
                low_degree = min(degree.get(src["name"], 0), degree.get(dest["name"], 0))
                same_component = find(i) == find(j)
                candidates.append((1 if same_component else 0, low_degree, length, i, j))

    result = list(paths)
    added = 0
    for _, _, _, i, j in sorted(candidates):
        src = points[i]
        dest = points[j]
        same_component = find(i) == find(j)
        if (
            same_component
            and
            degree.get(src["name"], 0) >= target_min_degree
            and degree.get(dest["name"], 0) >= target_min_degree
        ):
            continue
        path = make_path(src, dest)
        key = tuple(sorted((path["srcPointName"], path["destPointName"])))
        if key in existing:
            continue
        result.append(path)
        existing.add(key)
        union(i, j)
        degree[path["srcPointName"]] = degree.get(path["srcPointName"], 0) + 1
        degree[path["destPointName"]] = degree.get(path["destPointName"], 0) + 1
        added += 1
        if added >= max_extra_edges:
            break
    return result


def angle_distance_rad(a: float, b: float) -> float:
    diff = abs(a - b) % math.pi
    return min(diff, math.pi - diff)


def merge_hough_segments(
    segments: List[Dict[str, Any]],
    angle_tolerance_deg: float,
    line_distance_px: float,
    max_gap_px: float,
    min_length_px: float,
) -> List[Dict[str, Any]]:
    if angle_tolerance_deg <= 0 or not segments:
        return segments
    angle_tolerance = math.radians(angle_tolerance_deg)
    groups: List[Dict[str, Any]] = []
    for segment in sorted(segments, key=lambda item: item["length"], reverse=True):
        dx = segment["x2"] - segment["x1"]
        dy = segment["y2"] - segment["y1"]
        angle = math.atan2(dy, dx) % math.pi
        direction = (math.cos(angle), math.sin(angle))
        normal = (-direction[1], direction[0])
        offset = normal[0] * segment["x1"] + normal[1] * segment["y1"]
        t1 = direction[0] * segment["x1"] + direction[1] * segment["y1"]
        t2 = direction[0] * segment["x2"] + direction[1] * segment["y2"]
        if t2 < t1:
            t1, t2 = t2, t1
        target_group = None
        for group in groups:
            if (
                angle_distance_rad(angle, group["angle"]) <= angle_tolerance
                and abs(offset - group["offset"]) <= line_distance_px
            ):
                target_group = group
                break
        if target_group is None:
            target_group = {
                "angle": angle,
                "direction": direction,
                "normal": normal,
                "offset": offset,
                "intervals": [],
            }
            groups.append(target_group)
        target_group["intervals"].append((t1, t2))

    merged_segments: List[Dict[str, Any]] = []
    for group in groups:
        direction = group["direction"]
        normal = group["normal"]
        offset = group["offset"]
        intervals = sorted(group["intervals"])
        if not intervals:
            continue
        current_start, current_end = intervals[0]
        for start, end in intervals[1:]:
            if start - current_end <= max_gap_px:
                current_end = max(current_end, end)
                continue
            append_projected_segment(merged_segments, direction, normal, offset, current_start, current_end, min_length_px)
            current_start, current_end = start, end
        append_projected_segment(merged_segments, direction, normal, offset, current_start, current_end, min_length_px)
    return sorted(merged_segments, key=lambda item: item["length"], reverse=True)


def append_projected_segment(
    target: List[Dict[str, Any]],
    direction: Tuple[float, float],
    normal: Tuple[float, float],
    offset: float,
    start: float,
    end: float,
    min_length_px: float,
) -> None:
    length = abs(end - start)
    if length < min_length_px:
        return
    x1 = direction[0] * start + normal[0] * offset
    y1 = direction[1] * start + normal[1] * offset
    x2 = direction[0] * end + normal[0] * offset
    y2 = direction[1] * end + normal[1] * offset
    target.append({
        "x1": int(round(x1)),
        "y1": int(round(y1)),
        "x2": int(round(x2)),
        "y2": int(round(y2)),
        "length": length,
    })


def path_key(src_name: str, dest_name: str) -> Tuple[str, str]:
    return tuple(sorted((src_name, dest_name)))


def graph_adjacency(paths: List[Dict[str, Any]]) -> Dict[str, List[str]]:
    adjacency: Dict[str, List[str]] = {}
    for path in paths:
        src = path["srcPointName"]
        dest = path["destPointName"]
        adjacency.setdefault(src, []).append(dest)
        adjacency.setdefault(dest, []).append(src)
    return adjacency


def point_angle_degrees(a: Dict[str, Any], b: Dict[str, Any], c: Dict[str, Any]) -> float:
    ax = a["pixel"]["x"] - b["pixel"]["x"]
    ay = a["pixel"]["y"] - b["pixel"]["y"]
    cx = c["pixel"]["x"] - b["pixel"]["x"]
    cy = c["pixel"]["y"] - b["pixel"]["y"]
    la = math.hypot(ax, ay)
    lc = math.hypot(cx, cy)
    if la <= 0 or lc <= 0:
        return 0.0
    dot = max(min((ax * cx + ay * cy) / (la * lc), 1.0), -1.0)
    return math.degrees(math.acos(dot))


def collapse_collinear_points(
    points: List[Dict[str, Any]],
    paths: List[Dict[str, Any]],
    angle_tolerance_deg: float,
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    if angle_tolerance_deg <= 0 or len(points) < 3:
        return points, paths

    point_by_name = {point["name"]: point for point in points}
    path_by_key = {
        path_key(path["srcPointName"], path["destPointName"]): path
        for path in paths
    }
    protected_kinds = {"junction", "center"}
    changed = True
    while changed:
        changed = False
        adjacency = graph_adjacency(list(path_by_key.values()))
        for name, neighbors in list(adjacency.items()):
            point = point_by_name.get(name)
            if point is None or point.get("kind") in protected_kinds or len(neighbors) != 2:
                continue
            first = point_by_name.get(neighbors[0])
            second = point_by_name.get(neighbors[1])
            if first is None or second is None:
                continue
            angle = point_angle_degrees(first, point, second)
            if abs(180.0 - angle) > angle_tolerance_deg:
                continue
            first_key = path_key(first["name"], name)
            second_key = path_key(name, second["name"])
            first_path = path_by_key.get(first_key)
            second_path = path_by_key.get(second_key)
            if first_path is None or second_path is None:
                continue
            del path_by_key[first_key]
            del path_by_key[second_key]
            new_key = path_key(first["name"], second["name"])
            if new_key not in path_by_key:
                new_path = make_path(first, second)
                new_path["length_mm"] = max(
                    int(first_path.get("length_mm", 0)) + int(second_path.get("length_mm", 0)),
                    path_length_mm(first, second),
                    1,
                )
                path_by_key[new_key] = new_path
            del point_by_name[name]
            changed = True
            break

    kept_points = [point for point in points if point["name"] in point_by_name]
    kept_names = {point["name"] for point in kept_points}
    kept_paths = [
        path for path in path_by_key.values()
        if path["srcPointName"] in kept_names and path["destPointName"] in kept_names
    ]
    return kept_points, kept_paths


def thinning(mask: Any) -> Any:
    if hasattr(cv2, "ximgproc") and hasattr(cv2.ximgproc, "thinning"):
        return cv2.ximgproc.thinning(mask, thinningType=cv2.ximgproc.THINNING_ZHANGSUEN)

    # Fallback morphological skeleton for environments without opencv-contrib.
    skel = np.zeros(mask.shape, np.uint8)
    work = mask.copy()
    kernel = cv2.getStructuringElement(cv2.MORPH_CROSS, (3, 3))
    while True:
        eroded = cv2.erode(work, kernel)
        opened = cv2.dilate(eroded, kernel)
        temp = cv2.subtract(work, opened)
        skel = cv2.bitwise_or(skel, temp)
        work = eroded.copy()
        if cv2.countNonZero(work) == 0:
            return skel


def degree(skel: Any, x: int, y: int) -> int:
    height, width = skel.shape
    total = 0
    for dx, dy in NEIGHBORS8:
        nx, ny = x + dx, y + dy
        if 0 <= nx < width and 0 <= ny < height and skel[ny, nx]:
            total += 1
    return total


def cluster_nodes(skel: Any, raw_nodes: List[Tuple[int, int, str]], radius: int) -> List[Dict[str, Any]]:
    height, width = skel.shape
    if not raw_nodes:
        return []
    node_mask = np.zeros((height, width), np.uint8)
    for x, y, _ in raw_nodes:
        node_mask[y, x] = 255
    node_mask = cv2.dilate(node_mask, np.ones((radius, radius), np.uint8), iterations=1)
    num, labels, stats, centroids = cv2.connectedComponentsWithStats(node_mask, 8)
    nodes = []
    for label in range(1, num):
        cx, cy = centroids[label]
        x0 = int(stats[label, cv2.CC_STAT_LEFT])
        y0 = int(stats[label, cv2.CC_STAT_TOP])
        box_w = int(stats[label, cv2.CC_STAT_WIDTH])
        box_h = int(stats[label, cv2.CC_STAT_HEIGHT])
        candidates = []
        kinds = set()
        for yy in range(y0, y0 + box_h):
            for xx in range(x0, x0 + box_w):
                if labels[yy, xx] != label:
                    continue
                if skel[yy, xx]:
                    candidates.append((xx, yy, (xx - cx) ** 2 + (yy - cy) ** 2))
                for rx, ry, kind in raw_nodes:
                    if abs(rx - xx) <= radius and abs(ry - yy) <= radius:
                        kinds.add(kind)
        if candidates:
            sx, sy, _ = min(candidates, key=lambda item: item[2])
        else:
            sx, sy = int(round(cx)), int(round(cy))
        nodes.append({
            "x": int(sx),
            "y": int(sy),
            "kind": "junction" if "junction" in kinds else "endpoint",
        })
    return nodes


def add_samples(skel: Any, nodes: List[Dict[str, Any]], spacing_px: int, limit: int) -> None:
    ys, xs = np.where(skel)
    for x, y in zip(xs, ys):
        ix, iy = int(x), int(y)
        if nodes and min((ix - n["x"]) ** 2 + (iy - n["y"]) ** 2 for n in nodes) < spacing_px ** 2:
            continue
        nodes.append({"x": ix, "y": iy, "kind": "sample"})
        if len(nodes) >= limit:
            return


def dedupe_nodes(nodes: List[Dict[str, Any]], min_distance_px: int, limit: int) -> List[Dict[str, Any]]:
    priority = {"junction": 0, "endpoint": 1, "sample": 2, "center": 3}
    result = []
    for node in sorted(nodes, key=lambda item: priority.get(item["kind"], 9)):
        if all((node["x"] - old["x"]) ** 2 + (node["y"] - old["y"]) ** 2 > min_distance_px ** 2 for old in result):
            result.append(node)
        if len(result) >= limit:
            break
    return result


def shortest_path_on_skeleton(skel: Any, start: Tuple[int, int], goal: Tuple[int, int], max_steps: int) -> Optional[List[Tuple[int, int]]]:
    height, width = skel.shape
    queue = deque([start])
    previous = {start: None}
    while queue and len(previous) < max_steps:
        x, y = queue.popleft()
        if (x, y) == goal:
            break
        for dx, dy in NEIGHBORS8:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < width and 0 <= ny < height):
                continue
            if not skel[ny, nx] or (nx, ny) in previous:
                continue
            previous[(nx, ny)] = (x, y)
            queue.append((nx, ny))
    if goal not in previous:
        return None
    path = []
    cursor = goal
    while cursor is not None:
        path.append(cursor)
        cursor = previous[cursor]
    path.reverse()
    return path


def skeleton_edges(skel: Any, nodes: List[Dict[str, Any]], max_neighbor_distance_px: int, max_path_px: int) -> List[Dict[str, Any]]:
    edges = []
    seen = set()
    coords = [(node["x"], node["y"]) for node in nodes]
    for i, start in enumerate(coords):
        nearby = []
        for j, goal in enumerate(coords):
            if i == j:
                continue
            distance = math.hypot(start[0] - goal[0], start[1] - goal[1])
            if distance <= max_neighbor_distance_px:
                nearby.append((distance, j))
        for _, j in sorted(nearby)[:4]:
            key = tuple(sorted((i, j)))
            if key in seen:
                continue
            path = shortest_path_on_skeleton(skel, start, coords[j], max_steps=6000)
            if not path:
                continue
            length_px = sum(math.hypot(x2 - x1, y2 - y1) for (x1, y1), (x2, y2) in zip(path, path[1:]))
            if 8 <= length_px <= max_path_px:
                seen.add(key)
                edges.append({"src": i, "dest": j, "length_px": length_px, "trace": path})
    return edges[:120]


def build_from_skeleton(ctx: MapContext, mask: Any, method: str, args: argparse.Namespace) -> Tuple[Dict[str, Any], Any]:
    skeleton = thinning(mask)
    skel = skeleton > 0
    raw_nodes = []
    for y in range(ctx.height):
        for x in range(ctx.width):
            if not skel[y, x]:
                continue
            deg = degree(skel, x, y)
            if deg == 1:
                raw_nodes.append((x, y, "endpoint"))
            elif deg >= 3:
                raw_nodes.append((x, y, "junction"))
    nodes = cluster_nodes(skel, raw_nodes, radius=args.cluster_radius_px)
    add_samples(skel, nodes, spacing_px=args.sample_spacing_px, limit=args.max_nodes * 2)
    nodes = dedupe_nodes(nodes, min_distance_px=args.min_node_distance_px, limit=args.max_nodes)
    edges = skeleton_edges(
        skel,
        nodes,
        max_neighbor_distance_px=args.max_neighbor_distance_px,
        max_path_px=args.max_path_px,
    )

    points = [make_point(ctx, point_name(index, node["kind"]), node["x"], node["y"], node["kind"]) for index, node in enumerate(nodes)]
    paths = [make_path(points[edge["src"]], points[edge["dest"]]) for edge in edges]
    for path, edge in zip(paths, edges):
        path["length_mm"] = max(int(round(edge["length_px"] * ctx.resolution * 1000)), 1)
    if not paths:
        paths = nearest_connected_paths(
            points,
            max_distance_mm=int(round(args.max_neighbor_distance_px * ctx.resolution * 1000)),
        )
    points, paths = collapse_collinear_points(
        points,
        paths,
        angle_tolerance_deg=args.collinear_angle_deg,
    )
    paths = add_loop_closure_paths(
        points,
        paths,
        max_distance_mm=int(round(args.loop_closure_distance_px * ctx.resolution * 1000)),
        max_extra_edges=args.loop_closure_edges,
    )

    preview = cv2.cvtColor(ctx.image, cv2.COLOR_GRAY2BGR)
    preview[skeleton > 0] = (0, 255, 255)
    for edge in edges:
        for (x1, y1), (x2, y2) in zip(edge["trace"], edge["trace"][1:]):
            cv2.line(preview, (x1, y1), (x2, y2), (255, 0, 255), 1)
    draw_points(preview, points)
    return mapping_model(ctx, method, points, paths), preview


def generate_free_space(ctx: MapContext, args: argparse.Namespace) -> Tuple[Dict[str, Any], Any]:
    free = (ctx.image >= args.free_threshold).astype(np.uint8) * 255
    clean = cv2.morphologyEx(free, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8), iterations=1)
    clean = cv2.morphologyEx(clean, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8), iterations=1)
    num, labels, stats, centroids = cv2.connectedComponentsWithStats(clean, 8)
    components = []
    for label in range(1, num):
        area = int(stats[label, cv2.CC_STAT_AREA])
        if area >= args.min_component_area:
            components.append((area, label))
    components.sort(reverse=True)
    points = []
    paths = []
    for _, label in components[:args.max_components]:
        x = int(stats[label, cv2.CC_STAT_LEFT])
        y = int(stats[label, cv2.CC_STAT_TOP])
        width = int(stats[label, cv2.CC_STAT_WIDTH])
        height = int(stats[label, cv2.CC_STAT_HEIGHT])
        cx, cy = centroids[label]
        center = make_point(ctx, point_name(len(points), "center"), cx, cy, "center")
        points.append(center)
        mask = labels == label
        if max(width, height) < 40:
            continue
        ys, xs = np.where(mask)
        coords = np.column_stack((xs.astype(np.float32), ys.astype(np.float32)))
        mean = coords.mean(axis=0)
        centered = coords - mean
        cov = np.cov(centered.T)
        eigvals, eigvecs = np.linalg.eigh(cov)
        axis = eigvecs[:, np.argmax(eigvals)]
        projections = centered @ axis
        a = mean + axis * projections.min()
        b = mean + axis * projections.max()
        pa = make_point(ctx, point_name(len(points), "endpoint"), a[0], a[1], "endpoint")
        pb = make_point(ctx, point_name(len(points) + 1, "endpoint"), b[0], b[1], "endpoint")
        points.extend([pa, pb])
        paths.append(make_path(pa, pb))
    preview = cv2.cvtColor(ctx.image, cv2.COLOR_GRAY2BGR)
    preview[clean > 0] = (180, 220, 180)
    draw_points(preview, points)
    draw_paths(preview, points, paths)
    return mapping_model(ctx, "free-space connected components", points, paths), preview


def generate_free_space_skeleton(ctx: MapContext, args: argparse.Namespace) -> Tuple[Dict[str, Any], Any]:
    free = (ctx.image >= args.free_threshold).astype(np.uint8) * 255
    safe_free = cv2.erode(free, np.ones((args.free_erode_px, args.free_erode_px), np.uint8), iterations=1)
    return build_from_skeleton(ctx, safe_free, "free-space skeleton from map.pgm", args)


def generate_blackline_skeleton(ctx: MapContext, args: argparse.Namespace) -> Tuple[Dict[str, Any], Any]:
    line = (ctx.image <= args.dark_threshold).astype(np.uint8) * 255
    line = cv2.morphologyEx(line, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8), iterations=1)
    num, labels, stats, _ = cv2.connectedComponentsWithStats(line, 8)
    clean = np.zeros_like(line)
    for label in range(1, num):
        if int(stats[label, cv2.CC_STAT_AREA]) >= args.min_component_area:
            clean[labels == label] = 255
    return build_from_skeleton(ctx, clean, "dark-line skeleton from map.pgm", args)


def generate_hough_line(ctx: MapContext, args: argparse.Namespace) -> Tuple[Dict[str, Any], Any]:
    dark = (ctx.image <= args.dark_threshold).astype(np.uint8) * 255
    dark = cv2.morphologyEx(dark, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8), iterations=1)
    detect = cv2.dilate(dark, np.ones((3, 3), np.uint8), iterations=1)
    raw_lines = cv2.HoughLinesP(
        detect,
        rho=1,
        theta=np.pi / 180,
        threshold=args.hough_threshold,
        minLineLength=args.hough_min_line_px,
        maxLineGap=args.hough_max_gap_px,
    )
    segments = []
    if raw_lines is not None:
        for line in np.asarray(raw_lines).reshape(-1, 4):
            x1, y1, x2, y2 = [int(value) for value in line]
            length = math.hypot(x2 - x1, y2 - y1)
            if length >= args.hough_min_line_px:
                segments.append({"x1": x1, "y1": y1, "x2": x2, "y2": y2, "length": length})
    segments = merge_hough_segments(
        segments,
        angle_tolerance_deg=args.hough_merge_angle_deg,
        line_distance_px=args.hough_merge_distance_px,
        max_gap_px=args.hough_merge_gap_px,
        min_length_px=args.hough_min_line_px,
    )
    segments = sorted(segments, key=lambda item: item["length"], reverse=True)[: args.max_segments]
    nodes = []
    for segment in segments:
        nodes.append({"x": segment["x1"], "y": segment["y1"], "kind": "endpoint"})
        nodes.append({"x": segment["x2"], "y": segment["y2"], "kind": "endpoint"})
    nodes = merge_close_nodes(nodes, radius=args.cluster_radius_px)
    points = [make_point(ctx, point_name(index, node["kind"]), node["x"], node["y"], node["kind"]) for index, node in enumerate(nodes[: args.max_nodes])]
    paths = []
    seen = set()
    for segment in segments:
        src = nearest_point(points, segment["x1"], segment["y1"])
        dest = nearest_point(points, segment["x2"], segment["y2"])
        if src is None or dest is None or src["name"] == dest["name"]:
            continue
        key = tuple(sorted((src["name"], dest["name"])))
        if key in seen:
            continue
        seen.add(key)
        paths.append(make_path(src, dest))
    points, paths = collapse_collinear_points(
        points,
        paths,
        angle_tolerance_deg=args.collinear_angle_deg,
    )
    paths = add_loop_closure_paths(
        points,
        paths,
        max_distance_mm=int(round(args.loop_closure_distance_px * ctx.resolution * 1000)),
        max_extra_edges=args.loop_closure_edges,
    )
    preview = cv2.cvtColor(ctx.image, cv2.COLOR_GRAY2BGR)
    for segment in segments:
        cv2.line(preview, (segment["x1"], segment["y1"]), (segment["x2"], segment["y2"]), (0, 255, 255), 1)
    draw_paths(preview, points, paths)
    draw_points(preview, points)
    return mapping_model(ctx, "Hough line topology from dark map strokes", points, paths), preview


def merge_close_nodes(nodes: List[Dict[str, Any]], radius: int) -> List[Dict[str, Any]]:
    clusters: List[Dict[str, Any]] = []
    for node in nodes:
        placed = False
        for cluster in clusters:
            if math.hypot(node["x"] - cluster["x"], node["y"] - cluster["y"]) <= radius:
                cluster["items"].append(node)
                cluster["x"] = sum(item["x"] for item in cluster["items"]) / len(cluster["items"])
                cluster["y"] = sum(item["y"] for item in cluster["items"]) / len(cluster["items"])
                placed = True
                break
        if not placed:
            clusters.append({"x": float(node["x"]), "y": float(node["y"]), "items": [node]})
    return [{"x": int(round(c["x"])), "y": int(round(c["y"])), "kind": "endpoint"} for c in clusters]


def nearest_point(points: List[Dict[str, Any]], x: int, y: int) -> Optional[Dict[str, Any]]:
    if not points:
        return None
    return min(points, key=lambda point: math.hypot(point["pixel"]["x"] - x, point["pixel"]["y"] - y))


def mapping_model(ctx: MapContext, method: str, points: List[Dict[str, Any]], paths: List[Dict[str, Any]]) -> Dict[str, Any]:
    return {
        "source_map": str(ctx.map_yaml),
        "source_image": str(ctx.image_path),
        "method": method,
        "resolution_m_per_pixel": ctx.resolution,
        "origin": [ctx.origin_x, ctx.origin_y, ctx.origin_yaw],
        "image_size": {"width": ctx.width, "height": ctx.height},
        "unit_rule": "openTCS x/y = ROS map x/y * 1000; ROS uses meters, openTCS uses millimeters",
        "points": points,
        "paths": paths,
        "notes": [
            "Generated candidate topology. Validate every point/path in RViz and on site.",
            "Rename generated AGV_* points to business names before production use.",
            "Adjust one-way/two-way traffic rules, vehicle envelopes, locations and blocks manually.",
        ],
    }


def plant_model_json(mapping: Dict[str, Any], model_name: str) -> Dict[str, Any]:
    plant_points = []
    for point in mapping["points"]:
        pos = point["opentcs_position"]
        plant_points.append({
            "name": point["name"],
            "position": {"x": pos["x"], "y": pos["y"], "z": 0},
            "vehicleOrientationAngle": point["ros_pose"].get("yaw", 0.0),
            "type": "HALT_POSITION",
            "layout": {"position": {"x": pos["x"], "y": pos["y"]}, "labelOffset": {"x": 0, "y": 0}, "layerId": 0},
            "vehicleEnvelopes": [],
            "maxVehicleBoundingBox": {"length": 1000, "width": 800, "height": 600, "referenceOffset": {"x": 0, "y": 0}},
            "properties": [
                {"name": "ros.x", "value": str(point["ros_pose"]["x"])},
                {"name": "ros.y", "value": str(point["ros_pose"]["y"])},
                {"name": "ros.yaw", "value": str(point["ros_pose"].get("yaw", 0.0))},
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
    for path in mapping["paths"]:
        for suffix, src, dest in [
            ("", path["srcPointName"], path["destPointName"]),
            ("_REV", path["destPointName"], path["srcPointName"]),
        ]:
            plant_paths.append({
                "name": path["name"] + suffix,
                "srcPointName": src,
                "destPointName": dest,
                "length": max(int(path["length_mm"]), 1),
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
        "vehicles": [{
            "name": "Vehicle-01",
            "boundingBox": {"length": 1000, "width": 800, "height": 600, "referenceOffset": {"x": 0, "y": 0}},
            "energyLevelCritical": 0,
            "energyLevelGood": 90,
            "energyLevelFullyRecharged": 90,
            "energyLevelSufficientlyRecharged": 0,
            "maxVelocity": 1000,
            "maxReverseVelocity": 1000,
            "layout": {"routeColor": "#00FF00"},
            "properties": [],
        }],
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


def draw_points(preview: Any, points: List[Dict[str, Any]]) -> None:
    for point in points:
        px = point["pixel"]["x"]
        py = point["pixel"]["y"]
        kind = point.get("kind", "")
        color = (0, 0, 255) if kind == "junction" else ((0, 255, 0) if kind == "endpoint" else (255, 0, 0))
        cv2.circle(preview, (px, py), 5, color, -1)
        cv2.putText(preview, point["name"], (px + 6, py - 4), cv2.FONT_HERSHEY_SIMPLEX, 0.34, color, 1, cv2.LINE_AA)


def draw_paths(preview: Any, points: List[Dict[str, Any]], paths: List[Dict[str, Any]]) -> None:
    by_name = {point["name"]: point for point in points}
    for path in paths:
        src = by_name.get(path["srcPointName"])
        dest = by_name.get(path["destPointName"])
        if not src or not dest:
            continue
        a = src["pixel"]
        b = dest["pixel"]
        cv2.line(preview, (a["x"], a["y"]), (b["x"], b["y"]), (255, 0, 255), 2)


def write_json(path: Path, payload: Dict[str, Any]) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def generate(args: argparse.Namespace) -> Dict[str, Any]:
    ctx = MapContext(Path(args.map_yaml))
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    if args.mode == "free-space":
        mapping, preview = generate_free_space(ctx, args)
    elif args.mode == "free-space-skeleton":
        mapping, preview = generate_free_space_skeleton(ctx, args)
    elif args.mode == "hough-line":
        mapping, preview = generate_hough_line(ctx, args)
    elif args.mode == "blackline-skeleton":
        mapping, preview = generate_blackline_skeleton(ctx, args)
    else:
        raise ValueError(f"Unsupported mode: {args.mode}")

    model_name = args.model_name or f"agv-{args.mode}-candidate"
    plant = plant_model_json(mapping, model_name)
    mapping_file = output_dir / "agv_opentcs_mapping.json"
    plant_json_file = output_dir / "opentcs_plant_model_candidate.json"
    plant_xml_file = output_dir / "plant_model.xml"
    preview_file = output_dir / "preview.png"
    status_file = output_dir / "generation_status.json"
    write_json(mapping_file, mapping)
    write_json(plant_json_file, plant)
    plant_xml_file.write_text(plant_model_xml(plant), encoding="utf-8")
    cv2.imwrite(str(preview_file), preview)
    status = {
        "ok": True,
        "mode": args.mode,
        "mapYaml": str(ctx.map_yaml),
        "sourceImage": str(ctx.image_path),
        "outputDir": str(output_dir),
        "mappingFile": str(mapping_file),
        "plantJsonFile": str(plant_json_file),
        "plantXmlFile": str(plant_xml_file),
        "previewFile": str(preview_file),
        "points": len(mapping["points"]),
        "paths": len(mapping["paths"]),
        "plantPathsIncludingReverse": len(plant["paths"]),
        "collinearAngleDeg": args.collinear_angle_deg,
        "loopClosureEdges": args.loop_closure_edges,
        "loopClosureDistancePx": args.loop_closure_distance_px,
        "houghMergeAngleDeg": args.hough_merge_angle_deg,
        "houghMergeDistancePx": args.hough_merge_distance_px,
        "houghMergeGapPx": args.hough_merge_gap_px,
    }
    write_json(status_file, status)
    return status


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate openTCS topology candidates from ROS map.yaml/PGM.")
    parser.add_argument("--map-yaml", required=True, help="Path to ROS map.yaml.")
    parser.add_argument("--output-dir", default=str(DEFAULT_OUT_DIR), help="Output directory.")
    parser.add_argument(
        "--mode",
        choices=["free-space", "free-space-skeleton", "hough-line", "blackline-skeleton"],
        default="blackline-skeleton",
    )
    parser.add_argument("--model-name", default="", help="Generated openTCS model name.")
    parser.add_argument("--free-threshold", type=int, default=250)
    parser.add_argument("--dark-threshold", type=int, default=90)
    parser.add_argument("--free-erode-px", type=int, default=5)
    parser.add_argument("--min-component-area", type=int, default=20)
    parser.add_argument("--max-components", type=int, default=12)
    parser.add_argument("--max-nodes", type=int, default=60)
    parser.add_argument("--sample-spacing-px", type=int, default=60)
    parser.add_argument("--cluster-radius-px", type=int, default=9)
    parser.add_argument("--min-node-distance-px", type=int, default=18)
    parser.add_argument("--max-neighbor-distance-px", type=int, default=150)
    parser.add_argument("--max-path-px", type=int, default=220)
    parser.add_argument("--hough-threshold", type=int, default=18)
    parser.add_argument("--hough-min-line-px", type=int, default=28)
    parser.add_argument("--hough-max-gap-px", type=int, default=10)
    parser.add_argument("--hough-merge-angle-deg", type=float, default=0.0)
    parser.add_argument("--hough-merge-distance-px", type=float, default=8.0)
    parser.add_argument("--hough-merge-gap-px", type=float, default=40.0)
    parser.add_argument("--max-segments", type=int, default=40)
    parser.add_argument("--loop-closure-edges", type=int, default=0)
    parser.add_argument("--loop-closure-distance-px", type=int, default=120)
    parser.add_argument(
        "--collinear-angle-deg",
        type=float,
        default=0.0,
        help="Collapse degree-2 points on nearly straight lines. 0 disables this simplifier.",
    )
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON result.")
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv)
    try:
        result = generate(args)
        if args.json:
            print(json.dumps(result, ensure_ascii=False))
        else:
            print(f"points={result['points']} paths={result['paths']} output={result['outputDir']}")
        return 0
    except Exception as exc:
        payload = {"ok": False, "error": repr(exc)}
        if args.json:
            print(json.dumps(payload, ensure_ascii=False))
        else:
            print(f"ERROR: {exc!r}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
