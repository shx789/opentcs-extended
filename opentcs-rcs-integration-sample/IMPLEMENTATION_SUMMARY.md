# openTCS RCS 集成样例实现说明

## 1. 实现目标
基于你给出的集成规范（WCS-RCS HTTP、RCS-AGV MQTT）先落地一版 **RCS 样例骨架**，重点打通：

- I-09 创建搬运任务（WCS -> RCS）
- I-10 事件回传链路（openTCS/SSE -> RCS -> WCS Callback）
- 幂等、回调重试、事件映射等关键机制

> 说明：当前为可运行样例（sample），用于验证架构和流程；尚未全部替换为生产级组件。

---

## 2. 已完成内容

### 2.1 模块与工程接入
- 新增子模块：`opentcs-rcs-integration-sample`
- 已加入 Gradle 多模块构建：`settings.gradle`
- 新增运行任务：`runRcsSample`

### 2.2 I-09 创建任务（WCS -> RCS）
- 新增请求/响应 DTO：
  - `CreateMissionReq`
  - `CreateMissionResp`
- 新增任务创建服务：`WcsMissionService`
  - 参数校验
  - 幂等校验（`MISSION_CREATE + missionNo`）
  - 映射并调用 openTCS 建单端口
  - 注册 `missionNo -> callbackUrl` 供后续事件回传使用
- 新增 HTTP Handler：`WcsMissionHttpHandlers`
  - 路由：`POST /api/v1/wcs/agv/missions`

### 2.3 openTCS 下发映射
- 新增映射器：`OpenTcsPayloadMapper`
  - 业务任务 -> openTCS `TransportOrder` payload
  - `fromPoint/toPoint` -> destinations
  - mission/task/pallet/callback/priority -> properties
  - `priority -> deadline`（优先级越高，deadline 越早）
- 新增 in-memory openTCS 客户端端口实现：
  - `OpenTcsOrderClient`
  - `InMemoryOpenTcsOrderClient`

### 2.4 I-10 事件回传链路
- 新增 openTCS 事件模型：`OpenTcsTransportOrderEvent`
- 新增原始 SSE JSON 解析器：`OpenTcsSsePayloadParser`
- 新增事件映射器：`OpenTcsEventProjector`
  - `BEING_PROCESSED + index` -> `ARRIVED_FROM/ARRIVED_TO`
  - `FINISHED` -> `DROPPED`
  - `FAILED` -> `FAILED`（含原因字段）
- 新增消费器：`OpenTcsSseEventConsumer`
  - 从 `MissionStore` 查询 callback_url
  - 生成回调事件并入 Outbox

### 2.5 幂等与回调重试
- 幂等组件：
  - `IdempotencyService`
  - `IdempotencyStore`
  - `InMemoryIdempotencyStore`
- 回调 Outbox 组件：
  - `CallbackOutboxService`
  - `CallbackRetryProcessor`
  - `CallbackOutboxStore`
  - `InMemoryCallbackOutboxStore`
  - 指数退避重试策略（60s/120s/300s/600s/1800s）

### 2.6 启动入口
- 新增应用主类：`RcsIntegrationApplication`
- 默认端口：`8080`
- 支持通过运行参数传端口

### 2.7 openTCS HTTP 客户端（新增）
- 新增真实客户端实现：`HttpOpenTcsOrderClient`
  - 调用 `POST /v1/transportOrders/{orderName}`
  - 支持超时配置（`rcs.openTcs.timeoutMillis` / `RCS_OPENTCS_TIMEOUT_MILLIS`）
  - 支持失败重试（`rcs.openTcs.maxAttempts` / `RCS_OPENTCS_MAX_ATTEMPTS`）
  - 支持初始退避时间配置（`rcs.openTcs.initialRetryDelayMillis` / `RCS_OPENTCS_INITIAL_RETRY_DELAY_MILLIS`）
  - 支持 Bearer 鉴权（`rcs.openTcs.token` / `RCS_OPENTCS_TOKEN`）
- `RcsIntegrationApplication` 已支持按配置自动切换：
  - 未配置 `rcs.openTcs.baseUrl`（或 `RCS_OPENTCS_BASE_URL`）时，使用 `InMemoryOpenTcsOrderClient`
  - 配置了 baseUrl 时，使用 `HttpOpenTcsOrderClient`

### 2.8 CallbackSender 与事件接入（新增）
- 新增真实回调发送器：`HttpCallbackSender`
  - 回调地址支持绝对 URL 或相对路径（相对路径依赖 `rcs.callback.baseUrl` / `RCS_CALLBACK_BASE_URL`）
  - 支持超时配置（`rcs.callback.timeoutMillis` / `RCS_CALLBACK_TIMEOUT_MILLIS`）
  - 支持 Bearer 鉴权（`rcs.callback.token` / `RCS_CALLBACK_TOKEN`）
  - 支持 HMAC-SHA256 签名（`rcs.callback.signingSecret` / `RCS_CALLBACK_SIGNING_SECRET`）
  - 签名头：
    - `X-Callback-Timestamp`
    - `X-Callback-Signature`
  - 签名内容：`timestamp + "." + payloadJson`
- 新增 openTCS 事件接收接口：
  - `POST /api/v1/opentcs/events/transport-orders`
  - 流程：解析事件 -> 映射为 WCS 事件 -> 入 Outbox -> 触发一次发送（批量大小可配）
  - 批量大小配置：`rcs.callback.dispatchBatchSize` / `RCS_CALLBACK_DISPATCH_BATCH_SIZE`

### 2.9 SSE 长连接消费器（新增）
- 新增长连接消费器：`OpenTcsSseTransportOrderSubscriber`
  - 订阅 openTCS `SSE /events/transportOrders` 事件流
  - 自动重连（指数退避，支持初始/最大重连间隔配置）
  - 断点续传（自动维护 `Last-Event-ID` 并在重连时带上）
  - 支持 openTCS 认证头：
    - `X-Api-Access-Key`（`rcs.openTcs.apiAccessKey` / `RCS_OPENTCS_API_ACCESS_KEY`）
    - `Authorization: Bearer`（复用 `rcs.openTcs.token` / `RCS_OPENTCS_TOKEN`）
- `RcsIntegrationApplication` 已支持可选后台启动 SSE 订阅：
  - 开关：`rcs.openTcs.sse.enabled` / `RCS_OPENTCS_SSE_ENABLED`
  - SSE 地址可直接指定：`rcs.openTcs.sse.url` / `RCS_OPENTCS_SSE_URL`
  - 未指定 SSE 地址时，按 `rcs.openTcs.baseUrl` 自动推导：
    - `{baseUrl}/v1/sse?/events/transportOrders=true`

### 2.10 核心 Store 文件持久化（新增）
- 新增文件持久化实现：
  - `FileMissionStore`
  - `FileIdempotencyStore`
  - `FileCallbackOutboxStore`
- `RcsIntegrationApplication` 支持存储模式切换：
  - `memory`（默认）：使用 `InMemory*Store`
  - `file`：使用 `File*Store`，支持进程重启后恢复
- Outbox 重试状态更新已改为“处理后回写 Store”，确保 `SUCCESS/FAILED/retryCount/nextRetryAt` 能持久化
- 新增配置：
  - `rcs.store.mode` / `RCS_STORE_MODE`（`memory` 或 `file`）
  - `rcs.store.file.dir` / `RCS_STORE_FILE_DIR`（文件存储目录，默认 `./.rcs-store`）

---

## 3. 测试情况

已补充并通过的测试：

- 幂等：
  - `IdempotencyServiceTest`
- payload 映射：
  - `OpenTcsPayloadMapperTest`
- openTCS HTTP 客户端：
  - `HttpOpenTcsOrderClientTest`
- openTCS SSE 消费器：
  - `OpenTcsSseTransportOrderSubscriberTest`
- CallbackSender：
  - `HttpCallbackSenderTest`
- 回调重试：
  - `CallbackRetryProcessorTest`
  - `FileCallbackOutboxStoreTest`
- I-09 服务行为：
  - `WcsMissionServiceTest`
- 文件持久化：
  - `FileMissionStoreTest`
  - `FileIdempotencyStoreTest`
- I-10 映射与消费：
  - `OpenTcsEventProjectorTest`
  - `OpenTcsSsePayloadParserTest`
  - `OpenTcsSseEventConsumerTest`
- 端到端启动与路由：
  - `RcsIntegrationApplicationTest`

执行命令：

```bash
.\gradlew :opentcs-rcs-integration-sample:test
```

结果：`BUILD SUCCESSFUL`

---

## 4. 运行方式

### 4.1 启动样例服务
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample
```

指定端口：
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

### 4.2 调用 I-09 示例
```http
POST /api/v1/wcs/agv/missions
Content-Type: application/json

{
  "missionNo": "M202602100001",
  "taskNo": "T202602090001",
  "fromPoint": "P_WAIT_IN_01",
  "toPoint": "ST_IN_01",
  "palletNo": "PLT000000123",
  "priority": 30,
  "callbackUrl": "/api/v1/wcs/agv/events"
}
```

### 4.3 使用真实 openTCS HTTP 客户端
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample `
  -Drcs.openTcs.baseUrl=http://127.0.0.1:55200 `
  -Drcs.openTcs.token=your-token `
  -Drcs.openTcs.timeoutMillis=3000 `
  -Drcs.openTcs.maxAttempts=3 `
  -Drcs.openTcs.initialRetryDelayMillis=200
```

### 4.4 推送 openTCS 事件并触发回调发送
```http
POST /api/v1/opentcs/events/transport-orders
Content-Type: application/json

{
  "eventTime": "2026-04-14T10:35:21Z",
  "currentObjectState": {
    "name": "M202604140001",
    "state": "FINISHED",
    "currentDriveOrderIndex": 1,
    "processingVehicle": "AGV_01",
    "properties": {
      "task_no": "T202604140001"
    }
  }
}
```

可选回调配置（PowerShell）：
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample `
  -Drcs.callback.baseUrl=http://127.0.0.1:8081 `
  -Drcs.callback.token=your-wcs-token `
  -Drcs.callback.signingSecret=your-signing-secret `
  -Drcs.callback.timeoutMillis=3000 `
  -Drcs.callback.dispatchBatchSize=100
```

### 4.5 启用 openTCS SSE 长连接消费
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample `
  -Drcs.openTcs.baseUrl=http://127.0.0.1:55200 `
  -Drcs.openTcs.sse.enabled=true `
  -Drcs.openTcs.sse.requestTimeoutMillis=600000 `
  -Drcs.openTcs.sse.reconnectInitialDelayMillis=1000 `
  -Drcs.openTcs.sse.reconnectMaxDelayMillis=30000 `
  -Drcs.openTcs.apiAccessKey=your-access-key
```

如果你已经有完整 SSE 地址，也可直接指定：
```bash
-Drcs.openTcs.sse.url=http://127.0.0.1:55200/v1/sse?/events/transportOrders=true
```

### 4.6 启用文件持久化 Store
```bash
.\gradlew :opentcs-rcs-integration-sample:runRcsSample `
  -Drcs.store.mode=file `
  -Drcs.store.file.dir=./.rcs-store
```

---

## 5. 当前边界（尚未生产化）

以下仍是样例实现，后续建议替换：

- `File*Store` -> 数据库持久化（PostgreSQL/MySQL）
- MQTT 真车适配器（I-11）尚未接入此模块

---

## 6. 建议下一步

1. 接入数据库（mission/idempotency/outbox/point_mapping/error_mapping）
2. 接入 MQTT 适配器并打通 I-11
3. 增加可观测性（指标、日志追踪、告警）
