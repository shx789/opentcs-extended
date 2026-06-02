# WCS ⇄ RCS 集成接口规范（HTTP）

## 1. 接口清单

| 编号 | 方向 | 方法 | URL | 说明 |
|---|---|---|---|---|
| I-09 | WCS -> RCS | POST | `/api/v1/wcs/agv/missions` | 创建 AGV 搬运任务 |
| I-09-Q | WCS -> RCS | GET | `/api/v1/wcs/agv/missions/{mission_no}` | 查询 AGV 搬运任务 |
| I-09-C | WCS -> RCS | POST | `/api/v1/wcs/agv/missions/{mission_no}/cancel` | 取消 AGV 搬运任务 |
| I-10 | RCS -> WCS | POST | 由 `callback_url` 指定，建议 `/api/wcs/agv/events` | AGV 任务事件回传 |
| T-01 | WCS -> RCS | POST | `/api/v1/wcs/inbound/tasks` | 创建入库业务任务 |
| T-02 | WCS -> RCS | POST | `/api/v1/wcs/outbound/tasks` | 创建出库业务任务 |
| T-03 | WCS -> RCS | GET | `/api/v1/wcs/tasks/{biz_task_no}` | 查询业务任务 |
| T-04 | WCS -> RCS | POST | `/api/v1/wcs/tasks/{biz_task_no}/cancel` | 取消业务任务 |

说明：

- WCS 不直接对接 AGV，小车控制和状态适配由 RCS 负责。
- WCS 通过 I-09 向 RCS 下发搬运任务。
- RCS 根据 I-09 中的 `callback_url` 将 AGV 任务事件通过 I-10 回传给 WCS。
- `mission_no` 是 WCS 与 RCS 之间搬运任务的主幂等键。

## 2. 通用规范

### 2.1 请求头

```http
Content-Type: application/json
Accept: application/json
```

### 2.2 链路追踪头

| Header | 必填 | 说明 |
|---|---|---|
| `X-Trace-Id` | 否 | 全链路追踪 ID；调用方不传时 RCS 可自动生成 |
| `X-Request-Id` | 否 | 单次请求 ID；调用方不传时 RCS 可自动生成 |

### 2.3 统一响应结构

成功响应：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {}
}
```

失败响应：

```json
{
  "code": "RCS-4001",
  "msg": "missionNo must not be blank",
  "data": null
}
```

### 2.4 错误码

| HTTP状态码 | code | 说明 |
|---|---|---|
| 400 | `RCS-4001` | 参数错误、必填字段为空、字段格式不合法 |
| 404 | `RCS-4004` | 资源不存在，如 `mission_no` 不存在 |
| 409 | `RCS-4009` | 幂等冲突：同一 `mission_no` 重复提交但请求体不一致 |
| 409 | `RCS-4090` | 状态冲突，如终态任务再次取消 |
| 502 | `RCS-5002` | RCS 调用 openTCS 或下游服务失败 |
| 500 | `RCS-5000` | RCS 内部异常 |

## 3. I-09 创建搬运任务（WCS -> RCS）

### 3.1 请求

```http
POST /api/v1/wcs/agv/missions
```

### 3.2 用途

WCS 向 RCS 下发 AGV 物理搬运任务。RCS 接收后生成或关联 openTCS transport order，并负责后续 AGV 调度与任务状态回传。

### 3.3 请求字段

| 字段 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `mission_no` | string | 是 | `M202602100001` | AGV 搬运任务号；RCS 幂等键 |
| `task_no` | string | 是 | `T202602090001` | WCS/WMS 业务任务号 |
| `from_point` | string | 是 | `P_WAIT_IN_01` | 搬运起点 |
| `to_point` | string | 是 | `ST_IN_01` | 搬运终点 |
| `pallet_no` | string | 是 | `PLT000000123` | 托盘号 |
| `priority` | int | 否 | `50` | 优先级；不传时默认 `50` |
| `callback_url` | string | 是 | `/api/wcs/agv/events` | RCS 回调 WCS 的 AGV 事件地址；支持相对或绝对 URL |

### 3.4 请求示例

```json
{
  "mission_no": "M202602100001",
  "task_no": "T202602090001",
  "from_point": "P_WAIT_IN_01",
  "to_point": "ST_IN_01",
  "pallet_no": "PLT000000123",
  "priority": 50,
  "callback_url": "/api/wcs/agv/events"
}
```

### 3.5 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602090001` | 业务任务号 |
| `rcs_status` | string | `RECEIVED` | RCS 当前任务状态 |
| `idem_hit` | bool | `false` | 是否命中幂等 |

### 3.6 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "mission_no": "M202602100001",
    "task_no": "T202602090001",
    "rcs_status": "RECEIVED",
    "idem_hit": false
  }
}
```

### 3.7 幂等规则

| 场景 | 处理 |
|---|---|
| 首次提交 `mission_no` | 创建任务，返回 `idem_hit=false` |
| 重复提交相同 `mission_no` 且请求体一致 | 不重复创建任务，返回缓存结果，`idem_hit=true` |
| 重复提交相同 `mission_no` 但请求体不一致 | 返回 `409 / RCS-4009` |

## 4. I-09-Q 查询搬运任务（WCS -> RCS）

### 4.1 请求

```http
GET /api/v1/wcs/agv/missions/{mission_no}
```

### 4.2 路径参数

| 参数 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `mission_no` | string | 是 | `M202602100001` | AGV 搬运任务号 |

### 4.3 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602090001` | 业务任务号 |
| `rcs_status` | string | `RECEIVED` | RCS 当前任务状态 |

### 4.4 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "mission_no": "M202602100001",
    "task_no": "T202602090001",
    "rcs_status": "RECEIVED"
  }
}
```

## 5. I-09-C 取消搬运任务（WCS -> RCS）

### 5.1 请求

```http
POST /api/v1/wcs/agv/missions/{mission_no}/cancel
```

### 5.2 路径参数

| 参数 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `mission_no` | string | 是 | `M202602100001` | AGV 搬运任务号 |

### 5.3 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602090001` | 业务任务号 |
| `rcs_status` | string | `CANCELED` | RCS 当前任务状态 |
| `idem_hit` | bool | `false` | 是否命中幂等 |

### 5.4 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "mission_no": "M202602100001",
    "task_no": "T202602090001",
    "rcs_status": "CANCELED",
    "idem_hit": false
  }
}
```

### 5.5 处理规则

| 当前状态 | 处理 |
|---|---|
| 非终态 | 取消任务并返回 `CANCELED` |
| 已 `CANCELED` | 不重复取消，返回 `idem_hit=true` |
| 已终态 | 返回 `409 / RCS-4090` |

## 6. T-01/T-02 创建业务任务（WCS -> RCS）

### 6.1 接口说明

除 mission 维度接口外，RCS 当前还提供业务任务维度接口。该类接口适用于 WCS 直接以 `biz_task_no` 维度向 RCS 下发入库/出库任务，由 RCS 内部生成或关联 `mission_no/task_no`。

| 编号 | 方法 | URL | 说明 |
|---|---|---|---|
| T-01 | POST | `/api/v1/wcs/inbound/tasks` | 创建入库业务任务 |
| T-02 | POST | `/api/v1/wcs/outbound/tasks` | 创建出库业务任务 |

说明：

- `biz_task_no` 是业务任务幂等键。
- `mission_no`、`task_no` 可由调用方传入；如果不传，RCS 可根据 `biz_task_no` 生成默认值。
- 当前 WCS backend 已实现 mission 代理接口；这些业务任务接口目前在 RCS 端存在，WCS backend 尚未提供对应代理接口。

### 6.2 请求字段

| 字段 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `biz_task_no` | string | 是 | `BIZ-IN-202602100001` | 业务任务号；任务级幂等键 |
| `mission_no` | string | 否 | `M202602100001` | AGV 搬运任务号；不传时 RCS 可生成 |
| `task_no` | string | 否 | `T202602100001` | RCS/WCS 任务号；不传时 RCS 可生成 |
| `from_point` | string | 是 | `P_WAIT_IN_01` | 搬运起点 |
| `to_point` | string | 是 | `ST_IN_01` | 搬运终点 |
| `pallet_no` | string | 是 | `PLT000000123` | 托盘号 |
| `priority` | int | 否 | `50` | 优先级；不传时默认 `50` |
| `callback_url` | string | 是 | `/api/wcs/agv/events` | RCS 回调 WCS 的 AGV 事件地址 |

### 6.3 请求示例

```json
{
  "biz_task_no": "BIZ-IN-202602100001",
  "mission_no": "M202602100001",
  "task_no": "T202602100001",
  "from_point": "P_WAIT_IN_01",
  "to_point": "ST_IN_01",
  "pallet_no": "PLT000000123",
  "priority": 50,
  "callback_url": "/api/wcs/agv/events"
}
```

### 6.4 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `biz_task_no` | string | `BIZ-IN-202602100001` | 业务任务号 |
| `task_type` | string | `INBOUND` | 任务类型：`INBOUND` / `OUTBOUND` |
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602100001` | RCS/WCS 任务号 |
| `plc_job_no` | string | `PLC202602100001` | PLC 作业号；没有时可为空 |
| `rcs_status` | string | `RECEIVED` | RCS 当前任务状态 |
| `trace_id` | string | `TRACE-202602100001` | 全链路追踪 ID |
| `request_id` | string | `REQ-202602100001` | 请求 ID |
| `idem_hit` | bool | `false` | 是否命中幂等 |

### 6.5 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202602100001",
    "task_type": "INBOUND",
    "mission_no": "M202602100001",
    "task_no": "T202602100001",
    "plc_job_no": null,
    "rcs_status": "RECEIVED",
    "trace_id": "TRACE-202602100001",
    "request_id": "REQ-202602100001",
    "idem_hit": false
  }
}
```

### 6.6 幂等规则

| 场景 | 处理 |
|---|---|
| 首次提交 `biz_task_no` | 创建业务任务和搬运任务，返回 `idem_hit=false` |
| 重复提交相同 `biz_task_no` 且请求体一致 | 不重复创建任务，返回缓存结果，`idem_hit=true` |
| 重复提交相同 `biz_task_no` 但请求体不一致 | 返回 `409 / RCS-4009` |

## 7. T-03 查询业务任务（WCS -> RCS）

### 7.1 请求

```http
GET /api/v1/wcs/tasks/{biz_task_no}
```

### 7.2 路径参数

| 参数 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `biz_task_no` | string | 是 | `BIZ-IN-202602100001` | 业务任务号 |

### 7.3 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `biz_task_no` | string | `BIZ-IN-202602100001` | 业务任务号 |
| `task_type` | string | `INBOUND` | 任务类型 |
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602100001` | RCS/WCS 任务号 |
| `plc_job_no` | string | `PLC202602100001` | PLC 作业号；没有时可为空 |
| `rcs_status` | string | `RECEIVED` | RCS 当前任务状态 |
| `trace_id` | string | `TRACE-202602100001` | 全链路追踪 ID |
| `request_id` | string | `REQ-202602100001` | 请求 ID |
| `created_at` | string | `2026-02-10T10:00:00Z` | 创建时间 |
| `updated_at` | string | `2026-02-10T10:01:00Z` | 更新时间 |

### 7.4 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202602100001",
    "task_type": "INBOUND",
    "mission_no": "M202602100001",
    "task_no": "T202602100001",
    "plc_job_no": null,
    "rcs_status": "RECEIVED",
    "trace_id": "TRACE-202602100001",
    "request_id": "REQ-202602100001",
    "created_at": "2026-02-10T10:00:00Z",
    "updated_at": "2026-02-10T10:01:00Z"
  }
}
```

## 8. T-04 取消业务任务（WCS -> RCS）

### 8.1 请求

```http
POST /api/v1/wcs/tasks/{biz_task_no}/cancel
```

### 8.2 路径参数

| 参数 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `biz_task_no` | string | 是 | `BIZ-IN-202602100001` | 业务任务号 |

### 8.3 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `biz_task_no` | string | `BIZ-IN-202602100001` | 业务任务号 |
| `task_type` | string | `INBOUND` | 任务类型 |
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602100001` | RCS/WCS 任务号 |
| `plc_job_no` | string | `PLC202602100001` | PLC 作业号；没有时可为空 |
| `rcs_status` | string | `CANCELED` | RCS 当前任务状态 |
| `trace_id` | string | `TRACE-202602100001` | 全链路追踪 ID |
| `request_id` | string | `REQ-202602100001` | 请求 ID |
| `idem_hit` | bool | `false` | 是否命中幂等 |

### 8.4 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202602100001",
    "task_type": "INBOUND",
    "mission_no": "M202602100001",
    "task_no": "T202602100001",
    "plc_job_no": null,
    "rcs_status": "CANCELED",
    "trace_id": "TRACE-202602100001",
    "request_id": "REQ-202602100001",
    "idem_hit": false
  }
}
```

### 8.5 处理规则

| 当前状态 | 处理 |
|---|---|
| 非终态 | 取消关联搬运任务，并更新业务任务为 `CANCELED` |
| 已 `CANCELED` | 不重复取消，返回 `idem_hit=true` |
| 已终态 | 返回 `409 / RCS-4090` |

## 9. I-10 AGV 任务事件回传（RCS -> WCS）

### 9.1 请求

```http
POST {callback_url}
```

推荐 WCS 回调地址：

```http
POST /api/wcs/agv/events
```

### 9.2 用途

RCS 将 openTCS 任务事件或 AGV MQTT 反馈转换为统一的 AGV 任务事件，并回传给 WCS。WCS 据此推进执行任务状态、触发后续 PLC 阶段或向 WMS 回传最终结果。

### 9.3 请求字段

| 字段 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| `mission_no` | string | 是 | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | 是 | `T202602090001` | 业务任务号 |
| `event_type` | string | 是 | `ARRIVED_FROM` | 事件类型 |
| `point_id` | string | 否 | `P_WAIT_IN_01` | 事件发生点位 |
| `agv_id` | string | 否 | `AGV_01` | AGV 编号 |
| `battery` | int | 否 | `78` | 电量百分比 |
| `reason_code` | string | 否 | `LOW_BATTERY` | 失败原因码 |
| `reason_msg` | string | 否 | `battery below threshold` | 失败原因说明 |
| `trace_id` | string | 否 | `TRACE-202602100001` | 全链路追踪 ID |
| `request_id` | string | 否 | `REQ-202602100001` | 请求 ID |
| `event_time` | string | 是 | `2026-02-10 10:35:21` | 事件时间 |

### 9.4 event_type 枚举

| event_type | 含义 | 说明 |
|---|---|---|
| `ARRIVED_FROM` | 到达起点 | AGV 到达搬运起点 |
| `PICKED` | 已取货 | AGV 已完成取货动作 |
| `ARRIVED_TO` | 到达终点 | AGV 到达搬运终点 |
| `DROPPED` | 已放货 | AGV 已完成放货动作 |
| `FAILED` | 任务失败 | AGV 或 RCS 执行失败 |

兼容事件：

| 输入事件 | 标准化结果 |
|---|---|
| `COMPLETED` / `COMPLETE` / `FINISHED` | `DROPPED` |
| `ERROR` / `EXCEPTION` / `FAULT` | `FAILED` |
| `ARRIVED` | 根据已知 mission/task 的 `from_point`、`to_point` 与 `point_id` 推导为 `ARRIVED_FROM` 或 `ARRIVED_TO` |

RCS 对 AGV MQTT 回传使用 `message_id` 作为优先幂等键；没有 `message_id` 时使用 `agv_id + mission_no + event_type + event_time + seq` 组合键。相同幂等键已成功回调后，重复消息不会再次回调 WCS。

### 9.5 请求示例

```json
{
  "mission_no": "M202602100001",
  "task_no": "T202602090001",
  "event_type": "DROPPED",
  "point_id": "ST_IN_01",
  "agv_id": "AGV_01",
  "battery": 78,
  "reason_code": null,
  "reason_msg": null,
  "trace_id": "TRACE-202602100001",
  "request_id": "REQ-202602100001",
  "event_time": "2026-02-10 10:35:21"
}
```

### 9.6 成功响应字段

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `mission_no` | string | `M202602100001` | AGV 搬运任务号 |
| `task_no` | string | `T202602090001` | 业务任务号 |
| `wcs_status` | string | `ACCEPTED` | WCS 处理状态 |
| `idem_hit` | bool | `false` | 是否命中幂等 |

### 9.7 成功响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "mission_no": "M202602100001",
    "task_no": "T202602090001",
    "wcs_status": "ACCEPTED",
    "idem_hit": false
  }
}
```

### 9.8 WCS 处理规则

| event_type | WCS 处理建议 |
|---|---|
| `ARRIVED_FROM` | 记录 AGV 到达起点，可推进搬运任务为执行中 |
| `PICKED` | 记录取货完成，可校验托盘/物料信息 |
| `ARRIVED_TO` | 记录 AGV 到达终点 |
| `DROPPED` | 入库任务推进至 PLC 上架阶段；出库任务推进至最终结果回传阶段 |
| `FAILED` | 映射标准错误码，记录失败阶段，并同步给 WMS |

## 10. 状态枚举

### 10.1 WCS 任务状态

| 状态 | 含义 | 是否终态 |
|---|---|---|
| `RECEIVED` | 已接收任务 | 否 |
| `DISPATCHING` | 正在下发 RCS / PLC / 设备任务 | 否 |
| `EXECUTING` | 执行中 | 否 |
| `SUCCESS` | 执行成功 | 是 |
| `FAILED` | 执行失败 | 是 |

### 10.2 RCS 搬运任务状态

| 状态 | 含义 | 是否终态 |
|---|---|---|
| `RECEIVED` | 已接收任务 | 否 |
| `DISPATCHED` | 已分配 AGV / 已下发控制命令 | 否 |
| `EXECUTING` | 执行中 | 否 |
| `SUCCESS` | 执行成功 | 是 |
| `FAILED` | 执行失败 | 是 |
| `CANCELED` | 已取消 | 是 |

### 10.3 实现兼容说明

如果 RCS 内部使用 `DONE` 表示完成状态，对外接口建议统一映射为 `SUCCESS`；如果保留 `DONE`，需在双方接口联调前明确状态枚举，以免 WCS 状态机无法识别终态。

## 11. I-11 查询 AGV 控制命令下发状态（WCS -> RCS）

### 11.1 接口说明

该接口用于查询 RCS 对某个 mission 生成的 AGV MQTT 控制命令 outbox 状态，便于现场排查 `robot_control` 是否已发布、是否失败、是否等待重试。

### 11.2 请求

```http
GET /api/v1/wcs/agv/missions/{mission_no}/commands
```

路径参数：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `mission_no` | string | 是 | AGV 搬运任务号 |

### 11.3 响应字段（data[]）

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| `missionNo` | string | `M202602100001` | AGV 搬运任务号 |
| `taskNo` | string | `T202602090001` | WCS 业务任务号 |
| `commandStage` | string | `MISSION_START` | 控制命令阶段 |
| `idemKey` | string | `M202602100001:MISSION_START` | 命令幂等键 |
| `payloadJson` | string | `{...}` | 实际发布到 `robot_control` 的 JSON 字符串 |
| `status` | string | `PENDING`/`SUCCESS`/`FAILED` | 下发状态 |
| `retryCount` | int | `1` | 已重试次数 |
| `nextRetryAt` | number/null | `1780381116.604255349` | 下次重试时间戳；成功后为空 |
| `lastError` | string/null | `Could not publish...` | 最近一次失败原因 |

### 11.4 响应示例

```json
{
  "code": "0",
  "msg": "OK",
  "data": [
    {
      "missionNo": "M-CMD-OUTBOX-001",
      "taskNo": "T-CMD-OUTBOX-001",
      "commandStage": "MISSION_START",
      "idemKey": "M-CMD-OUTBOX-001:MISSION_START",
      "payloadJson": "{\"cmd\":\"start\",\"id\":2,\"cmd_type\":\"interest_point_control\"}",
      "status": "SUCCESS",
      "retryCount": 0,
      "nextRetryAt": null,
      "lastError": null
    }
  ]
}
```

### 11.5 状态说明

| 状态 | 含义 |
|---|---|
| `PENDING` | 已生成控制命令，等待后台 scheduler 发布 |
| `SUCCESS` | 已成功发布到 MQTT `robot_control` |
| `FAILED` | 发布失败，等待 `nextRetryAt` 到期后继续重试 |

### 11.6 当前实现说明

RCS 创建 mission 时不再同步阻塞 MQTT 发布。流程为：

```text
WCS 创建 mission
RCS 创建 openTCS transport order
RCS 保存 mission
RCS 写入 AGV command outbox
后台 scheduler 发布 robot_control
发布成功则 command 状态变为 SUCCESS
发布失败则 command 状态变为 FAILED 并按退避策略重试
```
