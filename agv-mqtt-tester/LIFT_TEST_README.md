# AGV 升降顶简化联调工具

## 用途

这是一个独立的 MQTT 测试工具，不依赖 openTCS、RCS 或地图。它用于现场单独验证真实 AGV 的升顶、降顶控制和反馈。

## 启动

1. 先把电脑连接到 AGV WiFi 或现场 MQTT 网络。
2. 修改 `config.json`，把 `mqtt_uri` 改成现场 Broker 地址。
3. 确认 `command_topic` 为 `robot_control`，订阅 `task_feedback`、`base_status` 或现场实际反馈 Topic。
4. 双击 `start-lift-control.bat`。
5. 浏览器打开 `http://127.0.0.1:8093/`。

## 按钮和报文

点击 `升顶测试` 会发布到 `robot_control`：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 3
}
```

点击 `降顶测试` 会发布：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 4
}
```

页面会解析并显示最近反馈中的：

- `material`：物料检测状态
- `up`：升顶到位状态
- `down`：降顶到位状态

真实车辆的这三个字段位于 `base_status.magnetic` 对象中，不是根节点：

```json
{
  "cmd_type": "base_status",
  "magnetic": {"material": false, "up": false, "down": false}
}
```

## 现场测试顺序

1. 先连接 MQTT，不点击控制按钮，确认能收到 AGV 上报。
2. 确认 AGV 周围安全、急停可用、控制 Topic 无误。
3. 空载点击 `升顶测试`，观察 AGV 实际动作和 `up` 反馈。
4. 按现场要求放置托盘后，再观察 `material` 反馈。
5. 点击 `降顶测试`，观察 AGV 实际动作和 `down` 反馈。
6. 保存 `logs\mqtt-messages.jsonl` 和 AGV 端日志，作为联调记录。

## 注意

- 工具是直连 AGV 的手动测试工具，不经过 openTCS/RCS。
- 按钮只负责发布命令，不会自动等待传感器，也不会自动重试。
- 如果现场反馈字段名称或 Topic 不同，应先记录真实报文，再调整协议映射。
- 第一次测试必须低风险、低速度、空载开始。
