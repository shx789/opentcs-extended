# AGV MQTT 简化测试工具

这个工具只做 MQTT 联通验证，不依赖 openTCS、RCS、地图映射。

用途：

- 连接真实小车 MQTT Broker。
- 订阅小车反馈 topic，例如 `task_feedback`。
- 实时查看小车发布的消息。
- 向控制 topic 发送命令，例如 `robot_control`。
- 通过专用按钮直接测试升顶、降顶 `magnetic_nav` 命令。
- 实时查看 `material`、`up`、`down` 反馈状态。
- 先验证“我们能不能连上小车、能不能看到反馈、能不能发控制命令”。

## 启动

Windows：

```bat
start.bat
```

浏览器打开：

```text
http://127.0.0.1:8093/
```

Linux：

```bash
bash start.sh
```

## 现场怎么填

如果小车 MQTT 地址是 `192.168.10.100:1883`：

```text
MQTT URI: mqtt://192.168.10.100:1883/
订阅 topic:
task_feedback
base_status
interest_point
#

控制 topic:
robot_control
```

如果现场只知道小车 IP，不知道 topic，可以先订阅 `#`，它会显示所有可见消息。确认 topic 后再改成精确 topic。

## 常用命令模板

## 升降顶测试

连接 MQTT 后，点击页面中的专用按钮：

- `升顶测试`：发布 `aim_action=3`
- `降顶测试`：发布 `aim_action=4`

升顶 JSON：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 3
}
```

降顶 JSON：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 4
}
```

页面会从订阅到的消息中显示：

- `material=true`：检测到物料
- `up=true`：升顶到位
- `down=true`：降顶到位

本工具是直接控制测试工具，不经过 openTCS/RCS。现场第一次测试必须确认急停可用，并先空载、低风险测试。

巡航点/兴趣点执行：

```json
{
  "cmd_type": "interest_point_control",
  "cmd": "start",
  "id": 1,
  "run_speed": 0.5,
  "path_stop_time": 0,
  "path_mode": 0,
  "circulates": 1,
  "time": 0
}
```

停止：

```json
{
  "cmd_type": "interest_point_control",
  "cmd": "stop",
  "id": 1,
  "run_speed": 0.5,
  "path_stop_time": 0,
  "path_mode": 0,
  "circulates": 1,
  "time": 0
}
```

读取兴趣点/巡航点：

```json
{
  "cmd_type": "interest_point_control",
  "cmd": "get",
  "id": 0,
  "run_speed": 0.5,
  "path_stop_time": 0,
  "path_mode": 0,
  "circulates": 1,
  "time": 1
}
```

## 判断是否成功

成功连接后，页面状态会显示：

```text
connected=true
```

发送控制命令后，至少要看到以下一种情况：

- 小车在 `task_feedback` 返回 `start` / `process` / `success`。
- 小车在其它 topic 返回状态、错误或位置。
- 小车实际运动。

如果只显示“已发布”，但没有任何反馈，常见原因：

- MQTT IP/端口填错。
- 小车和电脑不在同一网络。
- 控制 topic 不是 `robot_control`。
- 小车命令格式和当前模板不一致。
- 小车端没有启动 MQTT 控制程序。

## 打包

在 PowerShell 里执行：

```powershell
.\package_release.ps1
```

会生成：

```text
release/agv-mqtt-tester.zip
```

测试人员解压后运行 `start.bat` 即可。
