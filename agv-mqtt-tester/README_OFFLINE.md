# AGV MQTT 测试工具离线版

这个离线版给现场内网测试人员使用，不需要安装 Python，也不需要访问外网。
页面包含专用的真实 AGV 升降顶测试按钮，不依赖 openTCS 或 RCS。

## 使用步骤

1. 解压 `agv-mqtt-tester-offline.zip`。
2. 双击 `start-exe.bat`。
3. 浏览器会打开：

```text
http://127.0.0.1:8093/
```

如果浏览器没有自动打开，手动访问上面的地址。

## 现场填写

如果电脑连接的是小车 WiFi，且小车 MQTT 地址是 `192.168.10.100:1883`：

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

建议第一次先订阅 `#`，这样能看到当前 MQTT Broker 下所有允许订阅的消息。

## 测试顺序

1. 先只连接 MQTT，不发命令。
2. 观察页面是否有小车主动上报消息。
3. 如果能看到消息，再点击命令模板，例如 `start`。
4. 修改 `id` 为现场确认存在的巡航点/兴趣点 ID。
5. 发布到 `robot_control`。
6. 观察是否有 `task_feedback` 返回，以及小车是否实际动作。

## 升降顶测试

连接成功后使用页面中的按钮：

1. 点击 `升顶测试`，工具向 `robot_control` 发布 `magnetic_nav`，`aim_action=3`。
2. 观察 AGV 是否升顶，以及反馈中的 `up=true`、`material=true`。
3. 点击 `降顶测试`，工具向 `robot_control` 发布 `magnetic_nav`，`aim_action=4`。
4. 观察 AGV 是否降顶，以及反馈中的 `down=true`、`material=false`。

按钮会弹出二次确认。真实车辆测试时，现场人员必须确认车辆安全，并确保急停可用。

## 注意

- 这个工具只验证 MQTT，不连接 openTCS/RCS。
- 看到消息不代表命令一定能控制小车。
- 控制小车需要 topic 和 JSON 字段符合小车协议。
- 如果页面没有任何消息，先检查电脑是否和小车在同一网络、MQTT IP/端口是否正确。
