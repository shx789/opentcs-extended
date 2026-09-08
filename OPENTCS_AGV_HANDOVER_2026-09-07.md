# openTCS AGV 联调交接文档

更新时间：2026-09-07  
适用范围：当前 openTCS + RCS + MQTT AGV 联调方案  
当前已验证发布包：`release/opentcs-agv-suite-v1.0.16-20260905.zip`

## 1. 文档目的

本文用于现场联调和研发交接，帮助新成员快速了解：

- 系统整体架构和各模块职责；
- Windows 发布包的部署和启动方式；
- WCS/RCS、openTCS、AGV 之间的接口关系；
- MOVE_ONLY、PICK、DROP 三类任务的执行逻辑；
- MQTT 反馈格式和适配器状态机；
- 地图、点位、车辆配置方法；
- 现场测试、日志查看和常见问题处理方式。

本文描述的是当前代码实际实现，不代表 openTCS 原生所有能力。

## 2. 总体架构

### 2.1 正式控制链路

```text
WMS/WCS
   │ HTTP 创建业务任务
   ▼
RCS（8090）
   │ HTTP 创建 openTCS Transport Order
   ▼
openTCS Kernel（55200）
   │ 调度 Transport Order、生成 MovementCommand
   ▼
MqttCommAdapter（运行在 openTCS Kernel 内）
   │ MQTT 发布 robot_control
   ▼
AGV MQTT Broker / AGV 控制器
   │
   ├── task_feedback：导航或 magnetic_nav 动作反馈
   └── base_status：车辆、位置、电量、顶升、物料状态
```

任务完成反馈链路如下：

```text
AGV / openTCS Transport Order 状态
   ▼
openTCS SSE
   ▼
RCS OpenTcsSseEventConsumer
   ▼
Mission / WCS Task 状态更新
   ▼
回调 outbox
   ▼
WMS/WCS callback_url
```

### 2.2 控制职责边界

| 模块 | 主要职责 | 是否直接控制 AGV |
|---|---|---|
| WMS | 业务库存、入库、出库等业务请求 | 否 |
| WCS | 业务任务来源，可调用 RCS API | 当前方案中不直接发 AGV MQTT |
| RCS | 接收业务任务、创建 openTCS 订单、跟踪任务和回调 | 默认不直接控制 AGV |
| openTCS Kernel | 路径规划、资源分配、车辆调度、订单状态管理 | 通过适配器间接控制 |
| MqttCommAdapter | 将 openTCS MovementCommand 转成 AGV MQTT 命令，并解析反馈 | 是，负责发布 MQTT |
| AGV | 执行导航、顶升动作并上报状态 | 是实际执行端 |

默认配置保持：

```json
"enable_rcs_direct_command_bridge": false
```

不要让 RCS 直控 MQTT 和 openTCS 适配器同时控制同一台车，否则会出现重复下发、命令抢占和状态不一致。

## 3. 代码模块

### 3.1 openTCS 核心模块

| 模块 | 作用 |
|---|---|
| `opentcs-kernel` | openTCS Kernel、调度、车辆控制和 HTTP 服务 |
| `opentcs-api-base` | openTCS 基础数据模型和 API |
| `opentcs-common` | 公共工具和基础设施 |
| `opentcs-commadapter-loopback` | 原有回环/模拟车辆适配器 |
| `opentcs-commadapter-mqtt` | 当前 AGV MQTT 通信适配器 |
| `opentcs-kernelcontrolcenter` | Kernel 控制中心 |
| `opentcs-modeleditor` | Plant Model 编辑器 |
| `opentcs-operationsdesk` | 运行监控和操作界面 |

MQTT 适配器核心文件：

```text
opentcs-commadapter-mqtt/src/main/java/org/opentcs/mqttvehicle/MqttCommAdapter.java
```

### 3.2 RCS 模块

代码目录：

```text
opentcs-rcs-integration-sample/src/main/java/org/opentcs/rcs
```

主要职责：

- 提供 WCS/Mission HTTP API；
- 将 Mission 转成 openTCS Transport Order；
- 处理订单幂等，避免上游重试造成重复任务；
- 订阅 openTCS SSE，更新 Mission、WCS Task 和回调 outbox；
- 提供回调查询和 AGV 监控页面；
- 可选订阅 AGV 状态，但正式控制链路不依赖 RCS 直控 AGV。

重点类：

```text
WcsMissionService.java
OpenTcsPayloadMapper.java
HttpOpenTcsOrderClient.java
OpenTcsSseTransportOrderSubscriber.java
OpenTcsSseEventConsumer.java
WmsTaskResultService.java
AgvMonitorHttpHandlers.java
```

### 3.3 Windows AGV 配置和辅助工具

目录：

```text
agv-opentcs-adapter-windows
```

主要内容：

- `config/runtime_config.json`：运行配置；
- `bin/agv_config_web.py`：8091 配置页面；
- `bin/generate_map_topology.py`：根据地图生成候选拓扑；
- `bin/agv_native_feedback_adapter.py`：可选的原生 AGV 反馈适配服务；
- `bin/agv_no_car_feedback_simulator.py`：无真实车辆时的模拟器；
- `web/`：配置页面静态资源；
- `release/`：Windows 辅助程序发布目录。

## 4. 端口和服务

| 服务 | 默认地址 | 说明 |
|---|---|---|
| openTCS Kernel HTTP | `http://127.0.0.1:55200` | openTCS Web API |
| RCS | `http://127.0.0.1:8090` | WCS/RCS 接口、Mission、回调 |
| 配置页面 | `http://127.0.0.1:8091` | MQTT、地图、点位配置 |
| 联调控制台 | `http://127.0.0.1:8092` | 状态、订单、MQTT 观察和测试 |
| 本地 MQTT Broker | `tcp://127.0.0.1:1883` | 无真实车或本地模拟时使用 |
| 真实 AGV Broker | 例如 `tcp://192.168.1.100:1883` | 由现场 AGV 提供 |

真实车辆部署时，Windows 电脑必须能访问 AGV 的 MQTT IP。配置页面中使用 AGV Broker 地址后，启动脚本不会再启动本地 Broker。

## 5. 发布包部署

### 5.1 推荐发布包

当前已验证发布包：

```text
release/opentcs-agv-suite-v1.0.16-20260905.zip
```

发布包包含：

```text
opentcs/       openTCS Kernel、OperationsDesk、ModelEditor 和 Java Runtime
rcs/           RCS fat jar
adapter/       Windows 配置工具、配置文件、地图和辅助程序
dashboard/     8092 联调控制台
logs/          启动后生成的日志
start-all.bat  启动入口
stop-all.bat   停止入口
```

### 5.2 现场部署步骤

1. 停止旧版本的所有服务，避免旧 Kernel 占用 `55200`。
2. 解压发布包到现场工作目录，路径尽量不要包含特殊字符。
3. Windows 电脑连接 AGV WiFi 或现场网络。
4. 编辑 `adapter/config/runtime_config.json`，确认 MQTT 和 openTCS 配置。
5. 确认 Plant Model 已加载到当前 Kernel。
6. 双击 `start-all.bat`。
7. 检查 `8091` 配置页面和 `8092` 联调控制台。
8. 先做 MQTT 连接检查，再做 MOVE_ONLY，最后做 PICK/DROP。

### 5.3 启停命令

启动：

```bat
start-all.bat
```

停止：

```bat
stop-all.bat
```

如果脚本被 PowerShell 执行策略拦截，可使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\start-suite.ps1 -KeepAlive
```

### 5.4 真实车辆配置示例

```json
{
  "mqtt_uri": "mqtt://192.168.1.100:1883/",
  "source_topic": "task_feedback",
  "command_topic": "robot_control",
  "enable_rcs_direct_command_bridge": false,
  "open_tcs_base_url": "http://127.0.0.1:55200",
  "rcs_base_url": "http://127.0.0.1:8090",
  "vehicle_name": "Vehicle-01",
  "agv_id": "AGV_01"
}
```

## 6. openTCS Plant Model 和点位

### 6.1 点位层级

当前通常存在两类名称：

```text
Point：    ST_IN_01、P_WAIT_IN_01
Location： LOC_ST_IN_01、LOC_P_WAIT_IN_01
```

AGV 导航使用 Point；PICK/DROP 业务目的地通常使用 Location，并关联到对应 Point。

例如：

```text
LOC_P_WAIT_IN_01:PICK
LOC_ST_IN_01:DROP
```

openTCS 内部会把一个带操作的 Drive Order 拆成：

```text
导航到目标 Point
到达后执行目标操作
```

因此带 `PICK` 或 `DROP` 的导航 MovementCommand 仍然是导航命令，不能直接当成顶升命令。

### 6.2 地图重建

当重新扫描得到新的 ROS 地图时，需要重新生成候选 Plant Model：

1. 在 `8091` 上传新的 `map.yaml` 和对应的 `map.pgm`；
2. 选择生成粒度；
3. 检查候选点位、路径、方向和闭环；
4. 修改业务点名，例如 `ST_IN_01`、`P_WAIT_IN_01`；
5. 检查 AGV 点位 ID 映射；
6. 加载候选 Plant Model 到 openTCS；
7. 重新确认 `Vehicle-01`、车辆当前位置和路径可达性。

地图生成结果通常位于：

```text
adapter/config/generated/
```

常见文件：

```text
opentcs_plant_model_business.json
opentcs_plant_model_candidate.json
agv_opentcs_business_mapping.json
generation_status.json
```

地图重建后，旧点位名、旧路径和旧车辆引用可能导致 `404` 或订单不可达。加载新模型前要确认模型内容完整，特别是车辆名仍为 `Vehicle-01`。

### 6.3 MQTT 点位 ID 映射

推荐使用：

```text
nav:0=ST_IN_01,nav:1=P_WAIT_IN_01,nav:2=ST_OUT_01
```

也支持兼容格式：

```text
0=ST_IN_01,1=P_WAIT_IN_01
```

如果 AGV 的反馈是：

```json
{
  "cmd_type": "task_feedback",
  "type": "nav",
  "id": 0,
  "status": "success"
}
```

适配器需要通过 `nav:0=ST_IN_01` 将反馈反向映射为 openTCS Point。

## 7. WCS/RCS 接口

RCS 默认监听：

```text
http://127.0.0.1:8090
```

### 7.1 创建普通 Mission

接口：

```http
POST /api/v1/wcs/agv/missions
Content-Type: application/json
```

请求示例：

```json
{
  "mission_no": "M-WEB-001",
  "task_no": "T-WEB-001",
  "from_point": "P_WAIT_IN_01",
  "to_point": "ST_IN_01",
  "pallet_no": "PALLET-001",
  "priority": 50,
  "callback_url": "http://127.0.0.1:8090/api/v1/wms/inbound-results",
  "mission_type": "TRANSPORT",
  "from_operation": "PICK",
  "to_operation": "DROP"
}
```

创建结果中的：

```json
{
  "rcs_status": "RECEIVED"
}
```

只表示 RCS 已接收并创建流程，不表示 AGV 已经开始执行。

### 7.2 MOVE_ONLY

请求中指定：

```json
{
  "mission_type": "MOVE_ONLY"
}
```

或者显式指定：

```json
{
  "from_operation": "MOVE",
  "to_operation": "MOVE"
}
```

MOVE_ONLY 只导航，不执行顶升。

### 7.3 查询和取消 Mission

查询：

```http
GET /api/v1/wcs/agv/missions/{mission_no}
```

列表：

```http
GET /api/v1/wcs/agv/missions
```

查询 Mission 对应的 AGV command：

```http
GET /api/v1/wcs/agv/missions/{mission_no}/commands
```

取消：

```http
POST /api/v1/wcs/agv/missions/{mission_no}/cancel
```

### 7.4 WMS 入库/出库任务

入库：

```http
POST /api/v1/wcs/inbound/tasks
```

出库：

```http
POST /api/v1/wcs/outbound/tasks
```

查询业务任务：

```http
GET /api/v1/wcs/tasks/{biz_task_no}
```

取消业务任务：

```http
POST /api/v1/wcs/tasks/{biz_task_no}/cancel
```

### 7.5 回调和运行状态

查询 RCS 回调 outbox：

```http
GET /api/v1/rcs/callbacks
```

查询 RCS 到 openTCS SSE 的状态：

```http
GET /api/v1/wcs/agv/runtime
```

正常应确认：

```json
{
  "open_tcs_sse_enabled": true
}
```

openTCS 事件人工注入接口，仅用于测试：

```http
POST /api/v1/opentcs/events/transport-orders
```

生产联调应以 openTCS 实际 SSE 事件为准，不建议手工注入完成事件伪造闭环。

## 8. MQTT 接口

### 8.1 适配器发布导航命令

Topic：

```text
robot_control
```

典型格式：

```json
{
  "cmd_type": "interest_point_control",
  "cmd": "start",
  "id": 0,
  "run_speed": 0.5,
  "path_stop_time": 0,
  "path_mode": 2,
  "circulates": 0,
  "time": 0,
  "opentcs_vehicle": "Vehicle-01",
  "opentcs_order": "M-WEB-001",
  "opentcs_dest": "ST_IN_01",
  "opentcs_step_dest": "ST_IN_01"
}
```

### 8.2 适配器发布升顶/降顶命令

Topic：

```text
robot_control
```

升顶：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 3
}
```

降顶：

```json
{
  "cmd_type": "magnetic_nav",
  "aim_id": 0,
  "aim_dir": 0,
  "aim_action": 4
}
```

适配器会附加 openTCS 上下文字段，例如车辆名和操作名，AGV 控制器通常只需使用协议所需字段。

### 8.3 导航反馈

Topic：

```text
task_feedback
```

开始：

```json
{
  "cmd_type": "task_feedback",
  "type": "nav",
  "id": 0,
  "status": "start"
}
```

成功：

```json
{
  "cmd_type": "task_feedback",
  "type": "nav",
  "id": 0,
  "status": "success"
}
```

### 8.4 magnetic_nav 反馈

开始：

```json
{
  "cmd_type": "task_feedback",
  "type": "magnetic_nav",
  "id": 0,
  "status": "start"
}
```

成功：

```json
{
  "cmd_type": "task_feedback",
  "type": "magnetic_nav",
  "id": 0,
  "status": "success"
}
```

失败时应使用可识别的失败状态，例如：

```json
{
  "cmd_type": "task_feedback",
  "type": "magnetic_nav",
  "id": 0,
  "status": "failed"
}
```

### 8.5 base_status

Topic：

```text
base_status
```

典型结构：

```json
{
  "cmd_type": "base_status",
  "bms": {
    "soc": 42,
    "voltage": 21.6,
    "remaining_capacity": 9240,
    "status": 2
  },
  "pose": {
    "x": 0.219,
    "y": 0.331,
    "yaw": -0.253
  },
  "robot": {
    "mainerror": 0,
    "suberror": 0,
    "robot_status": 0,
    "maintask": 0,
    "subtask": 0
  },
  "magnetic": {
    "material": false,
    "up": false,
    "down": true
  }
}
```

适配器目前重点解析：

```text
bms.soc
robot.mainerror / robot.suberror
robot.robot_status
magnetic.material
magnetic.up
magnetic.down
magnetic.low
```

如果 AGV 上报的是全零 BMS 占位数据：

```json
{
  "soc": 0,
  "voltage": 0.0,
  "remaining_capacity": 0,
  "status": 0
}
```

适配器不会用它把车辆电量覆盖成 0。当前模型默认电量和阈值也按测试阶段可运行策略处理。

## 9. PICK/DROP 业务逻辑

### 9.1 取货的正确现场顺序

```text
1. AGV 到达 A 点
2. 确认顶升下降到位：down=true
3. 将物料放到 AGV 顶升平台
4. material=true
5. openTCS 下发 magnetic_nav aim_action=3
6. AGV 返回 magnetic_nav task_feedback success
7. base_status 返回 material=true、up=true
8. PICK 完成
9. openTCS 继续导航到 B 点
```

### 9.2 放货的正确现场顺序

```text
1. AGV 带货到达 B 点
2. openTCS 下发 magnetic_nav aim_action=4
3. AGV 顶升下降，down=true
4. 人工或设备取走物料
5. material=false
6. AGV 返回 magnetic_nav task_feedback success
7. DROP 完成
8. Transport Order 完成，RCS 通过 SSE 更新 Mission 和回调
```

### 9.3 PICK/DROP 双确认

实际下发升降命令时，完成条件是两个来源同时满足：

PICK：

```text
task_feedback.type=magnetic_nav
task_feedback.status=success
base_status.magnetic.material=true
base_status.magnetic.up=true
```

DROP：

```text
task_feedback.type=magnetic_nav
task_feedback.status=success
base_status.magnetic.material=false
base_status.magnetic.down=true
```

反馈先后顺序不要求固定。适配器会分别记录动作成功和最新传感器状态，两个条件满足后才完成升降 MovementCommand。

### 9.4 已有物料的特殊情况

如果任务开始前最新 `base_status` 已经是：

```json
{
  "material": true,
  "up": true,
  "down": false
}
```

执行 PICK 时会跳过再次发送升顶命令，直接完成 PICK，然后继续导航。

如果任务开始前已经是：

```json
{
  "material": false,
  "up": false,
  "down": true
}
```

执行 DROP 时可以跳过降顶命令，直接认为目标状态已满足。

注意：只有真正的无路径升降 MovementCommand 才会发送 `magnetic_nav`。带路径且操作名为 `PICK` 或 `DROP` 的 MovementCommand 是导航段，必须先用 `type=nav,status=success` 完成导航。

## 10. 调试方法

### 10.1 推荐测试顺序

```text
1. 服务健康检查
2. MQTT 连接检查
3. 查看 Vehicle-01 状态
4. MOVE_ONLY 单段导航
5. 已有物料场景：PICK 跳过 + 导航
6. 正常取货：PICK
7. 正常放货：DROP
8. 完整 Mission/RCS 回调
```

不要一开始直接测试完整搬运任务，否则容易把 MQTT、Plant Model、车辆状态、PICK、DROP、SSE 多个问题混在一起。

### 10.2 HTTP 健康检查

```powershell
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:55200/v1/kernel/version
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:55200/v1/vehicles
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:8090/api/v1/wcs/agv/runtime
```

重点检查车辆：

```json
{
  "name": "Vehicle-01",
  "integrationLevel": "TO_BE_UTILIZED",
  "procState": "IDLE",
  "state": "IDLE",
  "paused": false,
  "transportOrder": null,
  "currentPosition": "ST_IN_01"
}
```

### 10.3 MQTT 观察

现场至少同时观察：

```text
robot_control
task_feedback
base_status
```

建议记录每条消息的时间、Topic 和完整 JSON，尤其不要只看摘要日志，因为 `base_status` 的 `material/up/down` 决定升降是否完成。

### 10.4 关键日志关键词

openTCS/适配器日志重点搜索：

```text
Connected MQTT broker
Received MQTT feedback
Mapped MQTT feedback
Published MQTT command
Published MQTT lift command
magnetic_nav operation
drive order finished
Processing successful MQTT feedback
MQTT success did not match
AGV base_status reports
```

RCS 日志重点搜索：

```text
SSE
openTCS
callback
RECEIVED
DONE
FAILED
disconnected
```

### 10.5 正常搬运日志序列

已有物料且已升顶时：

```text
Lift operation PICK already satisfied by base_status; skipping MQTT command.
drive order finished ... PICK
Published MQTT command ... 放货点 ...
Received MQTT feedback ... type=nav ... status=success
drive order finished ... 放货点:MOVE
Published MQTT lift command ... operation=DROP
```

正常取货时：

```text
Published MQTT lift command ... operation=PICK
Received MQTT feedback ... type=magnetic_nav ... status=start
Received MQTT feedback ... type=magnetic_nav ... status=success
base_status material=true up=true
drive order finished ... PICK
Published MQTT command ... 下一导航点
```

正常放货时：

```text
Published MQTT lift command ... operation=DROP
Received MQTT feedback ... type=magnetic_nav ... status=start
Received MQTT feedback ... type=magnetic_nav ... status=success
base_status material=false down=true
drive order finished ... DROP
Transport order ... FINISHED
```

## 11. 常见问题

### 11.1 订单一直是 RECEIVED

`RECEIVED` 是 RCS Mission 初始状态，不等于 openTCS 已完成。

依次检查：

1. `GET /api/v1/wcs/agv/runtime` 是否显示 `open_tcs_sse_enabled=true`；
2. RCS 是否使用正确的 `RCS_OPENTCS_BASE_URL`；
3. openTCS SSE 是否连接成功；
4. openTCS Transport Order 是否已经 `FINISHED`；
5. `GET /api/v1/rcs/callbacks` 是否存在 `PENDING`、`SUCCESS` 或 `FAILED` 记录。

历史订单不会因为后来开启 SSE 自动补发事件，必须用新订单验证。

### 11.2 订单一直 DISPATCHABLE

表示订单已可调度，但当前没有可分配车辆。检查：

```text
integrationLevel=TO_BE_UTILIZED
procState=IDLE
state=IDLE
paused=false
transportOrder=null
currentPosition 不为空且存在于当前 Plant Model
```

还要检查车辆是否 stranded、是否仍绑定旧订单、是否有旧任务处于 `BEING_PROCESSED`。

### 11.3 车辆收到导航但 openTCS 不推进

检查 AGV 是否返回：

```json
{
  "cmd_type": "task_feedback",
  "type": "nav",
  "id": 0,
  "status": "success"
}
```

再检查 `nav:0=目标Point` 映射是否正确。新版本适配器会打印：

```text
Received MQTT feedback
Mapped MQTT feedback
Processing successful MQTT feedback
```

### 11.4 到放货点后没有降顶命令

先确认是否已经出现：

```text
drive order finished ... 放货点:MOVE
```

如果没有，问题在导航反馈完成；如果有但没有：

```text
Published MQTT lift command ... operation=DROP
```

再检查当前 openTCS 适配器 JAR 是否为对应发布版，以及当前下一条 MovementCommand 是否为无路径 DROP。

### 11.5 只看到 base_status 循环，任务不完成

`base_status` 持续上报本身是正常的。需要查看完整字段：

```text
PICK：material=true、up=true
DROP：material=false、down=true
```

如果 DROP 时物料还在车上：

```text
material=true
```

适配器会继续等待，不会把放货任务标记完成。正确操作是先降顶，再把物料从车上取走。

### 11.6 PICK/DROP 命令重复发送

检查是否使用旧版适配器，或者日志中是否存在：

```text
普通 task_feedback success
→ 再次 Published MQTT lift command
```

PICK/DROP 的完成不能只依据普通导航 success；实际升降命令必须等待 `magnetic_nav success + base_status`。

### 11.7 openTCS 加载 Plant Model 返回 404

确认：

- 配置的是 Kernel 基础地址，例如 `http://127.0.0.1:55200`，不要把 `/v1` 再填进去；
- 当前模型中没有旧点位、旧车辆或旧 Location 引用；
- 新地图生成的点位名和任务请求中的点位名一致；
- Plant Model JSON 是完整候选模型，不是只有映射文件。

### 11.8 `Object name already exists`

openTCS 中订单名必须唯一。上游超时后重复提交时，第一次可能已经成功，后续重试就会报：

```text
Object name already exists
```

调用方应使用稳定的 `mission_no` 并实现幂等查询，不要每次重试都生成新的不一致业务状态。

### 11.9 电量为 0 或车辆无法调度

部分 AGV 在 BMS 不可用时会上报全零占位数据。适配器会忽略这种占位值；模型测试阶段默认电量阈值配置为：

```json
{
  "energyLevelCritical": 0,
  "energyLevelGood": 90,
  "energyLevelFullyRecharged": 90,
  "energyLevelSufficientlyRecharged": 0
}
```

如果车辆仍不可调度，直接查询 `/v1/vehicles`，不要只看 BMS。

### 11.10 修改点位后反馈匹配失败

修改点位名后要同时更新：

```text
Plant Model 中的 Point/Location
runtime_config.json 的 point_id_map
RCS 需要的 AGV 点位映射
任务请求中的 from_point/to_point
```

推荐保留 `nav:id=PointName` 映射，并在新模型加载后重启或重新连接适配器。

## 12. 现场交接检查表

### 部署前

- [ ] Windows 电脑可访问 AGV MQTT Broker；
- [ ] `mqtt_uri`、`robot_control`、`task_feedback`、`base_status` 正确；
- [ ] `Vehicle-01` 存在于 Plant Model；
- [ ] 车辆点位 ID 映射已确认；
- [ ] RCS `open_tcs_base_url` 正确；
- [ ] 发布包版本已记录；
- [ ] 旧服务已停止，端口未被占用。

### MOVE_ONLY 验证

- [ ] 订单进入 `BEING_PROCESSED`；
- [ ] 车辆收到 `robot_control`；
- [ ] AGV 返回 `nav start`；
- [ ] AGV 返回 `nav success`；
- [ ] openTCS Transport Order 进入完成状态；
- [ ] RCS Mission 从 `RECEIVED` 更新到 `DONE`。

### PICK 验证

- [ ] 到达取货点前顶升为下降状态；
- [ ] 物料放上车辆后 `material=true`；
- [ ] 发送 `aim_action=3`；
- [ ] 收到 `magnetic_nav success`；
- [ ] 收到 `material=true、up=true`；
- [ ] 只发送一次 PICK；
- [ ] 完成后才开始下一段导航。

### DROP 验证

- [ ] 车辆带货到达放货点；
- [ ] 发送 `aim_action=4`；
- [ ] 顶升下降到位 `down=true`；
- [ ] 现场取走物料；
- [ ] `material=false`；
- [ ] 收到 `magnetic_nav success`；
- [ ] DROP Drive Order 完成；
- [ ] Transport Order、Mission、WCS 回调均完成。

## 13. 交接时应保留的信息

每次现场问题至少保留以下内容：

```text
1. 发布包版本
2. runtime_config.json 中的非敏感配置
3. Plant Model 名称和加载时间
4. Vehicle-01 的 /v1/vehicles JSON
5. Transport Order 的完整 JSON
6. RCS Mission 查询结果
7. RCS runtime 状态
8. robot_control 消息
9. task_feedback 消息
10. base_status 完整消息
11. Kernel、RCS、适配器对应时间段日志
```

现场排障时必须使用绝对时间，例如 `2026-09-07 15:20:30`，不要只描述“刚才”或“过一会儿”。

## 14. 当前已知限制

- 当前方案按单车 `Vehicle-01` 现场联调设计，多车需要扩展 AGV、车辆和映射配置；
- `PICK/DROP` 的物理完成依赖 AGV 正确上报 `magnetic_nav task_feedback success` 和 `base_status.magnetic`；
- 如果 AGV 不上报失败反馈，适配器无法仅凭 MQTT 心跳判断动作失败，需要补充 AGV 协议或增加超时策略；
- 地图重新生成后必须重新校验业务点位、路径、方向、Location 关联和车辆初始位置；
- RCS 回调依赖 openTCS SSE。历史完成事件不会在 SSE 后自动补发；
- 当前版本的 8092 页面主要用于联调观察，不应替代 WMS/WCS 正式业务入口；
- 真实部署时不要同时启用 RCS 直控 AGV 和 openTCS MqttCommAdapter。

## 15. 快速判断流程

遇到“任务不动”时按以下顺序判断：

```text
订单是否创建？
  └─ 否：检查 RCS 请求和 openTCS API

是否 BEING_PROCESSED？
  └─ 否：检查车辆 integration/procState/state/paused/stranded

是否发布 robot_control？
  └─ 否：检查适配器、MQTT 连接和 MovementCommand

AGV 是否返回 task_feedback？
  └─ 否：检查 AGV 控制器和反馈 Topic

导航是否返回 nav success？
  └─ 否：检查 AGV 导航执行和点位 ID

是否完成当前导航 Drive Order？
  └─ 否：检查反馈映射、Point/Location 名称和适配器日志

是否发布 magnetic_nav？
  └─ 否：检查是否已经进入无路径 PICK/DROP 动作

升降是否双确认？
  └─ 否：检查 magnetic_nav success、material、up、down

Transport Order 是否 FINISHED？
  └─ 否：继续看 openTCS 订单和车辆状态

Mission 是否 DONE、回调是否 SUCCESS？
  └─ 否：检查 RCS SSE、回调 outbox 和 callback_url
```

## 16. 版本记录

| 版本 | 日期 | 主要内容 | 现场状态 |
|---|---|---|---|
| `v1.0.12` | 2026-09-05 | RCS openTCS SSE 状态同步、测试阶段默认电量策略 | 历史版本 |
| `v1.0.13` | 2026-09-05 | PICK/DROP 升降双确认、已有物料状态处理、升降稳定等待 | 历史版本 |
| `v1.0.14` | 2026-09-05 | MQTT 反馈入口、点位映射和 `LOC_` 兼容诊断日志 | 历史版本 |
| `v1.0.15` | 2026-09-05 | 修复带 `DROP` 操作的导航段被误当成降顶动作 | 历史版本 |
| `v1.0.16` | 2026-09-05 | 增加 PICK/DROP 导航段回归保护、重复升降请求保护和升降失败处理 | 当前推荐 |

### 16.1 当前推荐验证边界

当前发布包已经完成本地构建和 MQTT 适配器测试。以下内容仍应由现场使用真实 AGV 验证：

- 从取货点到放货点的完整真实搬运闭环；
- AGV 实际 `magnetic_nav` 升降动作与 `base_status` 传感器状态的时序；
- 物料未取走时 DROP 是否按预期保持等待；
- 网络断开、AGV 动作失败和重连后的恢复行为；
- WMS/WCS 回调地址在现场网络下是否可达。

### 16.2 交接原则

- 现场问题先记录版本，再记录绝对时间；
- 先确认哪一层没有推进，再修改代码；
- 不要用 RCS 直控 MQTT 绕过 openTCS 复现问题；
- 不要用手工注入完成事件替代真实 AGV 反馈验证；
- 任何协议字段变更都应同步更新适配器解析、测试样例和本文档。
