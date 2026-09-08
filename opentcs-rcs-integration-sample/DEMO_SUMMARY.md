# openTCS RCS/WCS 联调改造与 Demo 总结（2026-04-20）

## 1. 目标

围绕你提的核心问题“**WCS 下发 RCS 搬运任务，RCS 如何落地执行**”，本次完成了两部分：

1. 对 `opentcs-rcs-integration-sample` 做面向联调的接口增强（可直接对接 WCS）。
2. 提供 `.bat` 一键 demo（含模拟 WCS 回调服务），便于本机快速演示闭环流程。

---

## 2. 本次代码改造结果

### 2.1 接口能力

已支持：

- `POST /api/v1/wcs/agv/missions`（创建任务）
- `POST /api/v1/wcs/agv/missions/{mission_no}/cancel`（取消任务）
- `GET /api/v1/wcs/agv/missions/{mission_no}`（查询任务）

关键入口：

- `src/main/java/org/opentcs/rcs/RcsIntegrationApplication.java`
- `src/main/java/org/opentcs/rcs/api/wcs/WcsMissionHttpHandlers.java`
- `src/main/java/org/opentcs/rcs/api/wcs/WcsMissionService.java`

### 2.2 JSON 与响应规范

- 输入/输出字段对齐 `snake_case`（并兼容历史 `camelCase` 入参）。
- 统一响应壳：`{ code, msg, data }`

DTO 关键文件：

- `src/main/java/org/opentcs/rcs/api/dto/CreateMissionReq.java`
- `src/main/java/org/opentcs/rcs/api/dto/CreateMissionResp.java`
- `src/main/java/org/opentcs/rcs/api/dto/AgvEventCallbackReq.java`
- `src/main/java/org/opentcs/rcs/api/dto/ApiResponse.java`
- `src/main/java/org/opentcs/rcs/api/dto/CancelMissionResp.java`
- `src/main/java/org/opentcs/rcs/api/dto/QueryMissionResp.java`

### 2.3 错误码映射

通过全局异常映射返回：

- `RCS-4001`：参数校验失败
- `RCS-4009`：幂等冲突（同 `mission_no` 不同请求体）
- `RCS-5000`：系统内部错误

关键文件：

- `src/main/java/org/opentcs/rcs/RcsIntegrationApplication.java`
- `src/main/java/org/opentcs/rcs/core/idem/IdempotencyConflictException.java`
- `src/main/java/org/opentcs/rcs/core/idem/IdempotencyService.java`

### 2.4 任务状态管理

`MissionStore` 增加状态更新语义，任务状态可追踪：

- `RECEIVED`
- `IN_PROGRESS`
- `DONE`
- `FAILED`
- `CANCELED`

关键文件：

- `src/main/java/org/opentcs/rcs/core/mission/MissionCallbackTarget.java`
- `src/main/java/org/opentcs/rcs/core/mission/MissionStore.java`
- `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsSseEventConsumer.java`

### 2.5 openTCS 取消任务打通

新增 openTCS cancel 调用（withdraw）：

- `POST /v1/transportOrders/{orderName}/withdrawal?immediate=true`

关键文件：

- `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsOrderClient.java`
- `src/main/java/org/opentcs/rcs/bridge/opentcs/InMemoryOpenTcsOrderClient.java`
- `src/main/java/org/opentcs/rcs/bridge/opentcs/HttpOpenTcsOrderClient.java`

---

## 3. 测试覆盖结果

已更新并通过测试（含新增场景）：

- `WcsMissionServiceTest`：创建幂等、取消、查询
- `RcsIntegrationApplicationTest`：端到端创建/回调/持久化、取消查询、幂等冲突 `RCS-4009`
- `OpenTcsSseEventConsumerTest`：事件驱动状态推进

执行命令：

```bat
.\gradlew :opentcs-rcs-integration-sample:test
```

---

## 4. 新增 Demo 脚本

### 4.1 在线联调版（RCS + 模拟 WCS）

- `demo-rcs-wcs.bat`
- `scripts/run-rcs-wcs-demo.ps1`
- `scripts/WcsMockServer.java`

流程：

1. 启动 mock WCS 回调服务
2. 启动 RCS sample
3. Mission A：创建 -> 查询 -> 取消 -> 查询
4. Mission B：创建 -> 注入 openTCS 事件 -> 验证回调日志 -> 查询状态

### 4.2 测试版 Demo（稳定）

- `demo-rcs-wcs-test.bat`

运行后可查看报告：

- `build/reports/tests/test/index.html`

---

## 5. 本机运行建议

### 推荐顺序

1. 先跑测试版 demo：

```bat
.\demo-rcs-wcs-test.bat
```

2. 再跑在线联调版：

```bat
.\demo-rcs-wcs.bat
```

### 可选：真实 openTCS 联调启动（手工）

```bat
.\gradlew :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090 -Drcs.openTcs.baseUrl=http://127.0.0.1:55200 -Drcs.openTcs.token=your-token
```

---

## 6. 说明（本次执行环境限制）

在当前受限沙箱里，`gradlew` 子进程会落到受限用户目录，导致在线 demo 启动受阻（`.gradle` lock 文件权限问题）。  
这属于当前执行环境限制，不影响你在本机正常权限下运行上述 `.bat` demo。

---

## 7. 下一步建议

1. 修正 `priority` 语义与文档一致（当前代码/文档存在“值大优先”与“deadline 映射”语义差异）。
2. 补充 `trace_id/request_id` 透传与日志串联。
3. 将 `Outbox retry` 改为独立调度线程，避免仅靠“有新事件时触发重试”。
4. 若要正式上线，建议把 `File*Store` 切到 DB 持久化。

