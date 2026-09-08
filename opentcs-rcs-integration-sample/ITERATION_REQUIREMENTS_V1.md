# WMS-WCS-RCS-PLC 迭代需求与接口设计（V1）

- 文档版本：`v1.0.0`
- 创建日期：`2026-04-21`
- 适用范围：`openTCS RCS Integration Sample` 下一阶段真实平台联调开发
- 文档目标：先冻结需求和接口边界，再进入开发迭代

---

## 1. 背景与目标

当前 `demo-wcs` 已支持：

- WCS 下发 RCS 搬运任务
- RCS 回调 WCS（任务事件）
- 与 openTCS Web API 真实连通（LIVE 模式）

下一阶段目标是把链路扩展到：

`WMS <-> WCS <-> RCS <-> openTCS <-> PLC`

并形成可上线的接口与可靠性基线（幂等、重试、错误码、追踪、验收）。

---

## 2. 迭代范围（去重后）

用户提出任务去重后为 8 类：

1. `WCS -> RCS` 下发搬运任务
2. `RCS -> WCS` 搬运任务回调接口
3. `WCS` 入库任务接收接口（来自 WMS）
4. `WCS` 出库任务接收接口（来自 WMS）
5. `WCS -> PLC` 下发上架指令（堆垛机）
6. `WMS -> PLC` 下发取货指令（由 WCS 转发/编排）
7. `WCS -> WMS` 入库结果回传
8. `WCS -> WMS` 出库结果回传

---

## 3. 非目标（V1 不做）

1. 多仓多租户隔离
2. 复杂波次/批次调度优化
3. 设备级路径优化算法（由 RCS/openTCS 负责）
4. 跨地域容灾与双活

---

## 4. 统一业务实体

为避免跨系统字段混乱，V1 固定以下主键：

1. `biz_task_no`：WMS 业务任务号（入库/出库主键）
2. `mission_no`：WCS/RCS 搬运任务号（幂等键）
3. `task_no`：RCS 内任务号（可与 `biz_task_no` 一致）
4. `plc_job_no`：PLC 指令号
5. `trace_id` / `request_id`：全链路追踪号

---

## 5. 端到端主流程

### 5.1 入库

1. WMS 调 WCS 入库接口，提交 `biz_task_no`
2. WCS 校验并落库（状态 `RECEIVED`）
3. WCS 下发 RCS 搬运任务（生成 `mission_no`）
4. RCS 回调 WCS（`ARRIVED_FROM/PICKED/ARRIVED_TO/DROPPED/FAILED`）
5. `DROPPED` 后 WCS 下发 PLC 上架指令
6. PLC 执行结果回传 WCS
7. WCS 汇总后回传 WMS 入库结果（`DONE/FAILED`）

### 5.2 出库

1. WMS 调 WCS 出库接口，提交 `biz_task_no`
2. WCS 下发 PLC 取货指令（或由 WCS 编排转发）
3. PLC 取货完成后，WCS 下发 RCS 搬运任务
4. RCS 回调 WCS 任务进展
5. 完成后 WCS 回传 WMS 出库结果

---

## 6. 状态机（V1 冻结）

统一状态（WCS 视角）：

- `RECEIVED`
- `IN_PROGRESS`
- `WAIT_PLC`
- `DONE`
- `FAILED`
- `CANCELED`

状态迁移规则：

1. 新任务进入 `RECEIVED`
2. RCS 事件 `ARRIVED_FROM/PICKED/ARRIVED_TO` -> `IN_PROGRESS`
3. RCS 事件 `DROPPED`：
   - 入库：进入 `WAIT_PLC`
   - 出库：可直接进入 `DONE`（若无 PLC 后置动作）
4. PLC 成功 -> `DONE`
5. 任意失败 -> `FAILED`
6. 取消请求成功 -> `CANCELED`
7. `DONE/FAILED/CANCELED` 为终态，终态后仅允许幂等重放

---

## 7. 接口清单（V1 规划）

### 7.1 已有（保留）

1. `POST /api/v1/wcs/agv/missions`
2. `GET /api/v1/wcs/agv/missions/{mission_no}`
3. `POST /api/v1/wcs/agv/missions/{mission_no}/cancel`
4. `POST /api/v1/opentcs/events/transport-orders`（事件注入/桥接）

### 7.2 新增：WMS -> WCS

1. `POST /api/v1/wcs/inbound/tasks`（入库任务接收）
2. `POST /api/v1/wcs/outbound/tasks`（出库任务接收）
3. `GET /api/v1/wcs/tasks/{biz_task_no}`（任务全链路查询）
4. `POST /api/v1/wcs/tasks/{biz_task_no}/cancel`

### 7.3 新增：WCS -> PLC（由 WCS 主动调用）

1. `POST /api/v1/plc/putaway-jobs`（上架）
2. `POST /api/v1/plc/pick-jobs`（取货）

### 7.4 新增：PLC -> WCS 回调

1. `POST /api/v1/wcs/plc/jobs/{plc_job_no}/events`

### 7.5 新增：WCS -> WMS 回传

1. `POST /api/v1/wms/inbound-results`
2. `POST /api/v1/wms/outbound-results`

---

## 8. 幂等与一致性策略

1. WMS 入库/出库接口：`biz_task_no` 幂等
2. WCS -> RCS 下发：`mission_no` 幂等
3. RCS 回调：`mission_no + event_type + event_time` 幂等
4. PLC 回调：`plc_job_no + event_type + event_time` 幂等
5. WCS -> WMS 回传：`biz_task_no + result_type` 幂等
6. 幂等冲突返回统一错误码（见第 9 章）

---

## 9. 错误码体系（V1）

统一响应壳：

```json
{
  "code": "0",
  "msg": "OK",
  "data": {}
}
```

错误码建议：

1. `RCS-4001` 参数校验失败
2. `RCS-4004` 资源不存在
3. `RCS-4009` 幂等冲突
4. `RCS-4010` 鉴权失败
5. `RCS-4090` 状态机冲突（非法状态迁移）
6. `RCS-4290` 频控触发
7. `RCS-5002` 下游依赖失败（openTCS/PLC/WMS）
8. `RCS-5000` 内部错误

---

## 10. 可靠性与补偿

### 10.1 Outbox 机制（必须）

对以下异步调用统一启用 outbox + 重试：

1. RCS -> WCS 回调
2. WCS -> WMS 回传
3. WCS -> PLC 指令（若采用异步下发）

### 10.2 重试策略

1. `60s -> 120s -> 300s -> 600s -> 1800s`
2. 最大重试次数可配置，超过后进入 `DEAD_LETTER`

### 10.3 人工补偿接口（V1 建议）

1. `POST /api/v1/wcs/replay/{biz_task_no}`：补发回调/结果
2. `POST /api/v1/wcs/tasks/{biz_task_no}/force-fail`
3. `POST /api/v1/wcs/tasks/{biz_task_no}/force-done`（需审计）

---

## 11. 安全与审计

1. 所有外部入口支持 `X-Api-Access-Key`/Bearer 二选一
2. 回调接口支持 `timestamp + payload` HMAC 签名
3. 所有变更型接口必须记录审计日志：
   - 操作人/来源系统
   - `trace_id/request_id`
   - 请求体摘要

---

## 12. 可观测性

### 12.1 日志

强制输出：

1. `trace_id`
2. `request_id`
3. `biz_task_no`
4. `mission_no`
5. `plc_job_no`

### 12.2 指标

1. 任务总量/成功率/失败率
2. 回调重试次数与失败积压数
3. 从 WMS 接单到 DONE 的端到端耗时（P50/P95/P99）
4. 下游依赖错误分布（openTCS/PLC/WMS）

---

## 13. 验收标准（真实平台）

必须在真实 openTCS + PLC（或 PLC mock）环境验收通过：

1. 入库主流程成功（含 PLC 上架成功回传）
2. 出库主流程成功（含 PLC 取货环节）
3. 无效点位/无效任务返回可诊断错误码
4. 回调丢包可自动重试并最终一致
5. 幂等重复请求不产生重复业务动作
6. 关键链路具备可追踪性（日志可串联）

---

## 14. 迭代计划建议

### Sprint 1（P0）

1. WMS 入/出库接入接口
2. 状态机 + 幂等落库
3. 错误码统一
4. WCS -> WMS 回传接口（含 outbox）

### Sprint 2（P0/P1）

1. PLC 指令接口与回调接口
2. 补偿与重放接口
3. 安全（access key/hmac）与审计

### Sprint 3（P1）

1. 仪表盘指标
2. 压测与稳定性优化
3. 文档冻结 `v1.1`

---

## 15. 当前实现差距（截至 2026-04-21）

已完成：

1. `demo-wcs` 与 openTCS 真实联通
2. 任务下发、查询、取消
3. RCS 事件回调与任务状态推进
4. demo 侧点位建议与点位校验辅助
5. openTCS 调用失败错误码透传（`RCS-5002`）

待完成（对齐本设计）：

1. WMS 入库/出库接入接口
2. PLC 指令与回调完整闭环
3. WCS -> WMS 结果回传
4. 人工补偿与任务纠偏接口
5. 全量 E2E 自动化回归（含异常场景）

