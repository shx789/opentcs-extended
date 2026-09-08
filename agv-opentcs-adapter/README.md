# AGV openTCS Adapter

这个目录是部署包，放在 `/home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter`，用于在不修改第三方 AGV/ROS 代码的情况下，把 AGV 原生 `task_feedback` 上报同步到 openTCS 车辆位置。

## 目录

- `bin/agv_native_feedback_adapter.py`：外部边车 adapter 主程序。
- `config/agv_opentcs_business_topology_draft.json`：AGV 点位到 openTCS 点位的映射。
- `config/opentcs_plant_model_business_topology_draft.json`：当前 openTCS plant model 备份。
- `docs/README_opentcs_agv_integration.md`：完整联调说明。
- `.env`：启动兜底配置，主要配置运行时配置文件路径。
- `config/runtime_config.json`：运行时热加载配置，现场联调时主要改这个文件。
- `config/runtime_config.example.json`：真实部署配置模板。
- `start.sh` / `stop.sh` / `status.sh`：启动、停止、查看状态。
- `logs/`：运行日志和状态文件。

## 启动

```bash
cd /home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter
./start.sh
./status.sh
```

## 停止

```bash
cd /home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter
./stop.sh
```

## 真实部署时通常只需要改 `config/runtime_config.json`

重点配置：

- `mqtt_uri`：AGV 使用的 MQTT broker，例如 `mqtt://192.168.1.50:1883/`。
- `source_topic`：AGV 原生反馈 topic，通常是 `task_feedback`。
- `command_topic`：上位机/调度下发控制 topic，通常是 `robot_control`。
- `open_tcs_base_url`：openTCS HTTP 地址。
- `vehicle_name`：openTCS 车辆名。
- `point_id_map`：AGV 原生 `type:id` 到 openTCS 点名的映射，例如 `nav:0=WAIT_WEST,charge:0=CHARGE_01`。
- `mapping_file`：映射 JSON 文件路径。

## 热加载配置

adapter 启动后会每约 1 秒检查 `config/runtime_config.json` 的修改时间：

- 修改 `point_id_map`、`distance_tolerance_m`、`open_tcs_base_url`、`rcs_base_url` 会直接刷新内存配置。
- 修改 `mqtt_uri`、`source_topic`、`command_topic`、`forward_command_topic` 会自动断开旧 MQTT 并重连新配置。
- JSON 写错时不会覆盖当前有效配置，状态文件会记录 `runtimeConfigError`。
- `.env` 仍保留为兜底；如果没有 `runtime_config.json`，会继续使用 `.env`。

现场换机器人 IP 时只需要改：

```json
{
  "mqtt_uri": "mqtt://192.168.1.60:1883/"
}
```

不需要重启 openTCS、RCS 或第三方 AGV 代码。

## 厂商原生反馈协议

已兼容厂商提供的 `task_feedback` JSON：

- `cmd_type=task_feedback`：表示这是反馈消息。
- `type`：支持 `track`、`nav`、`charge`、`point`。
- `id`：轨迹 ID、巡航点 ID、回充点 ID 或目标点 ID。
- `status`：`process` 映射为到达过程点，`success` 映射为任务完成，`failure/timeout` 映射为任务失败。
- `now_pose` / `goal_pose`：用于坐标兜底匹配 openTCS 点位。

实际部署时优先配置 `type:id` 映射，避免不同任务类型共用同一个 `id` 时冲突：

```bash
AGV_ADAPTER_POINT_ID_MAP=nav:0=WAIT_WEST,nav:1=WAIT_EAST,charge:0=CHARGE_01
```

如果厂商只使用一种任务类型，也可以继续写兼容格式：

```bash
AGV_ADAPTER_POINT_ID_MAP=0=WAIT_WEST,1=WAIT_EAST
```

## 不需要改第三方 AGV 代码

adapter 只订阅 AGV 已有的 `task_feedback` topic，并根据 `type/id/status/now_pose/goal_pose` 判断点位，然后调用 openTCS 更新车辆位置。因此第三方 AGV 包不需要重新编译。

## 无车自动反馈器

新增了一个独立的无车反馈器：

- `bin/agv_no_car_feedback_simulator.py`
- `start_no_car_sim.sh`
- `stop_no_car_sim.sh`
- `status_no_car_sim.sh`

用途：

- 不启动真实小车
- 不改 `catkin_ws`
- 直接监听 RCS 下发的 `robot_control`
- 自动回发原生 `task_feedback start/success`

适合做这条全链路：

- `RCS -> robot_control -> no-car simulator -> task_feedback -> adapter -> openTCS/RCS`

注意：

- 如果同时运行第三方 `ros_robot_control_node`，两边都会消费 `robot_control`，可能产生重复反馈
- 做纯无车闭环时，建议停掉 ROS 车端节点，只保留：
  - RCS
  - MQTT broker
  - `agv_native_feedback_adapter.py`
  - `agv_no_car_feedback_simulator.py`

启动：

```bash
cd /home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter
./start_no_car_sim.sh
```

查看状态：

```bash
cd /home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter
./status_no_car_sim.sh
```

停止：

```bash
cd /home/shx/workspace/opentcs/opentcs/agv-opentcs-adapter
./stop_no_car_sim.sh
```

可选环境变量：

- `AGV_NO_CAR_START_DELAY_SEC`：默认 `0.2`
- `AGV_NO_CAR_SUCCESS_DELAY_SEC`：默认 `2.0`
- `AGV_NO_CAR_FAIL_IF_POINT_MISSING`：默认 `false`

反馈器会读取：

- `config/runtime_config.json` 里的 `mqtt_uri`
- `config/runtime_config.json` 里的 `command_topic`
- `config/runtime_config.json` 里的 `source_topic`
- `config/runtime_config.json` 里的 `agv_point_file`

并按 `interest_point.json` 中的 `id -> x/y/z` 自动构造 `task_feedback`。

## 端到端测试结果

最近一次完整链路测试：

- Mission：`M-E2E-104215`
- Task：`T-E2E-104215`
- 路线：`WAIT_WEST -> WAIT_EAST`
- RCS 下发：`robot_control`，目标 `id=1`
- 原生 AGV 反馈：`task_feedback`，先 `status=process`，再 `status=success`
- Adapter 解析：`id=1 -> WAIT_EAST`
- Adapter 发布 RCS 事件：`ARRIVED` / `COMPLETED`
- RCS 回调：`ARRIVED_TO` / `DROPPED`
- RCS mission 最终状态：`DONE`
- openTCS 车辆最终位置：`Vehicle-01 WAIT_EAST IDLE IDLE`

注意：adapter 发给 RCS 的 `event_time` 使用 UTC ISO 格式，例如 `2026-07-02T02:42:18Z`。RCS 当前 parser 能稳定解析这个格式。
