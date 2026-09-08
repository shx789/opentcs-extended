# 4.7 WCS-RCS 集成规范（HTTP）

> 冻结联调版本：`WCS_RCS_HTTP_API_FREEZE_V1.md`

## 4.7.1 集成说明
RCS 为基于 openTCS 二次开发的机器人调度系统，负责接收 WCS 下发的搬运任务，完成 AGV 任务调度、车辆状态管理、任务反馈转换及 MQTT 协议适配。  
WCS 不直接与 AGV 通信，而是通过 HTTP API 向 RCS 下发搬运任务，再由 RCS 控制 AGV。

- 通信协议：HTTP/HTTPS
- 报文格式：`application/json; charset=utf-8`
- 字段命名：建议 `snake_case`
- 时间格式：`yyyy-MM-dd HH:mm:ss`（如：`2026-02-10 10:35:21`）
- 字符编码：UTF-8

---

## 4.7.2 接口总览（你问的“是不是只有这几个”）

### A. 当前代码已实现

| 编号 | 方向 | Method | URL | 说明 |
|---|---|---|---|---|
| I-09 | WCS -> RCS | POST | `/api/v1/wcs/agv/missions` | 创建搬运任务 |
| I-10 | RCS -> WCS | POST | 由 `callback_url` 指定 | AGV 事件回传 |

### B. 生产建议补齐（建议在联调前明确）

| 编号 | 方向 | Method | URL | 说明 |
|---|---|---|---|---|
| I-09-CANCEL | WCS -> RCS | POST | `/api/v1/wcs/agv/missions/{mission_no}/cancel` | 取消任务 |
| I-09-QUERY | WCS -> RCS | GET | `/api/v1/wcs/agv/missions/{mission_no}` | 查询任务状态 |
| I-10-BATCH | RCS -> WCS | POST | `/api/v1/wcs/agv/events/batch` | 批量事件回传（可选） |
| I-10-ACK | RCS -> WCS | - | 同 I-10 响应体约定 | 明确回调确认字段与幂等规则 |

说明：  
从“RCS 主动请求 WCS”这个角度，当前实现里核心就是 I-10 回调。  
你感觉“少”，通常是因为生产对接还需要补充取消、查询、批量和统一 ACK 契约。

---

## 4.7.3 I-09 创建搬运任务（WCS -> RCS）

### 接口定义
- Method：`POST`
- URL：`/api/v1/wcs/agv/missions`
- 用途：派发物理搬运指令  
  入库场景：起始区 -> 入库站台  
  出库场景：出库站台 -> 终点

### 请求字段

| 字段 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| mission_no | string | 是 | M202602100001 | AGV 搬运任务号，RCS 幂等键 |
| task_no | string | 是 | T202602090001 | 对应业务任务号 |
| from_point | string | 是 | P_WAIT_IN_01 | 搬运起点 |
| to_point | string | 是 | ST_IN_01 | 搬运终点 |
| pallet_no | string | 是 | PLT000000123 | 托盘号 |
| priority | int | 否 | 50 | 优先级（值越大优先级越高） |
| callback_url | string | 是 | /api/v1/wcs/agv/events | AGV 事件回调地址（支持相对/绝对 URL） |

### 响应字段（data）

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| mission_no | string | M202602100001 | AGV 任务号 |
| task_no | string | T202602090001 | 业务任务号 |
| rcs_status | string | RECEIVED | RCS 当前状态 |
| idem_hit | bool | false | 是否命中幂等 |

---

## 4.7.4 I-10 AGV 事件回传（RCS -> WCS）

### 接口定义
- Method：`POST`
- URL：由 I-09 的 `callback_url` 指定（推荐：`/api/v1/wcs/agv/events`）
- 用途：RCS 向 WCS 回传任务状态与过程事件

### 请求头（建议）
- `Content-Type: application/json`
- `Accept: application/json`
- `Authorization: Bearer <token>`（可选）
- `X-Callback-Timestamp: <epoch_millis>`（启用签名时）
- `X-Callback-Signature: <hmac_sha256_hex>`（启用签名时）

签名内容建议：`timestamp + "." + payload_json`

### 请求字段

| 字段 | 类型 | 必填 | 示例 | 说明 |
|---|---|---|---|---|
| mission_no | string | 是 | M202602100001 | AGV 搬运任务号 |
| task_no | string | 是 | T202602090001 | 业务任务号 |
| event_type | string | 是 | ARRIVED_FROM | 事件类型 |
| point_id | string | 否 | P_WAIT_IN_01 | 事件发生点位 |
| agv_id | string | 否 | AGV_01 | AGV 编号 |
| battery | int | 否 | 78 | 电量百分比 |
| reason_code | string | 否 | LOW_BATTERY | 失败原因码 |
| reason_msg | string | 否 | battery below threshold | 失败原因说明 |
| event_time | string | 是 | 2026-02-10 10:35:21 | 事件时间 |

### `event_type` 枚举建议

| event_type | 含义 |
|---|---|
| ARRIVED_FROM | 到达起点 |
| PICKED | 已取货 |
| ARRIVED_TO | 到达终点 |
| DROPPED | 已放货 |
| FAILED | 任务失败 |

### 响应字段（data）

| 字段 | 类型 | 示例 | 说明 |
|---|---|---|---|
| mission_no | string | M202602100001 | AGV 任务号 |
| task_no | string | T202602090001 | 业务任务号 |
| wcs_status | string | ACCEPTED | WCS 处理状态 |
| idem_hit | bool | false | 是否命中幂等 |

### WCS 处理规则
- `DROPPED`：
  - 入库任务：推进至 PLC 上架阶段
  - 出库任务：推进至最终结果回传阶段
- `FAILED`：
  - WCS 将 RCS 原始原因映射为标准错误码，并同步给 WMS

---

## 4.7.5 联调前必须确认项（比“接口数量”更关键）

1. 鉴权方式：Bearer Token、来源、更新周期  
2. 签名验签：算法、参与签名字段、时钟偏差容忍  
3. 幂等键：I-09 用 `mission_no`；I-10 用 `mission_no + event_type + event_time`  
4. 状态码约定：`2xx` 才算回调成功；`4xx/5xx` 触发重试策略  
5. 超时与重试：超时阈值、最大重试次数、退避策略  
6. 事件乱序处理：WCS 是否按状态机过滤逆序事件  
7. 错误码映射：`reason_code` 到 WCS/WMS 标准错误码  
8. 时间基准：统一时区与格式（建议 UTC 或明确本地时区）  
9. 数据字典：点位、AGV 编号、任务类型、优先级范围  
10. 日志追踪：`trace_id`/`request_id` 透传规则

---

## 4.7.6 实现注意事项（和当前样例代码相关）

- 当前样例 Java DTO 字段是 `camelCase`（如 `missionNo`）。  
- 若你要严格按 `snake_case` 与 WCS 联调，需要统一 JSON 命名策略（如 `@JsonProperty` 或全局命名配置），避免字段不匹配。  
