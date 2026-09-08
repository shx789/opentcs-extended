# AGV 实机联调检查表

## 1. 当前状态

目前已经完成：

| 项目 | 状态 |
|---|---|
| WCS -> RCS HTTP mission | 已通过 |
| RCS -> 真实 openTCS transport order | 已通过 |
| AGV adapter 旧反馈 topic -> RCS 新反馈 topic | 已通过本地软件联调 |
| RCS 接收 AGV MQTT 反馈并回调 WCS | 已通过 |
| 真实 AGV 小车运动 | 未验证 |
| 真实车端 MQTT Server | 未验证 |
| RCS -> AGV 控制命令下发 | 当前尚未完整实现/验证 |
| PLC/堆垛机实物联动 | 未验证 |

关键判断：下一步不是继续本地模拟，而是接真实 AGV MQTT 和真实车端控制协议。

## 2. 实机联调前必须确认的信息

| 信息 | 必填 | 说明 |
|---|---|---|
| AGV MQTT Broker IP | 是 | 确认是车端 broker 还是现场统一 broker |
| MQTT 端口 | 是 | 默认可能是 `1883`，现场必须确认 |
| MQTT 用户名/密码 | 是 | 如果有认证，RCS/AGV adapter 都要配置 |
| 控制 topic | 是 | 当前 AGV 代码使用 `robot_control` |
| 状态 topic | 是 | 当前 AGV 代码订阅/解析 `base_status` |
| 任务反馈 topic | 是 | 当前 AGV 代码订阅/解析 `task_feedback` |
| 其他反馈 topic | 否 | 文档提到 `feedback`，需要确认真实用途 |
| AGV 编号 | 是 | 例如 `AGV_01`，需和 WCS/RCS/openTCS 一致 |
| 点位编号 | 是 | WCS 点位、openTCS 点位、AGV 点位需统一 |
| 地图/模型 | 是 | openTCS plant model 与 AGV 地图点位映射 |
| 真实 payload 样例 | 是 | 至少抓 `base_status`、`task_feedback`、`feedback` 各 3-5 条 |

## 3. 真实 AGV MQTT 报文采集

先只监听，不下发控制命令。

需要采集：

| Topic | 需要样例 | 目标 |
|---|---|---|
| `base_status` | 3-5 条 | 确认在线、电量、故障、位置字段 |
| `task_feedback` | 3-5 条 | 确认任务状态枚举和点位字段 |
| `feedback` | 3-5 条 | 确认是否有补充任务/导航反馈 |

采集后要核对这些字段是否和当前代码一致：

| 当前代码字段 | 真实车是否必须确认 |
|---|---|
| `cmd_type=base_status` | 是 |
| `bms.soc` | 是，当前作为电量 |
| `robot.status` | 是，当前用于判断运行状态/错误状态 |
| `robot.mainerror` | 是，当前用于错误判断 |
| `robot.suberror` | 是，当前用于错误判断 |
| `bms.error` | 是，当前用于错误判断 |
| `cmd_type=task_feedback` | 是 |
| `status=process/success/failure/timeout` | 是 |
| `id` | 是，当前可作为点位/任务 id |
| `type=point/nav/charge/track/location/magnetic_nav` | 是 |
| `mission_no/task_no/point_id` | 是，真实车可能没有，需要 RCS 内部映射 |

## 4. AGV 现有控制 topic 与 JSON 格式

从 `ros_mqtt.cpp` 代码复核，AGV 旧控制 topic 是：

```text
robot_control
```

当前代码会向 `robot_control` 发布这些控制报文：

| 控制类型 | `cmd_type` | 关键字段 | 说明 |
|---|---|---|---|
| 速度控制 | `move` | `vx`, `vy`, `vz` | 直接速度控制 |
| 兴趣点/巡航点 | `interest_point_control` | `cmd`, `id`, `run_speed`, `path_stop_time`, `path_mode`, `circulates`, `time` | 点位导航，`cmd=start/stop/random` |
| 轨迹点 | `trajectory_point_control` | `cmd`, `id`, `dir`, `run_speed`, `stop_time`, `circulates` | 轨迹导航，`cmd=start/stop` |
| 多任务 | `more_task_control` | `cmd`, `main_task`, `sub_task`, `main_loop`, `sub_loop`, `run_speed`, `stop_time` | 多任务启动/暂停/停止 |
| 回充 | `charge_point_control` | `cmd`, `id`, `run_speed` | 回充点控制 |
| 错误复位 | `error_reset` | 无 | 清故障 |
| 重定位/复位 | `reset` | 无 | 重定位/复位 |
| 软件急停 | `software_stop` | `stop=true/false` | 软件急停/解除 |
| 磁导航 | `magnetic_nav` | `aim_id`, `aim_dir`, `aim_action` | 磁导航控制 |

点位导航示例，需现场确认真实 AGV 是否接受：

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

停止点位导航示例：

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

软件急停示例：

```json
{
  "cmd_type": "software_stop",
  "stop": true
}
```

解除软件急停示例：

```json
{
  "cmd_type": "software_stop",
  "stop": false
}
```

## 5. 当前 RCS 控制下发缺口

目前已经实现的是 AGV -> RCS -> WCS 反馈链路。

当前仍缺：

```text
RCS mission -> AGV robot_control 控制命令
```

也就是说，如果现场要求 RCS 直接驱动车运动，RCS 需要补 MQTT 发布逻辑：

```text
WCS mission(from_point/to_point)
-> RCS 分配 AGV
-> RCS 根据点位映射生成 robot_control JSON
-> 发布到 robot_control
-> AGV 执行动作
-> AGV 通过 task_feedback/base_status 回传
-> RCS 回调 WCS
```

## 6. RCS -> AGV 控制下发建议设计

已实现一个可配置、默认关闭的 RCS MQTT command publisher，默认不影响现有车端逻辑。

建议配置项：

| 配置 | 默认值 | 说明 |
|---|---|---|
| `RCS_AGV_COMMAND_ENABLED` | `false` | 默认关闭，现场确认后开启 |
| `RCS_AGV_COMMAND_BROKER_URI` | `tcp://127.0.0.1:1883` | AGV/现场 broker 地址 |
| `RCS_AGV_COMMAND_TOPIC` | `robot_control` | AGV 控制 topic |
| `RCS_AGV_COMMAND_QOS` | `1` | MQTT QoS |
| `RCS_AGV_DEFAULT_RUN_SPEED` | `0.5` | 默认运行速度 |
| `RCS_AGV_POINT_ID_MAP` | 空 | openTCS/WCS 点位到 AGV 数字 id 映射 |

点位映射必须确认，因为 AGV 控制报文使用 `id`，而 WCS/openTCS 使用 `Point-01`、`P_WAIT_IN_01` 等字符串点位。

示例映射：

```json
{
  "Point-01": 1,
  "Point-02": 2,
  "P_WAIT_IN_01": 101,
  "ST_IN_01": 201
}
```

## 7. 实机测试顺序

### 7.1 只读接入

目标：不控制小车，只确认真实反馈。

检查项：

| 检查项 | 结果 |
|---|---|
| RCS/AGV adapter 能连接真实 MQTT broker | 待测 |
| 能收到 `base_status` | 待测 |
| 能解析电量 | 待测 |
| 能解析在线/离线 | 待测 |
| 能解析故障码 | 待测 |
| 能收到 `task_feedback` | 待测 |
| 能识别真实任务状态枚举 | 待测 |

### 7.2 单车空载点位任务

目标：小车空载移动，不接 PLC。

流程：

```text
WCS 创建 mission
RCS 创建 openTCS order
RCS 下发 robot_control
AGV 移动
AGV 上报 task_feedback
RCS 回调 WCS
```

通过标准：

| 检查项 | 通过标准 |
|---|---|
| AGV 接收控制命令 | 车端日志/反馈确认 |
| AGV 到达目标点 | task_feedback 或 base_status 确认 |
| RCS 回调 WCS | WCS 查询到事件 |
| mission 状态 | 到点为 `IN_PROGRESS`，完成为 `DONE` |

### 7.3 带货任务

目标：验证真实搬运业务。

检查项：

| 检查项 | 结果 |
|---|---|
| 托盘号与 mission 绑定 | 待测 |
| 到起点反馈 | 待测 |
| 取货反馈 | 待测 |
| 到终点反馈 | 待测 |
| 放货反馈 | 待测 |
| WCS 后续状态推进 | 待测 |

### 7.4 异常测试

必须测试：

| 异常 | 预期 |
|---|---|
| 低电量 | RCS/WCS 收到 `FAILED` 或标准低电量事件 |
| MQTT 断线 | RCS 能识别断线或重连 |
| 任务超时 | WCS/RCS 能进入失败处理 |
| 软件急停 | AGV 停止，RCS/WCS 记录异常 |
| 重复反馈 | 不重复回调 WCS |
| 重复任务 | 不重复创建/不重复执行 |

## 8. 下一步执行清单

| 顺序 | 动作 | 负责人 | 状态 |
|---|---|---|---|
| 1 | 拿真实 AGV MQTT broker 信息 | 现场/AGV | 待办 |
| 2 | 抓真实 `base_status/task_feedback/feedback` payload | 现场/AGV | 待办 |
| 3 | 确认 AGV 点位 id 与 WCS/openTCS 点位映射 | 现场/系统 | 待办 |
| 4 | 确认 `robot_control` 是否仍是控制 topic | 现场/AGV | 待办 |
| 5 | 确认 `interest_point_control` 是否可驱动车 | 现场/AGV | 待办 |
| 6 | 补 RCS -> AGV command publisher | RCS | 已完成，待实机确认 |
| 7 | 单车空载实机联调 | 全链路 | 待办 |
| 8 | 单车带货实机联调 | 全链路 | 待办 |
| 9 | PLC/堆垛机联动 | WCS/PLC | 待办 |

## 9. 当前判断

如果现场 AGV 只需要上报状态，不要求 RCS 直接控制，那么当前反馈链路已经足够进入只读联调。

如果现场 AGV 要由 RCS/openTCS 直接调度运动，当前已具备 RCS -> AGV `robot_control` 发布能力；实机开启前仍必须先确认点位字符串到 AGV 数字 id 的映射、控制 topic 和现场安全策略。

## 10. RCS -> AGV command publisher 当前实现状态（2026-06-02）

当前已经补上 RCS 向 AGV `robot_control` 发布控制命令的能力，但默认关闭，必须现场显式开启。

实际配置项：

| 配置 | 默认值 | 说明 |
|---|---|---|
| `RCS_AGV_COMMAND_ENABLED` | `false` | 是否启用 RCS 下发 AGV 控制命令 |
| `RCS_AGV_COMMAND_BROKER_URI` | `tcp://127.0.0.1:1883` | AGV MQTT broker 地址 |
| `RCS_AGV_COMMAND_CLIENT_ID` | `rcs-agv-command-publisher` | MQTT client id |
| `RCS_AGV_COMMAND_TOPIC` | `robot_control` | AGV 控制 topic |
| `RCS_AGV_COMMAND_QOS` | `1` | MQTT QoS |
| `RCS_AGV_COMMAND_USERNAME` | 空 | MQTT 用户名 |
| `RCS_AGV_COMMAND_PASSWORD` | 空 | MQTT 密码 |
| `RCS_AGV_DEFAULT_RUN_SPEED` | `0.5` | `interest_point_control` 默认速度 |
| `RCS_AGV_POINT_ID_MAP` | 空 | WCS/openTCS 点位字符串到 AGV 数字 id 的映射 |

已验证内容：

| 验证项 | 结果 | 边界 |
|---|---|---|
| RCS 创建 mission 后发布 `robot_control` | 通过 | 本地 `amqtt` broker，不是实车 broker |
| WCS 创建 mission 后经 RCS 发布 `robot_control` | 通过 | WCS 真进程，PLC 使用 mock server |
| payload 格式符合 catwin AGV 代码 `interest_point_control/start` | 通过 | 根据 `ros_mqtt.cpp` 代码确认 |
| 缺少点位映射时拒绝发布 | 通过 | 单元测试覆盖 |
| 实车接收并运动 | 未验证 | 需要现场 AGV |

本地验证捕获 payload：

```json
{"cmd":"start","id":2,"path_mode":0,"time":0,"cmd_type":"interest_point_control","path_stop_time":0,"circulates":1,"run_speed":0.5}
```

实机开启前必须确认：

| 必查项 | 原因 |
|---|---|
| `robot_control` 是否仍为现场控制 topic | 防止发错 topic |
| `interest_point_control/start` 是否允许直接驱动车 | 防止协议版本不一致 |
| `id` 与现场地图点位是否一致 | 防止车去错误点位 |
| 速度 `run_speed` 是否符合现场安全要求 | 防止速度过高 |
| MQTT broker 认证、ACL、QoS 要求 | 防止连接失败或权限不足 |
| 急停/复位现场处理流程 | 防止测试中无法安全停止 |

## 11. AGV command outbox 联调前检查（2026-06-02）

RCS 已具备 command outbox、失败重试和查询接口。实机联调时应先用查询接口确认命令状态，再判断是否是车端问题。

查询接口：

```http
GET /api/v1/wcs/agv/missions/{mission_no}/commands
```

状态判断：

| 状态 | 现场含义 | 排查方向 |
|---|---|---|
| `PENDING` | RCS 已生成命令，等待发布 | 看 scheduler 是否运行、tick 是否过长 |
| `SUCCESS` | RCS 已发布到 MQTT broker | 如果车没动，重点查车端订阅、topic、payload、点位 id |
| `FAILED` | RCS 发布 MQTT 失败 | 查 broker 地址、网络、认证、ACL、端口 |

实机第一轮建议：

1. 开启 `RCS_AGV_COMMAND_ENABLED=true`。
2. 配置真实 `RCS_AGV_COMMAND_BROKER_URI`。
3. 配置现场确认过的 `RCS_AGV_POINT_ID_MAP`。
4. 创建一个低风险空载 mission。
5. 查询 `/commands`，确认状态是否到 `SUCCESS`。
6. 如果 `SUCCESS` 但车不动，去车端查 `robot_control` 收包日志。
7. 如果 `FAILED`，先不要继续发任务，优先修 broker 连接或权限。
