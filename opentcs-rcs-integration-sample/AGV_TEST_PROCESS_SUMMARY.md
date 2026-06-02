# AGV 测试过程总结

## 1. 测试结论

上次 AGV 测试已经完成的是本地软件集成联调，不是完整现场整车验收。

已验证链路：

```text
WCS -> RCS -> MQTT Broker -> AGV 适配程序 -> RCS MQTT 消费 -> WCS 回调
```

测试结论：

| 项目 | 结论 |
|---|---|
| AGV MQTT 反馈协议 | 已验证通过 |
| AGV 适配程序旧 topic 转新协议 | 已验证通过 |
| RCS 订阅 AGV MQTT 并解析 | 已验证通过 |
| RCS 回调 WCS | 已验证通过 |
| WCS 接收并查询 AGV 事件 | 已验证通过 |
| MQTT 反馈幂等 | 已验证通过 |
| 真实小车运动 | 未覆盖 |
| 真实导航、避障、充电、急停 | 未覆盖 |
| PLC/堆垛机实物联动 | 未覆盖 |
| FAT/SAT 正式验收 | 未完成 |

准确判断：软件反馈链路已经完成，可以进入真实小车联调；但不能认为整车功能已经完成验收。

## 2. 测试类型说明

这次测试属于本地软件集成测试，目标是确认 AGV 反馈链路和协议转换能跑通。

不是现场整车测试，因为没有接真实 AGV 物理运动、真实车端 MQTT Server、真实 PLC 或堆垛机。

测试覆盖层级：

| 层级 | 是否完成 | 说明 |
|---|---|---|
| 代码编译 | 是 | RCS Java、WCS Python、AGV catkin 编译/测试通过 |
| 单元测试 | 是 | RCS/WCS 关键逻辑测试通过 |
| 接口测试 | 是 | WCS/RCS HTTP 接口本地验证通过 |
| 软件联调 | 是 | WCS、RCS、MQTT、AGV adapter 实际启动验证通过 |
| 真实小车联调 | 否 | 没有控制真实小车移动 |
| PLC 实物联调 | 否 | 没有真实 PLC 握手 |
| 现场验收测试 | 否 | 需要现场设备和正式测试记录 |

## 3. 测试环境

本地参与组件：

| 组件 | 作用 |
|---|---|
| WCS backend | 提供任务下发接口和 AGV 事件接收接口 |
| RCS/openTCS integration sample | 接收 WCS mission，订阅 AGV MQTT，回调 WCS |
| MQTT broker | 本地 MQTT 消息中转 |
| AGV adapter `ros_mqtt_modbus` | 模拟车端适配程序，消费旧 AGV topic 并发布新 RCS topic |

关键端口：

| 端口 | 组件 |
|---|---|
| `8000` | WCS HTTP API |
| `8090` | RCS HTTP API |
| `1883` | MQTT broker |

## 4. 实际测试流程

### 4.1 启动 MQTT Broker

测试中使用 `amqtt` 作为本地 broker。

一开始 RCS 订阅 MQTT 时遇到 SUBACK `128`，原因是 broker 默认 topic ACL 拒绝订阅。后续通过 ACL 配置允许匿名访问相关 topic 后，RCS 可以正常订阅。

ACL 允许的 topic 包括：

```text
#
agv/#
base_status
task_feedback
feedback
```

这个问题说明：RCS 订阅失败不是 RCS MQTT 代码逻辑问题，而是 broker 权限/ACL 问题。

### 4.2 启动 WCS

启动 WCS backend 后，确认 WCS 提供以下接口：

| 方法 | URL | 说明 |
|---|---|---|
| POST | `/api/wcs/rcs/missions` | WCS 创建 AGV mission |
| GET | `/api/wcs/rcs/missions/{mission_no}` | WCS 查询 RCS mission |
| POST | `/api/wcs/rcs/missions/{mission_no}/cancel` | WCS 取消 RCS mission |
| POST | `/api/wcs/agv/events` | WCS 接收 RCS 回调 |
| GET | `/api/wcs/agv/events/{mission_no}` | 查询 WCS 收到的 AGV 事件 |

### 4.3 启动 RCS

RCS 启动时打开 AGV MQTT 订阅能力：

```text
RCS_AGV_MQTT_ENABLED=true
RCS_AGV_MQTT_BROKER_URI=tcp://127.0.0.1:1883
RCS_AGV_MQTT_TOPIC=agv/+/task/events
RCS_CALLBACK_BASE_URL=http://127.0.0.1:8000
```

确认 RCS 监听 `8090`，并能接收 WCS 创建的 mission。

### 4.4 通过 WCS 创建 AGV 搬运任务

测试 mission 示例：

```json
{
  "mission_no": "M-AGV-TEST-001",
  "task_no": "T-AGV-TEST-001",
  "from_point": "P_WAIT_IN_01",
  "to_point": "ST_IN_01",
  "pallet_no": "PLT001",
  "priority": 50
}
```

预期结果：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "mission_no": "M-AGV-TEST-001",
    "task_no": "T-AGV-TEST-001",
    "rcs_status": "RECEIVED",
    "idem_hit": false
  }
}
```

验证点：

| 验证点 | 结果 |
|---|---|
| WCS 能接收 mission 创建请求 | 通过 |
| WCS 能转发到 RCS | 通过 |
| RCS 能保存 mission | 通过 |
| RCS 能保存 callback_url | 通过 |

## 5. AGV 新协议 MQTT 反馈测试

### 5.1 完成事件 `DROPPED`

向 RCS 订阅的 AGV topic 发布完成事件：

Topic：

```text
agv/AGV_01/task/events
```

Payload：

```json
{
  "message_id": "AGV_01-M-AGV-TEST-001-1",
  "agv_id": "AGV_01",
  "event_type": "DROPPED",
  "mission_no": "M-AGV-TEST-001",
  "task_no": "T-AGV-TEST-001",
  "point_id": "ST_IN_01",
  "battery": 80,
  "seq": 1,
  "event_time": "2026-06-01T15:20:00Z"
}
```

结果：

| 验证点 | 结果 |
|---|---|
| RCS 收到 MQTT 消息 | 通过 |
| RCS 解析 `DROPPED` | 通过 |
| RCS 回调 WCS | 通过 |
| WCS 查询到 `DROPPED` 事件 | 通过 |
| RCS mission 状态变为 `DONE` | 通过 |

### 5.2 异常事件 `FAILED`

测试 payload：

```json
{
  "message_id": "AGV_01-M-AGV-TEST-003-FAIL",
  "agv_id": "AGV_01",
  "event_type": "FAILED",
  "mission_no": "M-AGV-TEST-003",
  "task_no": "T-AGV-TEST-003",
  "point_id": "P_WAIT_IN_01",
  "battery": 22,
  "reason_code": "LOW_BATTERY",
  "reason_msg": "battery below threshold",
  "seq": 1,
  "event_time": "2026-06-01T15:23:00Z"
}
```

结果：

| 验证点 | 结果 |
|---|---|
| WCS 收到 `FAILED` | 通过 |
| `reason_code=LOW_BATTERY` 保留 | 通过 |
| `reason_msg=battery below threshold` 保留 | 通过 |
| RCS mission 状态变为 `FAILED` | 通过 |

### 5.3 到点事件 `ARRIVED`

测试中发现并修复了一个问题：

问题：

- AGV 反馈 `ARRIVED` 是泛化事件。
- RCS 需要根据 `point_id` 判断是到达起点还是终点。
- 原先纯 mission 创建接口没有把 `from_point/to_point` 保存到 mission 上下文。
- 导致 `ARRIVED` 无法推导为 `ARRIVED_FROM` 或 `ARRIVED_TO`。

修复：

- RCS 创建 mission 时保存 `from_point`、`to_point`、`pallet_no`、`priority`。
- AGV 事件消费逻辑先查 TaskStore，查不到再查 MissionStore。

复测 payload：

```json
{
  "message_id": "AGV_01-M-AGV-TEST-005-ARRIVE-FROM",
  "agv_id": "AGV_01",
  "event_type": "ARRIVED",
  "mission_no": "M-AGV-TEST-005",
  "task_no": "T-AGV-TEST-005",
  "point_id": "P_WAIT_IN_01",
  "battery": 78,
  "seq": 1,
  "event_time": "2026-06-01T15:43:00Z"
}
```

WCS 收到结果：

```json
{
  "event_type": "ARRIVED_FROM",
  "point_id": "P_WAIT_IN_01",
  "agv_id": "AGV_01",
  "battery": 78
}
```

结论：`ARRIVED` 推导通过。

## 6. MQTT 幂等测试

测试方法：连续发布两次相同 MQTT 消息，保持相同 `message_id`。

测试消息关键字段：

```text
message_id = AGV_01-M-AGV-TEST-005-ARRIVE-FROM
```

第一次测试发现问题：

- WCS 收到了两条重复事件。
- 原因是 RCS callback outbox 中已 `SUCCESS` 的记录会被重复 MQTT 消息覆盖成 `PENDING`。
- 结果导致重复回调 WCS。

修复：

- `CallbackOutboxService.enqueue()` 入队前先检查 `idemKey` 是否已经存在。
- 如果存在，不重新入队。
- 成功回调过的重复消息不会再次回调 WCS。

复测结果：

| 验证点 | 结果 |
|---|---|
| 连续发布两次相同 `message_id` | 已执行 |
| WCS 只收到一条事件 | 通过 |
| RCS mission 状态正常 | 通过 |

结论：AGV MQTT 反馈幂等通过。

## 7. AGV 适配程序旧 topic 转新协议测试

这一步是关键测试，因为它不是直接模拟 RCS 新协议，而是实际启动 AGV 代码里的可执行文件：

```text
devel/lib/ros_mqtt_modbus/ros_mqtt_modbus
```

启动环境变量：

```bash
AGV_ID=AGV_01
MQTT_BROKER_URL=mqtt://127.0.0.1:1883
AGV_MISSION_NO=M-AGV-TEST-006
AGV_TASK_NO=T-AGV-TEST-006
AGV_RCS_TASK_TOPIC=agv/AGV_01/task/events
AGV_RCS_STATUS_TOPIC=agv/AGV_01/status
```

向旧 AGV topic 发布消息：

Topic：

```text
task_feedback
```

Payload：

```json
{
  "cmd_type": "task_feedback",
  "status": "success",
  "id": 1006,
  "type": "point",
  "mission_no": "M-AGV-TEST-006",
  "task_no": "T-AGV-TEST-006",
  "point_id": "ST_IN_01",
  "goal_pose": {
    "x": 1.0,
    "y": 2.0,
    "yaw": 0.0
  }
}
```

结果：

| 验证点 | 结果 |
|---|---|
| AGV adapter 收到旧 `task_feedback` | 通过 |
| AGV adapter 发布新 RCS task event | 通过 |
| RCS 收到新 MQTT event | 通过 |
| RCS 标准化为 WCS `DROPPED` | 通过 |
| WCS 查询到 `DROPPED` | 通过 |
| RCS mission 状态变为 `DONE` | 通过 |

WCS 查询结果摘要：

```json
{
  "mission_no": "M-AGV-TEST-006",
  "events": [
    {
      "mission_no": "M-AGV-TEST-006",
      "task_no": "T-AGV-TEST-006",
      "event_type": "DROPPED",
      "point_id": "ST_IN_01",
      "agv_id": "AGV_01",
      "battery": null,
      "reason_code": null,
      "reason_msg": null
    }
  ]
}
```

结论：AGV 旧反馈 topic 到 RCS 新协议的旁路转换链路通过。

## 8. 自动化验证

### 8.1 WCS 测试

执行范围：

```text
tests/test_rcs_api.py
tests/test_task_executor.py
tests/test_device_status.py
```

结果：

```text
17 passed
```

### 8.2 RCS 测试

执行：

```text
./gradlew :opentcs-rcs-integration-sample:test
```

结果：

```text
BUILD SUCCESSFUL
```

### 8.3 AGV 编译测试

执行：

```text
catkin_make --pkg ros_mqtt_modbus
```

结果：

```text
Built target ros_mqtt_modbus
```

## 9. 为什么这个流程是正规的

从软件集成测试角度看，这个流程是正规的，因为它具备以下要素：

| 要素 | 是否具备 |
|---|---|
| 明确测试对象 | 是，WCS/RCS/MQTT/AGV adapter |
| 明确输入 | 是，mission 请求和 AGV MQTT payload |
| 明确输出 | 是，WCS callback 和 RCS mission 状态 |
| 正向场景 | 是，`DROPPED` |
| 异常场景 | 是，`FAILED` |
| 过程状态场景 | 是，`ARRIVED` |
| 幂等场景 | 是，重复 `message_id` |
| 兼容场景 | 是，旧 `task_feedback` 转新协议 |
| 自动化验证 | 是，WCS/RCS 测试和 AGV 编译 |
| 可复现记录 | 是，已记录 topic、payload、结果 |

但它不是完整现场验收测试，因为没有真实车和真实 PLC。

## 10. 未覆盖内容

| 未覆盖项 | 说明 |
|---|---|
| 真实 AGV 移动 | 没验证小车实际行走 |
| 真实导航点位 | 没验证地图点位、坐标、openTCS 点位一致性 |
| 真实车端 MQTT Server | 当前使用本地 broker |
| 断线重连 | 未系统测试 broker 断开、RCS 重连、AGV 离线恢复 |
| 急停/复位/充电 | 未验证真实安全和控制动作 |
| PLC/堆垛机握手 | 未验证 WCS 后续 PLC 阶段 |
| 长时间稳定性 | 未做 8 小时或 24 小时 soak test |
| 多车并发 | 当前只测 `AGV_01` 单车 |
| 乱序/延迟消息 | 未系统测试 MQTT 乱序、重复、延迟到达 |

## 11. 后续现场测试建议

进入真实小车联调时，建议按以下顺序测：

1. MQTT 连通性：确认 RCS 能连真实车端 MQTT Server。
2. 车辆状态：确认 `ONLINE`、`OFFLINE`、`BATTERY` 反馈正常。
3. 单车空载任务：WCS 下发 mission，AGV 空载到起点、到终点。
4. 单车带货任务：验证取货、到终点、放货反馈。
5. 异常任务：低电量、任务失败、超时、急停。
6. 断线恢复：断 MQTT、断 RCS、断 AGV 网络后恢复。
7. 幂等重放：重复发送相同任务和相同反馈，确认不重复执行/不重复回调。
8. PLC 联动：入库 `DROPPED` 后推进 PLC 上架，出库 `DROPPED` 后推进结果回传。
9. 长时间稳定性：连续任务和长时间运行。
10. 多车并发：多 AGV 同时任务、状态隔离、任务归属正确。


## 12. 真实性复核（2026-06-02）

复核目的：确认上次测试中哪些是真实项目代码，哪些是本地模拟输入，哪些是默认内存实现。

### 12.1 代码层面结论

| 链路/组件 | 代码复核结论 |
|---|---|
| WCS -> RCS HTTP 调用 | 真实 HTTP 客户端，不是 mock；`RCSClient` 调用 `/api/v1/wcs/agv/missions` |
| RCS mission API | 真实 Javalin API 路由，不是 demo 路由；测试使用 `/api/v1/wcs/agv/missions` |
| RCS -> WCS callback | 真实 HTTP callback sender，可发到 WCS callback URL |
| RCS -> openTCS | 有真实 HTTP client，也有默认 in-memory client |
| AGV adapter | 启动的是 catkin 编译出的真实 `ros_mqtt_modbus` 可执行文件 |
| AGV MQTT 输入 | 上次本地测试的输入由 `amqtt_pub` 人工发布，不是真实车端传感/导航产生 |
| WCS callback 后续业务 | 当前 WCS 只保存 AGV event 供查询，未推进真实 WMS/PLC 完整业务闭环 |

关键代码事实：

- RCS 如果没有配置 `rcs.openTcs.baseUrl` 或 `RCS_OPENTCS_BASE_URL`，会使用 `InMemoryOpenTcsOrderClient`。
- RCS 如果配置了 `RCS_OPENTCS_BASE_URL`，会使用 `HttpOpenTcsOrderClient` 调 openTCS Web API。
- AGV adapter 代码里确实订阅旧 topic `base_status` 和 `task_feedback`，并通过 `mg_mqtt_pub()` 发布新 RCS topic。
- `agv/{agv_id}/task/events` 和 `agv/{agv_id}/status` 是本项目为 RCS/WCS 统一反馈定义的标准化 MQTT 协议，不是 openTCS 官方协议；AGV 既有输入 topic 仍是 `base_status`、`task_feedback`、`feedback`。

### 12.2 上次测试中哪些是模拟的

| 项目 | 是否模拟 | 说明 |
|---|---|---|
| AGV adapter 程序 | 否 | 实际启动了 `devel/lib/ros_mqtt_modbus/ros_mqtt_modbus` |
| 旧 `task_feedback` 输入 | 是 | 用 `amqtt_pub` 人工发布，不是真实车运动产生 |
| MQTT broker | 是/本地替代 | 使用本地 `amqtt`，不是真实车端 MQTT Server |
| RCS MQTT 消费 | 否 | RCS 实际通过 Paho MQTT 订阅 broker |
| WCS HTTP 接口 | 否 | WCS 服务实际启动并接收 HTTP 请求 |
| openTCS transport order | 上次 AGV 测试默认未保证真实 | 因为当时 RCS 启动命令未显式配置 `RCS_OPENTCS_BASE_URL` |

### 12.3 真实 openTCS 补测

为排除 RCS 创建 transport order 只是 in-memory 的风险，2026-06-02 做了补测。

补测步骤：

1. 启动真实 openTCS Kernel。
2. 确认 Kernel HTTP API 可访问：`GET /v1/kernel/version`。
3. Kernel 加载模型 `modbus-opentcs-demo`，进入 `OPERATING` 状态。
4. 启动 RCS 时显式设置：

```text
RCS_OPENTCS_BASE_URL=http://127.0.0.1:55200
```

5. 通过 RCS 直接创建 mission：`M-REAL-OPENTCS-001`。
6. 查询 openTCS `/v1/transportOrders`，transport order 数量从 `0` 变成 `1`，订单名为 `M-REAL-OPENTCS-001`。
7. 再启动 WCS，通过 WCS 创建 mission：`M-WCS-REAL-OPENTCS-001`。
8. 查询 openTCS `/v1/transportOrders`，transport order 数量从 `1` 变成 `2`，订单列表包含 `M-WCS-REAL-OPENTCS-001`。

补测结果：

| 验证项 | 结果 |
|---|---|
| openTCS Kernel HTTP API | 通过 |
| RCS 显式使用真实 openTCS HTTP client | 通过 |
| RCS -> openTCS 创建 transport order | 通过 |
| WCS -> RCS -> openTCS 创建 transport order | 通过 |

补测证据摘要：

```text
GET /v1/kernel/version -> baselineVersion 7.3.0-SNAPSHOT
before orders: 0
RCS create M-REAL-OPENTCS-001 -> code 0
orders after RCS direct create: 1 [M-REAL-OPENTCS-001]
WCS create M-WCS-REAL-OPENTCS-001 -> code 0
orders after WCS create: 2 [M-REAL-OPENTCS-001, M-WCS-REAL-OPENTCS-001]
```

### 12.4 修正后的真实性判断

| 链路 | 真实性判断 |
|---|---|
| WCS -> RCS -> openTCS transport order | 已通过真实 openTCS Kernel 补测 |
| AGV adapter 旧 topic -> 新 RCS topic | 已通过真实 adapter 可执行文件测试 |
| AGV 反馈输入来源 | 仍是人工 MQTT 输入，不是真实车运动生成 |
| 真实 AGV 物理运动 | 未验证 |
| 真实车端 MQTT Server | 未验证 |
| PLC 实物联动 | 未验证 |

结论：现在可以确认 WCS/RCS/openTCS 创建订单链路不是纯模拟；AGV adapter 程序也不是模拟程序。但 AGV 反馈输入仍是人工发布 MQTT 消息，尚不能等同于真实小车现场运行结果。

## 13. 最终判断

```text
AGV MQTT 反馈协议和适配链路：完成
WCS/RCS/AGV adapter 本地联调：完成
WCS -> RCS -> 真实 openTCS 创建订单：完成（2026-06-02 补测）
真实小车现场联调：未完成
正式验收测试：未完成
```

这份测试结果可以作为进入真实小车联调前的软件集成测试依据。

## 14. RCS -> AGV 控制命令发布补测（2026-06-02）

补测目的：确认 RCS 创建搬运任务后，是否能按 AGV 现有 `robot_control` 协议发布控制命令；并确认 WCS 转发到 RCS 时也能触发同样的 MQTT 发布。

### 14.1 本次新增代码范围

| 项目 | 结论 |
|---|---|
| RCS command publisher | 已新增，默认关闭 |
| 控制 topic | `robot_control` |
| 控制 payload | 使用 AGV 代码已有的 `interest_point_control/start` 格式 |
| 点位映射 | 必须通过 `RCS_AGV_POINT_ID_MAP` 显式配置，不自动猜测 |
| 默认行为 | `RCS_AGV_COMMAND_ENABLED=false`，不影响现有小车运行 |

当前发布格式：

```json
{
  "cmd_type": "interest_point_control",
  "cmd": "start",
  "id": 2,
  "run_speed": 0.5,
  "path_stop_time": 0,
  "path_mode": 0,
  "circulates": 1,
  "time": 0
}
```

说明：`id=2` 来自测试配置 `RCS_AGV_POINT_ID_MAP='Point-01:1,Point-02:2'`，不是代码硬编码。

### 14.2 单元测试

执行命令：

```bash
JAVA_HOME=/home/shx/.local/jdk/jdk-21.0.10+7 \
PATH=/home/shx/.local/jdk/jdk-21.0.10+7/bin:$PATH \
./gradlew :opentcs-rcs-integration-sample:test
```

结果：

```text
BUILD SUCCESSFUL in 6s
```

覆盖点：

| 测试项 | 结果 |
|---|---|
| command publisher payload 构造 | 通过 |
| 缺少点位映射时拒绝发布 | 通过 |
| WcsMissionService 旧测试兼容 | 通过 |
| WcsTaskService 旧测试兼容 | 通过 |

### 14.3 RCS -> MQTT `robot_control` 发布测试

测试环境：

| 组件 | 使用对象 | 真实性边界 |
|---|---|---|
| RCS | 实际启动 `opentcs-rcs-integration-sample` Java 进程 | 真实 RCS sample 进程 |
| MQTT broker | 本地 `amqtt`，监听 `127.0.0.1:1883` | 不是实车 MQTT Server |
| MQTT 订阅 | `amqtt_sub -t robot_control -n 1` | 实际订阅 broker 捕获消息 |
| openTCS client | 此轮未配置 `RCS_OPENTCS_BASE_URL` | 使用 RCS 默认 in-memory openTCS client，仅验证 MQTT 发布 |
| AGV 实车 | 未连接 | 没有验证物理运动 |

RCS 启动关键配置：

```bash
RCS_AGV_COMMAND_ENABLED=true
RCS_AGV_COMMAND_BROKER_URI=tcp://127.0.0.1:1883
RCS_AGV_COMMAND_TOPIC=robot_control
RCS_AGV_POINT_ID_MAP='Point-01:1,Point-02:2'
```

创建任务：

```bash
curl -sS -X POST http://127.0.0.1:8090/api/v1/wcs/agv/missions \
  -H 'Content-Type: application/json' \
  -d '{"mission_no":"M-CMD-TEST-002","task_no":"T-CMD-TEST-002","from_point":"Point-01","to_point":"Point-02","pallet_no":"PLT-CMD-002","priority":50,"callback_url":"/api/wcs/agv/events"}'
```

RCS 返回：

```json
{"code":"0","msg":"OK","data":{"mission_no":"M-CMD-TEST-002","task_no":"T-CMD-TEST-002","rcs_status":"RECEIVED","idem_hit":false}}
```

MQTT 捕获到的 `robot_control` payload：

```json
{"cmd":"start","id":2,"path_mode":0,"time":0,"cmd_type":"interest_point_control","path_stop_time":0,"circulates":1,"run_speed":0.5}
```

结论：RCS 创建 mission 后，已能向 `robot_control` 发布 AGV 控制命令。

### 14.4 WCS -> RCS -> MQTT `robot_control` 发布测试

测试环境：

| 组件 | 使用对象 | 真实性边界 |
|---|---|---|
| WCS | 实际启动 `wcs_backend-main/run.py`，监听 `8000` | 真实 WCS HTTP 服务进程 |
| PLC | 项目自带 `MockModbusServer`，监听 `5020` | PLC 是模拟服务器，仅用于让 WCS 正常启动 |
| RCS | 实际启动 `opentcs-rcs-integration-sample`，监听 `8090` | 真实 RCS sample 进程 |
| MQTT broker | 本地 `amqtt` | 不是实车 MQTT Server |
| AGV 实车 | 未连接 | 没有验证物理运动 |

WCS 健康检查：

```json
{"status":"ok","devices":1,"rcs_enabled":true,"active_tasks":0}
```

通过 WCS 创建任务：

```bash
curl -sS -X POST http://127.0.0.1:8000/api/wcs/rcs/missions \
  -H 'Content-Type: application/json' \
  -d '{"mission_no":"M-WCS-CMD-TEST-001","task_no":"T-WCS-CMD-TEST-001","from_point":"Point-01","to_point":"Point-02","pallet_no":"PLT-WCS-CMD-001","priority":50}'
```

WCS 返回的 RCS 响应：

```json
{"code":"0","msg":"OK","data":{"mission_no":"M-WCS-CMD-TEST-001","task_no":"T-WCS-CMD-TEST-001","rcs_status":"RECEIVED","idem_hit":false}}
```

MQTT 捕获到的 `robot_control` payload：

```json
{"cmd":"start","id":2,"path_mode":0,"time":0,"cmd_type":"interest_point_control","path_stop_time":0,"circulates":1,"run_speed":0.5}
```

结论：WCS 创建 mission 后，经 RCS 可以触发 `robot_control` 控制命令发布。

### 14.5 真实性修正

| 链路 | 当前结论 |
|---|---|
| RCS -> MQTT `robot_control` | 已通过本地 broker 捕获验证 |
| WCS -> RCS -> MQTT `robot_control` | 已通过进程级链路验证 |
| RCS -> 真实 openTCS -> MQTT 同时开启 | 未在同一轮测试中验证 |
| 真实车端 MQTT Server | 未验证 |
| AGV 接收命令并运动 | 未验证 |
| 点位 id 与现场地图一致性 | 未验证 |

本次补测不能替代实车测试。它只能证明软件链路已经具备向 AGV 控制 topic 发布命令的能力。

## 15. 当前最终状态（2026-06-02 更新）

```text
AGV MQTT 反馈协议和适配链路：完成
RCS 订阅 AGV MQTT 状态并解析消息：完成
RCS -> WCS AGV event callback：完成
WCS -> RCS HTTP mission 接口：完成
WCS -> RCS -> 真实 openTCS 创建订单：完成
RCS -> MQTT robot_control 控制命令发布：完成，本地 broker 验证
WCS -> RCS -> MQTT robot_control 控制命令发布：完成，本地 broker 验证
真实车端 MQTT Server 联调：未完成
真实 AGV 物理运动：未完成
PLC 实物联动：未完成
正式验收测试：未完成
```

## 16. AGV 侧构建验证（2026-06-02）

验证目的：确认 `catkin_ws/src/ros_mqtt_modbus/src/ros_mqtt.cpp` 中新增的反馈 sidecar 逻辑没有破坏 AGV 小车侧包编译。

执行命令：

```bash
source /opt/ros/noetic/setup.bash
catkin_make -DCATKIN_WHITELIST_PACKAGES=ros_mqtt_modbus
```

结果：

```text
[100%] Built target ros_mqtt_modbus
```

说明：

| 项目 | 结论 |
|---|---|
| 编译结果 | 通过 |
| ROS 环境 | `/opt/ros/noetic` |
| 构建范围 | 仅白名单构建 `ros_mqtt_modbus` |
| 运行实车 | 未验证 |

构建中存在原项目警告，例如包名 `wj_716N_lidar` 命名不符合 ROS 规范、`generate_messages()` 没有消息文件等；这些不是本次 AGV MQTT sidecar 改动引入的编译错误。

## 17. 当前实现风险点（2026-06-02）

| 风险点 | 当前行为 | 建议 |
|---|---|---|
| RCS 创建 openTCS order 后 MQTT 发布失败 | 已改为 command outbox，mission 创建不再同步阻塞 MQTT 发布 | 现场仍建议确认是否需要失败后取消 openTCS order 的业务补偿策略 |
| `robot_control` client 每次发布临时连接 | 简单可靠，但高频任务下连接开销较大 | 实车稳定后可改成长连接 publisher |
| 只按 `to_point` 下发目标点 | 当前只覆盖点位导航到目标点 | 如果现场需要先到起点、取货、放货动作，需要扩展任务阶段命令 |
| 点位映射配置错误 | 会导致 AGV 去错误数字点位 | 实机前必须由现场地图逐点确认 |

## 18. 安全调整后复测（2026-06-02）

针对“openTCS order 已创建但 MQTT 发布失败不可追踪”的风险，已做最小修正：

| 调整项 | 当前行为 |
|---|---|
| mission 保存时机 | openTCS order 创建成功后立即保存 mission |
| MQTT 发布失败 | mission 状态更新为 `FAILED` 后抛出异常 |
| 成功路径 | 仍返回 `RECEIVED`，并保存幂等成功响应 |
| 后续建议 | command outbox/异步重试已完成；实机前仍需明确是否要增加取消 order 补偿策略 |

调整后重新执行 RCS 进程级 MQTT 发布测试：

```text
RCS create M-CMD-AFTER-FIX-001 -> code 0, rcs_status RECEIVED
robot_control captured -> {"cmd":"start","id":2,"path_mode":0,"time":0,"cmd_type":"interest_point_control","path_stop_time":0,"circulates":1,"run_speed":0.5}
```

结论：安全调整后，RCS 创建 mission 仍能发布 `robot_control` 控制命令。

## 19. AGV command outbox + 重试 + 查询接口（2026-06-02）

本次继续完成无实机前的软件收口：RCS 不再只同步发布 `robot_control`，而是把 AGV 控制命令写入 command outbox，由后台 scheduler 发布并失败重试。

### 19.1 新增能力

| 能力 | 状态 |
|---|---|
| AGV command outbox | 已实现 |
| command 幂等键 | 已实现，格式为 `mission_no:command_stage` |
| 后台发布 scheduler | 已实现 |
| MQTT 发布失败重试 | 已实现 |
| command 状态查询接口 | 已实现，`GET /api/v1/wcs/agv/missions/{mission_no}/commands` |
| file store 持久化 | 已实现，文件名 `agv-command-outbox-store.json` |

### 19.2 配置项

| 配置 | 默认值 | 说明 |
|---|---|---|
| `RCS_AGV_COMMAND_ENABLED` | `false` | 是否启用 AGV command outbox 和 MQTT 发布 |
| `RCS_AGV_COMMAND_BROKER_URI` | `tcp://127.0.0.1:1883` | MQTT broker |
| `RCS_AGV_COMMAND_TOPIC` | `robot_control` | AGV 控制 topic |
| `RCS_AGV_POINT_ID_MAP` | 无 | 点位映射，启用时必填 |
| `RCS_AGV_COMMAND_RETRY_TICK_MILLIS` | `1000` | command scheduler tick 间隔 |
| `RCS_AGV_COMMAND_DISPATCH_BATCH_SIZE` | `100` | 每轮最多发布 command 数 |

### 19.3 成功路径实测

测试环境：本地 `amqtt` broker，RCS Java 进程，未接真实 AGV。

创建 mission：`M-CMD-OUTBOX-001`。

创建响应：

```json
{"code":"0","msg":"OK","data":{"mission_no":"M-CMD-OUTBOX-001","task_no":"T-CMD-OUTBOX-001","rcs_status":"RECEIVED","idem_hit":false}}
```

查询 command：

```json
{"missionNo":"M-CMD-OUTBOX-001","taskNo":"T-CMD-OUTBOX-001","commandStage":"MISSION_START","idemKey":"M-CMD-OUTBOX-001:MISSION_START","status":"SUCCESS","retryCount":0,"nextRetryAt":null,"lastError":null}
```

MQTT 捕获：

```json
{"time":0,"path_mode":0,"id":2,"cmd":"start","run_speed":0.5,"circulates":1,"path_stop_time":0,"cmd_type":"interest_point_control"}
```

结论：command outbox 成功发布后状态变为 `SUCCESS`。

### 19.4 失败和自动重试实测

测试步骤：

1. 停止本地 MQTT broker。
2. RCS 保持运行。
3. 创建 mission：`M-CMD-OUTBOX-FAIL-001`。
4. 查询 command 状态。
5. 恢复 MQTT broker。
6. 等待超过首次 5 秒退避时间。
7. 再次查询 command 状态。

broker 停止时创建响应：

```json
{"code":"0","msg":"OK","data":{"mission_no":"M-CMD-OUTBOX-FAIL-001","task_no":"T-CMD-OUTBOX-FAIL-001","rcs_status":"RECEIVED","idem_hit":false}}
```

broker 不可用时 command 状态：

```json
{"missionNo":"M-CMD-OUTBOX-FAIL-001","commandStage":"MISSION_START","status":"FAILED","retryCount":1,"lastError":"Could not publish AGV robot_control command"}
```

broker 恢复后 command 状态：

```json
{"missionNo":"M-CMD-OUTBOX-FAIL-001","commandStage":"MISSION_START","status":"SUCCESS","retryCount":2,"nextRetryAt":null,"lastError":null}
```

结论：MQTT broker 临时不可用时，mission 创建不丢失 command；command 会进入 `FAILED` 并在 broker 恢复后自动重试成功。

### 19.5 真实性边界

| 项目 | 结论 |
|---|---|
| RCS Java 进程 | 真实运行 |
| MQTT 发布 | 真实发布到本地 broker |
| command outbox 状态查询 | 真实 HTTP 查询 |
| broker 故障恢复重试 | 已实际验证 |
| 真实车端 MQTT Server | 未验证 |
| AGV 实车运动 | 未验证 |

### 19.6 `nextRetryAt` 序列化说明

实测 `/commands` 接口中 `nextRetryAt` 由 Javalin 默认 JSON mapper 输出为数字时间戳，例如：

```json
"nextRetryAt": 1780381116.604255349
```

因此接口文档已按当前真实输出修正为 `number/null`。如果后续 WCS 需要 ISO-8601 字符串，可以再统一替换 Javalin JSON mapper。
