# WMS -> WCS Task API（Sprint 1）

本文档对应 `openTCS RCS Integration Sample` 已落地的 Sprint 1 接口能力。

## 1. 已实现接口

1. `POST /api/v1/wcs/inbound/tasks`
2. `POST /api/v1/wcs/outbound/tasks`
3. `GET /api/v1/wcs/tasks/{biz_task_no}`
4. `POST /api/v1/wcs/tasks/{biz_task_no}/cancel`

说明：

1. `biz_task_no` 为幂等键（按接口类型区分入库/出库）。
2. 创建任务时会触发已有 `WCS -> RCS(openTCS)` 搬运任务下发。
3. 入库任务在收到 `FINISHED` 搬运事件后，查询状态会进入 `WAIT_PLC`（预留 PLC 闭环阶段）。

## 2. 请求/响应

### 2.1 创建入库任务

`POST /api/v1/wcs/inbound/tasks`

```json
{
  "biz_task_no": "BIZ-IN-202604210001",
  "mission_no": "M202604210001",
  "task_no": "T202604210001",
  "from_point": "Point-0020",
  "to_point": "Point-0026",
  "pallet_no": "PLT000000123",
  "priority": 80,
  "callback_url": "http://127.0.0.1:8090/demo/wcs/callback"
}
```

成功响应：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202604210001",
    "task_type": "INBOUND",
    "mission_no": "M202604210001",
    "task_no": "T202604210001",
    "plc_job_no": null,
    "rcs_status": "RECEIVED",
    "trace_id": "trace-xxx",
    "request_id": "req-xxx",
    "idem_hit": false
  }
}
```

### 2.2 创建出库任务

`POST /api/v1/wcs/outbound/tasks`

请求体与入库一致，`task_type` 为 `OUTBOUND`。

### 2.3 任务查询

`GET /api/v1/wcs/tasks/{biz_task_no}`

成功响应（示例）：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202604210001",
    "task_type": "INBOUND",
    "mission_no": "M202604210001",
    "task_no": "T202604210001",
    "plc_job_no": null,
    "rcs_status": "WAIT_PLC",
    "trace_id": "trace-xxx",
    "request_id": "req-xxx",
    "created_at": "2026-04-21T14:20:31.201Z",
    "updated_at": "2026-04-21T14:21:10.901Z"
  }
}
```

### 2.4 任务取消

`POST /api/v1/wcs/tasks/{biz_task_no}/cancel`

成功响应（示例）：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {
    "biz_task_no": "BIZ-IN-202604210001",
    "task_type": "INBOUND",
    "mission_no": "M202604210001",
    "task_no": "T202604210001",
    "plc_job_no": null,
    "rcs_status": "CANCELED",
    "trace_id": "trace-xxx",
    "request_id": "req-xxx",
    "idem_hit": false
  }
}
```

## 3. 错误码

1. `RCS-4001` 参数校验失败
2. `RCS-4004` 资源不存在（如 `biz_task_no` 不存在）
3. `RCS-4009` 幂等冲突（同幂等键但请求体不一致）
4. `RCS-4090` 状态机冲突（非法状态迁移/终态取消）
5. `RCS-5002` 下游依赖失败（openTCS 等）
6. `RCS-5000` 内部错误

## 4. 真实联调结果（2026-04-21）

基于本次代码版本，在 LIVE 模式验证结果：

1. `demo/wcs` 页面显示 `LIVE openTCS HTTP mode`
2. `POST /api/v1/wcs/inbound/tasks` 返回 `code=0`
3. openTCS `GET /v1/transportOrders` 数量从 `8` 增长到 `9`
4. 注入 `FINISHED` 事件后，`GET /api/v1/wcs/tasks/{biz_task_no}` 返回 `WAIT_PLC`
