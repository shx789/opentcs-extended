# openTCS ↔ AGV 联调控制台

## 入口

- 实时控制台：`http://127.0.0.1:8090`
- openTCS Kernel HTTP：`http://127.0.0.1:55200`
- RCS 示例服务：`http://127.0.0.1:8080`
- MQTT：`127.0.0.1:1883`

## 当前地图/模型

- openTCS plant model：`opentcs_plant_model_business_topology_draft.json`
- AGV ↔ openTCS 映射：`agv_opentcs_business_topology_draft.json`
- AGV 拓扑预览：`agv_business_topology_preview.png`
- 控制台服务：`agv_opentcs_dashboard_server.py`

## 控制台功能

1. 拓扑/车辆实时视图
   - 绿色点表示 `Vehicle-01` 当前所在 openTCS 点位。
   - 紫色点表示页面下拉框当前选择的终点。
   - 蓝色箭头表示 openTCS 有向 path。

2. 任务控制
   - 起点/终点下拉框来自映射文件中的 19 个 AGV 点位。
   - `自动执行选中路线`：创建 mission，然后自动发送 `ARRIVED` 和 `COMPLETED` 仿真反馈。
   - `只创建 Mission`：只让 RCS 创建任务并下发 `robot_control`，后续可手动点 `发送 ARRIVED` / `发送 COMPLETED`。
   - `把 openTCS 车辆放到起点`：直接调用 openTCS HTTP，把 `Vehicle-01` 的位置重置到所选起点，方便重复测试。
   - `导出联调报告`：下载当前 health、missions、callbacks、MQTT 消息、点位映射的 JSON 快照。

3. 安全限制
   - 目前页面只允许 dev/fake_odom 仿真。
   - 如果检测到真实 ROS 模式且没有 fake/dev 模式，后端会拒绝发送仿真任务。
   - 真实小车接入前，不需要启动 `move_base_dev_mode:=false`。

## 一次完整流程

1. 页面选择起点/终点。
2. 点击 `自动执行选中路线` 或 `只创建 Mission`。
3. RCS 接收 WCS mission 请求。
4. RCS 发布 MQTT `robot_control`。
5. 当前无真实车时，页面通过 MQTT `task_feedback` 模拟 AGV 到点。
6. `ros_mqtt_modbus` 转发为 `agv/AGV_01/task/events`。
7. RCS 根据映射文件把 AGV 点位转换为 openTCS point。
8. RCS 调用 openTCS `PUT /v1/vehicles/Vehicle-01/position`。
9. openTCS OperationsDesk 和控制台视图更新车辆位置。

## 已验证

- 服务健康：openTCS / RCS / MQTT / ROS Master 均正常。
- 控制台可读取 19 个映射点。
- `JUNCTION_MID -> WAIT_EAST` 自动联调成功：mission `DONE`，`Vehicle-01` 最终位置 `WAIT_EAST`。
- `/api/report` 可导出联调报告。

## 常用接口

```bash
curl -sS http://127.0.0.1:8090/api/health
curl -sS http://127.0.0.1:8090/api/state
curl -sS http://127.0.0.1:8090/api/report

curl -sS -X POST http://127.0.0.1:8090/api/run-demo \
  -H 'Content-Type: application/json' \
  -d '{"from_point":"JUNCTION_MID","to_point":"WAIT_EAST","complete":true}'

curl -sS -X POST http://127.0.0.1:8090/api/set-position \
  -H 'Content-Type: application/json' \
  -d '{"point":"WAIT_WEST"}'
```

## 外部 AGV Adapter（推荐真实部署方式）

为了避免修改第三方 AGV/ROS 代码，新增了独立边车服务：

- 文件：`agv_native_feedback_adapter.py`
- 输入：订阅 AGV 原生 MQTT topic `task_feedback`
- 可选输入：旁路监听 `robot_control`，用于记录最近一次 RCS 下发任务上下文
- 输出：调用 openTCS `PUT /v1/vehicles/Vehicle-01/position`
- 默认点位映射：支持 `type:id=openTCS点名`，例如 `nav:0=WAIT_WEST,nav:1=WAIT_EAST`；旧格式 `0=WAIT_WEST` 仍兼容
- 状态文件：`agv_native_feedback_adapter_status.json`
- 日志文件：`agv_native_feedback_adapter.log`

启动示例：

```bash
setsid env \
  AGV_ADAPTER_MQTT_URI=mqtt://127.0.0.1:1883/ \
  AGV_ADAPTER_OPENTCS_URL=http://127.0.0.1:55200 \
  AGV_ADAPTER_VEHICLE_NAME=Vehicle-01 \
  AGV_ADAPTER_POINT_ID_MAP=nav:0=WAIT_WEST,nav:1=WAIT_EAST \
  python -u .\bin\agv_native_feedback_adapter.py
```

这个服务不在 `catkin_ws` 内，不需要重新编译第三方 AGV 代码。真实小车接入时，只要 AGV 继续发布原生 `task_feedback`，adapter 就能根据 `type:id`、`id` 或坐标映射到 openTCS 点位。

已验证的原生反馈示例：

```json
{
  "cmd_type": "task_feedback",
  "type": "point",
  "id": 1,
  "status": "success",
  "goal_pose": {"x": 0.23, "y": 0.01, "yaw": 0.09},
  "now_pose": {"x": 0.23, "y": 0.01, "yaw": 0.09}
}
```

验证结果：adapter 优先解析 `type=point,id=1` 或 `type=nav,id=1` 为 `WAIT_EAST`，openTCS 中 `Vehicle-01` 更新到 `WAIT_EAST`。
