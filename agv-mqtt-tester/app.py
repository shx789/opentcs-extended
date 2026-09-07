#!/usr/bin/env python3
import argparse
import json
import os
import socket
import sys
import threading
import time
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
from urllib.parse import parse_qs, urlparse

try:
    import paho.mqtt.client as mqtt
except Exception as exc:
    mqtt = None
    MQTT_IMPORT_ERROR = exc


if getattr(sys, "frozen", False):
    ROOT = Path(sys.executable).resolve().parent
else:
    ROOT = Path(__file__).resolve().parent
CONFIG_FILE = ROOT / "config.json"
CONFIG_EXAMPLE_FILE = ROOT / "config.example.json"
LOG_DIR = ROOT / "logs"
MESSAGE_LOG = LOG_DIR / "mqtt-messages.jsonl"
MAX_MESSAGES = 500


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def read_json(path: Path, default: Any) -> Any:
    try:
        if path.exists():
            return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default
    return default


def write_json(path: Path, data: Any) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_config() -> Dict[str, Any]:
    if CONFIG_FILE.exists():
        return read_json(CONFIG_FILE, {})
    return read_json(CONFIG_EXAMPLE_FILE, {})


def save_config(config: Dict[str, Any]) -> None:
    write_json(CONFIG_FILE, config)


def parse_mqtt_uri(uri: str) -> Tuple[str, int]:
    raw = (uri or "").strip()
    if not raw:
        raise ValueError("mqtt_uri is empty")
    if "://" not in raw:
        raw = "mqtt://" + raw
    parsed = urlparse(raw)
    host = parsed.hostname
    if not host:
        raise ValueError(f"invalid mqtt_uri: {uri}")
    port = parsed.port or 1883
    return host, port


def split_topics(raw: str) -> List[str]:
    topics: List[str] = []
    for part in str(raw or "").replace(",", "\n").splitlines():
        topic = part.strip()
        if topic and topic not in topics:
            topics.append(topic)
    return topics


def compact_payload(payload: bytes) -> Tuple[str, Optional[Any]]:
    text = payload.decode("utf-8", errors="replace")
    try:
        return text, json.loads(text)
    except Exception:
        return text, None


class MqttRuntime:
    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.client = None
        self.connected = False
        self.connecting = False
        self.last_error: Optional[str] = None
        self.config = load_config()
        self.messages: deque = deque(maxlen=MAX_MESSAGES)
        self.seq = 0
        LOG_DIR.mkdir(exist_ok=True)

    def status(self) -> Dict[str, Any]:
        with self.lock:
            return {
                "connected": self.connected,
                "connecting": self.connecting,
                "last_error": self.last_error,
                "seq": self.seq,
                "config": self.safe_config(),
                "mqtt_import_error": repr(MQTT_IMPORT_ERROR) if mqtt is None else None,
            }

    def safe_config(self) -> Dict[str, Any]:
        cfg = dict(self.config)
        if cfg.get("password"):
            cfg["password"] = "***"
        return cfg

    def add_message(self, direction: str, topic: str, payload_text: str, payload_json: Any = None) -> None:
        with self.lock:
            self.seq += 1
            item = {
                "seq": self.seq,
                "time": now_iso(),
                "direction": direction,
                "topic": topic,
                "payload": payload_text,
                "json": payload_json,
            }
            self.messages.appendleft(item)
        try:
            with MESSAGE_LOG.open("a", encoding="utf-8") as fp:
                fp.write(json.dumps(item, ensure_ascii=False) + "\n")
        except Exception:
            pass

    def get_messages(self, limit: int = 200) -> List[Dict[str, Any]]:
        with self.lock:
            return list(self.messages)[: max(1, min(limit, MAX_MESSAGES))]

    def clear_messages(self) -> None:
        with self.lock:
            self.messages.clear()
            self.seq += 1

    def disconnect(self) -> None:
        with self.lock:
            client = self.client
            self.client = None
            self.connecting = False
            self.connected = False
        if client is not None:
            try:
                client.loop_stop()
                client.disconnect()
            except Exception:
                pass

    def connect(self, config: Dict[str, Any]) -> Dict[str, Any]:
        if mqtt is None:
            raise RuntimeError(f"paho-mqtt is not importable: {MQTT_IMPORT_ERROR!r}")
        merged = dict(load_config())
        merged.update({k: v for k, v in config.items() if v is not None})
        host, port = parse_mqtt_uri(str(merged.get("mqtt_uri") or ""))
        topics = split_topics(str(merged.get("subscribe_topics") or ""))
        if not topics:
            raise ValueError("subscribe_topics is empty")

        self.disconnect()
        with self.lock:
            self.config = merged
            self.connecting = True
            self.last_error = None
        save_config(merged)

        client_id = str(merged.get("client_id") or f"agv-mqtt-tester-{os.getpid()}")
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        except Exception:
            client = mqtt.Client(client_id=client_id)

        username = str(merged.get("username") or "").strip()
        password = str(merged.get("password") or "")
        if username:
            client.username_pw_set(username, password)

        def on_connect(client_obj, userdata, flags, reason_code, properties=None):
            ok = int(reason_code) == 0 if isinstance(reason_code, int) else str(reason_code) in ("0", "Success")
            with self.lock:
                self.connected = ok
                self.connecting = False
                self.last_error = None if ok else f"connect failed: {reason_code}"
            if ok:
                for topic in topics:
                    client_obj.subscribe(topic, qos=int(merged.get("qos") or 0))
                self.add_message("system", "$connect", f"connected {host}:{port}, subscribed={topics}")

        def on_disconnect(client_obj, userdata, flags=None, reason_code=None, properties=None):
            with self.lock:
                self.connected = False
                self.connecting = False
                if reason_code not in (None, 0):
                    self.last_error = f"disconnected: {reason_code}"
            self.add_message("system", "$disconnect", f"disconnected reason={reason_code}")

        def on_message(client_obj, userdata, message):
            text, parsed = compact_payload(message.payload)
            self.add_message("in", message.topic, text, parsed)

        client.on_connect = on_connect
        client.on_disconnect = on_disconnect
        client.on_message = on_message

        try:
            client.connect(host, port, keepalive=30)
            client.loop_start()
        except Exception as exc:
            with self.lock:
                self.client = None
                self.connected = False
                self.connecting = False
                self.last_error = repr(exc)
            raise

        with self.lock:
            self.client = client
        deadline = time.time() + 2.0
        while time.time() < deadline:
            with self.lock:
                if self.connected or self.last_error:
                    break
            time.sleep(0.05)
        return self.status()

    def publish(self, topic: str, payload: str, qos: int = 0, retain: bool = False) -> Dict[str, Any]:
        topic = str(topic or "").strip()
        if not topic:
            raise ValueError("topic is empty")
        with self.lock:
            client = self.client
            connected = self.connected
        if client is None or not connected:
            raise RuntimeError("MQTT is not connected")
        payload_text = str(payload or "")
        parsed = None
        try:
            parsed = json.loads(payload_text)
            payload_text = json.dumps(parsed, ensure_ascii=False, separators=(",", ":"))
        except Exception:
            pass
        info = client.publish(topic, payload_text.encode("utf-8"), qos=int(qos), retain=bool(retain))
        self.add_message("out", topic, payload_text, parsed)
        return {"ok": True, "mid": getattr(info, "mid", None), "topic": topic, "payload": payload_text}


RUNTIME = MqttRuntime()


HTML = r"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AGV MQTT 测试工具</title>
<style>
:root{--bg:#0f172a;--panel:#111827;--line:#334155;--soft:#172033;--text:#e5e7eb;--muted:#94a3b8;--blue:#38bdf8;--green:#22c55e;--red:#f87171;--yellow:#fbbf24}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
header{padding:18px 22px;border-bottom:1px solid var(--line);background:#111827;position:sticky;top:0;z-index:2}
h1{font-size:22px;margin:0 0 6px}.sub{color:var(--muted);font-size:13px}.grid{display:grid;grid-template-columns:420px 1fr;gap:14px;padding:14px}.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px}h2{font-size:16px;margin:0 0 12px}
label{display:block;font-size:13px;color:#cbd5e1;margin:8px 0 4px}input,textarea,select{width:100%;background:#020617;color:var(--text);border:1px solid var(--line);border-radius:7px;padding:9px;font-family:inherit}textarea{min-height:120px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
button{border:0;border-radius:7px;padding:9px 11px;background:#2563eb;color:#fff;font-weight:700;cursor:pointer;margin:8px 6px 0 0}button.alt{background:#475569}button.good{background:#16a34a}button.warn{background:#b45309}button.danger{background:#dc2626}
.row{display:grid;grid-template-columns:1fr 1fr;gap:8px}.status{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}.badge{border:1px solid var(--line);background:#1e293b;border-radius:999px;padding:6px 9px;color:#cbd5e1;font-size:12px}.ok{color:#86efac}.err{color:#fca5a5}.warnText{color:#fde68a}.small{font-size:12px;color:var(--muted);line-height:1.55}.sensor-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:10px 0}.sensor{border:1px solid var(--line);border-radius:7px;padding:10px;background:#020617;text-align:center}.sensor strong{display:block;font-size:18px;margin-top:4px}.sensor.active{border-color:var(--green);color:#86efac}.sensor.inactive{color:#fca5a5}.sensor.unknown{color:var(--muted)}
pre{white-space:pre-wrap;word-break:break-word;background:#020617;border:1px solid #1e293b;border-radius:7px;padding:10px;max-height:260px;overflow:auto}.messages{max-height:680px;overflow:auto}.msg{border-bottom:1px solid #243244;padding:8px 0}.msg .meta{font-size:12px;color:#93c5fd;margin-bottom:4px}.msg.out .meta{color:#86efac}.msg.system .meta{color:#fde68a}.payload{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12px;white-space:pre-wrap;word-break:break-word}
@media(max-width:1000px){.grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<header>
  <h1>AGV MQTT 测试工具</h1>
  <div class="sub">只用于小车 MQTT 联通、订阅反馈和手工下发控制命令，不连接 openTCS/RCS。</div>
  <div class="status" id="statusBadges"></div>
</header>
<main class="grid">
  <section class="card">
    <h2>1. 连接 MQTT</h2>
    <label>MQTT URI</label>
    <input id="mqtt_uri" placeholder="mqtt://192.168.10.100:1883/">
    <div class="row">
      <div><label>Client ID</label><input id="client_id"></div>
      <div><label>QoS</label><input id="qos" type="number" min="0" max="2" value="0"></div>
    </div>
    <div class="row">
      <div><label>用户名</label><input id="username"></div>
      <div><label>密码</label><input id="password" type="password"></div>
    </div>
    <label>订阅 topic，一行一个。初次联调可用 # 看全部消息</label>
    <textarea id="subscribe_topics"></textarea>
    <label>控制 topic</label>
    <input id="command_topic" value="robot_control">
    <button onclick="connectMqtt()">连接并订阅</button>
    <button class="alt" onclick="disconnectMqtt()">断开</button>
    <button class="danger" onclick="clearMessages()">清空消息</button>
    <p class="small">真实小车测试时，先确认电脑已经连接到小车 WiFi 或同一内网，并能访问小车 MQTT 端口。</p>

    <h2 style="margin-top:18px">2. 升降顶测试</h2>
    <p class="small">按钮会直接向控制 topic 发布 magnetic_nav。真实车辆测试前确认车辆处于安全状态。</p>
    <div class="sensor-grid">
      <div class="sensor unknown" id="sensor_material">material<strong>未知</strong></div>
      <div class="sensor unknown" id="sensor_up">up<strong>未知</strong></div>
      <div class="sensor unknown" id="sensor_down">down<strong>未知</strong></div>
    </div>
    <button class="good" onclick="liftUpTest()">升顶测试</button>
    <button class="warn" onclick="liftDownTest()">降顶测试</button>
    <button class="alt" onclick="presetLiftUp()">生成升顶 JSON</button>
    <button class="alt" onclick="presetLiftDown()">生成降顶 JSON</button>

    <h2 style="margin-top:18px">3. 导航/原始命令</h2>
    <div class="row">
      <div><label>目标点/巡航点 ID</label><input id="point_id" type="number" value="1"></div>
      <div><label>速度 run_speed</label><input id="run_speed" type="number" value="0.5" step="0.1"></div>
    </div>
    <button class="good" onclick="presetStart()">模板：start</button>
    <button class="warn" onclick="presetStop()">模板：stop</button>
    <button class="alt" onclick="presetGet()">模板：get</button>
    <label>发布 payload，可手工修改</label>
    <textarea id="payload"></textarea>
    <button onclick="publishCommand()">发布到控制 topic</button>
    <pre id="result">暂无操作</pre>
  </section>

  <section class="card">
    <h2>4. 实时 MQTT 消息</h2>
    <p class="small">绿色是本工具发出的消息，蓝色是从 MQTT 收到的消息，黄色是连接状态。消息会写入 logs/mqtt-messages.jsonl。</p>
    <div class="messages" id="messages"></div>
  </section>
</main>
<script>
let currentConfig = {};

function $(id){return document.getElementById(id)}
function esc(s){return String(s ?? '').replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]))}
async function api(path, opts){const r=await fetch(path, opts); const t=await r.text(); let j; try{j=JSON.parse(t)}catch(e){j={raw:t}} if(!r.ok) throw j; return j}
function show(obj){$('result').textContent=JSON.stringify(obj,null,2)}

function formConfig(){
  return {
    mqtt_uri:$('mqtt_uri').value.trim(),
    client_id:$('client_id').value.trim(),
    username:$('username').value.trim(),
    password:$('password').value,
    subscribe_topics:$('subscribe_topics').value,
    command_topic:$('command_topic').value.trim(),
    qos:Number($('qos').value || 0),
    retain:false
  }
}

function fillConfig(c){
  currentConfig=c||{};
  $('mqtt_uri').value=c.mqtt_uri||'';
  $('client_id').value=c.client_id||'agv-mqtt-tester';
  $('username').value=c.username||'';
  $('password').value=(c.password && c.password !== '***') ? c.password : '';
  $('subscribe_topics').value=c.subscribe_topics||'task_feedback\nrobot_control\n#';
  $('command_topic').value=c.command_topic||'robot_control';
  $('qos').value=c.qos ?? 0;
}

function badges(st){
  const c=st.config||{};
  $('statusBadges').innerHTML=[
    st.connected?'<span class="badge ok">MQTT 已连接</span>':'<span class="badge err">MQTT 未连接</span>',
    `<span class="badge">uri ${esc(c.mqtt_uri||'-')}</span>`,
    `<span class="badge">cmd ${esc(c.command_topic||'-')}</span>`,
    `<span class="badge">seq ${esc(st.seq||0)}</span>`,
    st.last_error?`<span class="badge err">${esc(st.last_error)}</span>`:'',
    st.mqtt_import_error?`<span class="badge err">paho 导入失败</span>`:''
  ].join('');
}

async function loadConfig(){
  const data=await api('/api/config');
  fillConfig(data.config||{});
}

async function connectMqtt(){try{const data=await api('/api/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(formConfig())}); show(data); await refresh()}catch(e){show(e)}}
async function disconnectMqtt(){try{const data=await api('/api/disconnect',{method:'POST'}); show(data); await refresh()}catch(e){show(e)}}
async function clearMessages(){try{const data=await api('/api/clear',{method:'POST'}); show(data); await refresh()}catch(e){show(e)}}

function basePayload(cmd){
  return {
    cmd_type:'interest_point_control',
    cmd,
    id:Number($('point_id').value || 0),
    run_speed:Number($('run_speed').value || 0.5),
    path_stop_time:0,
    path_mode:0,
    circulates:1,
    time:cmd === 'get' ? 1 : 0
  }
}
function setPayload(obj){$('payload').value=JSON.stringify(obj,null,2)}
function presetStart(){setPayload(basePayload('start'))}
function presetStop(){setPayload(basePayload('stop'))}
function presetGet(){const p=basePayload('get'); p.id=0; setPayload(p)}

function magneticNavPayload(action){
  return {cmd_type:'magnetic_nav',aim_id:0,aim_dir:0,aim_action:action}
}
function presetLiftUp(){setPayload(magneticNavPayload(3))}
function presetLiftDown(){setPayload(magneticNavPayload(4))}
async function publishPreset(payload){
  setPayload(payload);
  await publishCommand();
}
async function liftUpTest(){
  if(!confirm('确认向真实 AGV 发送升顶命令？')) return;
  try{await publishPreset(magneticNavPayload(3))}catch(e){show(e)}
}
async function liftDownTest(){
  if(!confirm('确认向真实 AGV 发送降顶命令？')) return;
  try{await publishPreset(magneticNavPayload(4))}catch(e){show(e)}
}

function booleanValue(value){
  if(typeof value==='boolean') return value;
  if(typeof value==='number') return value !== 0;
  if(typeof value==='string') return ['true','1','yes','on'].includes(value.trim().toLowerCase());
  return null;
}
function updateSensor(id,value){
  const node=$(id); const state=booleanValue(value);
  node.classList.toggle('active',state === true);
  node.classList.toggle('inactive',state === false);
  node.classList.toggle('unknown',state === null);
  node.querySelector('strong').textContent=state === null ? '未知' : (state ? '触发' : '未触发');
}
function updateLiftStatus(items){
  for(const item of items){
    if(item.direction !== 'in') continue;
    let data=item.json;
    if(!data){try{data=JSON.parse(item.payload||'{}')}catch(e){continue}}
    if(!data || typeof data !== 'object') continue;
    const root=data.data && typeof data.data === 'object' ? data.data : data;
    if(['material','up','down'].some(k=>Object.prototype.hasOwnProperty.call(root,k))){
      updateSensor('sensor_material',root.material);
      updateSensor('sensor_up',root.up);
      updateSensor('sensor_down',root.down);
      break;
    }
  }
}

async function publishCommand(){
  try{
    const data=await api('/api/publish',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({topic:$('command_topic').value.trim(), payload:$('payload').value, qos:Number($('qos').value||0), retain:false})});
    show(data); await refresh();
  }catch(e){show(e)}
}

function renderMessages(items){
  if(!items.length){$('messages').innerHTML='<p class="small">暂无消息</p>';return}
  $('messages').innerHTML=items.map(m=>{
    const cls=m.direction==='out'?'out':(m.direction==='system'?'system':'in');
    let payload=m.payload||'';
    try{payload=JSON.stringify(JSON.parse(payload),null,2)}catch(e){}
    return `<div class="msg ${cls}"><div class="meta">#${esc(m.seq)} ${esc(m.time)} ｜ ${esc(m.direction)} ｜ ${esc(m.topic)}</div><div class="payload">${esc(payload)}</div></div>`
  }).join('');
  updateLiftStatus(items);
}

async function refresh(){
  try{
    const data=await api('/api/messages?limit=200');
    badges(data.status||{});
    renderMessages(data.messages||[]);
  }catch(e){show(e)}
}

loadConfig().then(()=>{presetStart(); refresh(); setInterval(refresh, 1000)});
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    server_version = "AgvMqttTester/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"[{now_iso()}] {self.address_string()} {fmt % args}")

    def send_json(self, data: Any, status: int = 200) -> None:
        body = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_body_json(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0:
            return {}
        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        return json.loads(raw) if raw.strip() else {}

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path in ("/", "/index.html"):
            body = HTML.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if parsed.path == "/api/config":
            self.send_json({"config": RUNTIME.safe_config(), "config_file": str(CONFIG_FILE)})
            return
        if parsed.path == "/api/status":
            self.send_json(RUNTIME.status())
            return
        if parsed.path == "/api/messages":
            query = parse_qs(parsed.query)
            limit = int((query.get("limit") or ["200"])[0])
            self.send_json({"status": RUNTIME.status(), "messages": RUNTIME.get_messages(limit)})
            return
        self.send_json({"error": "not found"}, status=404)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        try:
            if parsed.path == "/api/connect":
                self.send_json(RUNTIME.connect(self.read_body_json()))
            elif parsed.path == "/api/disconnect":
                RUNTIME.disconnect()
                self.send_json(RUNTIME.status())
            elif parsed.path == "/api/publish":
                body = self.read_body_json()
                self.send_json(RUNTIME.publish(
                    str(body.get("topic") or ""),
                    str(body.get("payload") or ""),
                    int(body.get("qos") or 0),
                    bool(body.get("retain") or False),
                ))
            elif parsed.path == "/api/clear":
                RUNTIME.clear_messages()
                self.send_json({"ok": True, "status": RUNTIME.status()})
            else:
                self.send_json({"error": "not found"}, status=404)
        except Exception as exc:
            self.send_json({"ok": False, "error": repr(exc)}, status=400)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("AGV_MQTT_TESTER_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("AGV_MQTT_TESTER_PORT", "8093")))
    args = parser.parse_args()

    try:
        socket.setdefaulttimeout(8)
        server = ThreadingHTTPServer((args.host, args.port), Handler)
    except OSError as exc:
        print(f"Failed to start server on {args.host}:{args.port}: {exc}")
        return 1
    print(f"AGV MQTT tester: http://{args.host}:{args.port}/")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        RUNTIME.disconnect()
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
