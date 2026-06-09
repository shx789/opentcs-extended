# openTCS RCS 集成样例

本项目基于原始 openTCS 代码扩展，保留 openTCS 原有内核、调度、建模、操作台等模块，并新增一个 RCS 集成样例模块，用于演示 WCS、RCS、openTCS 与 AGV MQTT 之间的任务下发、状态同步和回调闭环。

原始 openTCS 英文 README 可在 `main` 分支或 openTCS 官方仓库查看；本 README 重点说明当前分支相对原始 openTCS 的差异和新增集成能力。

## 功能特性

- **WCS/RCS 任务接口**：提供 WCS 创建、查询、取消 AGV mission 和 WMS 入库/出库任务的 HTTP API。
- **openTCS 订单桥接**：将 WCS/RCS 任务转换为 openTCS transport order，并通过 HTTP 客户端提交给 openTCS。
- **openTCS 事件接收**：支持 HTTP 事件注入和 SSE 订阅，消费 openTCS transport order 状态变化。
- **WCS 回调机制**：将 openTCS 任务状态投影成 WCS 事件回调，提供失败重试和 outbox 存储。
- **AGV MQTT 命令下发**：根据 mission 状态生成 AGV 控制命令，通过 MQTT 发布到机器人控制 topic。
- **AGV MQTT 状态反馈**：订阅 AGV 状态 topic，解析任务执行反馈并同步 RCS/WCS 状态。
- **幂等与状态流转**：对任务创建、取消等关键接口提供幂等保护和状态机校验。
- **文件/内存双存储**：支持 demo 内存模式，也支持文件模式持久化任务、mission、回调、幂等和 AGV command outbox。
- **本地演示页面**：内置 WCS demo 页面和 AGV monitor 页面，便于联调、查看 mission、command、callback 与运行时状态。
- **自动化测试**：新增 RCS 集成模块的单元测试，覆盖 openTCS 映射、SSE 解析、WCS 服务、回调重试、AGV MQTT 命令和反馈等逻辑。

## 技术栈

- **基础平台**：openTCS，Java 21，Gradle。
- **HTTP 服务**：Javalin。
- **JSON 处理**：Jackson。
- **AGV 通信**：Eclipse Paho MQTT Client。
- **任务调度**：openTCS HTTP API 与 SSE 事件流。
- **存储模式**：内存存储、JSON 文件存储。
- **测试框架**：JUnit 5、AssertJ、Mockito。

## 与原始 openTCS 的主要差异

### 1. 新增 Gradle 子模块

原始 openTCS 没有 RCS/WCS/AGV 集成样例。本分支在 [settings.gradle](settings.gradle) 中新增：

```gradle
include 'opentcs-rcs-integration-sample'
```

新增模块目录：

```text
opentcs-rcs-integration-sample/
```

该模块是独立的 RCS 集成服务，不直接改动 openTCS kernel 的核心调度逻辑。

### 2. 新增 MQTT 依赖

在 [gradle/libs.versions.toml](gradle/libs.versions.toml) 中新增 Eclipse Paho MQTT Client：

```toml
mqtt-client = "1.2.5"
mqtt-client = { group = "org.eclipse.paho", name = "org.eclipse.paho.client.mqttv3", version.ref = "mqtt-client" }
```

用于 AGV 命令发布和 AGV 状态订阅。

### 3. 新增 RCS 集成服务

新增启动入口：

```text
opentcs-rcs-integration-sample/src/main/java/org/opentcs/rcs/RcsIntegrationApplication.java
```

它负责组装 HTTP API、openTCS 客户端、SSE 订阅器、WCS 回调 outbox、AGV command outbox、MQTT 发布器和 MQTT 状态订阅器。

### 4. 新增启动/停止脚本

根目录新增本地联调脚本：

```text
demo-rcs-wcs.bat
demo-rcs-wcs-test.bat
demo-rcs-wcs-e2e.bat
start-opentcs-all.bat
start-opentcs-all.ps1
start-opentcs-3windows.sh
stop-opentcs-all.bat
stop-opentcs-all.ps1
```

这些脚本用于启动 openTCS 相关窗口、启动 RCS 样例服务，以及执行端到端 demo 流程。

### 5. 新增协议和联调文档

`opentcs-rcs-integration-sample/` 下新增多份中文/接口文档，用于说明 WCS/RCS、WMS/WCS、AGV MQTT 和真实 AGV 联调流程。

## 项目结构

```text
opentcs/
|-- README.adoc                         # 原始 openTCS README
|-- README.md                           # 当前中文说明文档
|-- settings.gradle                     # 新增 opentcs-rcs-integration-sample 子模块
|-- gradle/libs.versions.toml           # 新增 MQTT 客户端依赖
|-- opentcs-api-base/                   # 原始 openTCS 模块
|-- opentcs-common/                     # 原始 openTCS 模块
|-- opentcs-kernel/                     # 原始 openTCS kernel
|-- opentcs-operationsdesk/             # 原始 openTCS 操作台
|-- opentcs-modeleditor/                # 原始 openTCS 建模工具
|-- opentcs-rcs-integration-sample/     # 新增 RCS/WCS/AGV 集成样例模块
|   |-- build.gradle                    # 子模块构建脚本
|   |-- AGV_RCS_MQTT_PROTOCOL.md        # AGV/RCS MQTT 协议说明
|   |-- WCS_RCS_HTTP_API_CURRENT.md     # 当前 WCS/RCS HTTP API 文档
|   |-- REAL_MACHINE_AGV_COMMISSIONING_CHECKLIST.md
|   |-- scripts/
|   |   |-- WcsMockServer.java
|   |   |-- run-rcs-wcs-demo.ps1
|   |   |-- run-rcs-wcs-e2e.ps1
|   |   `-- wcs-mock-server.ps1
|   |-- src/main/java/org/opentcs/rcs/
|   |   |-- RcsIntegrationApplication.java
|   |   |-- agvcommand/                 # AGV command outbox、重试、查询接口
|   |   |-- api/
|   |   |   |-- dto/                    # WCS/RCS API 请求和响应模型
|   |   |   `-- wcs/                    # WCS/RCS HTTP handler 和业务服务
|   |   |-- bridge/
|   |   |   |-- agv/                    # AGV MQTT 发布、订阅、解析、消费
|   |   |   `-- opentcs/                # openTCS 订单、事件、SSE 桥接
|   |   |-- callback/                   # WCS 回调 outbox 和重试
|   |   |-- core/
|   |   |   |-- idem/                   # 幂等存储和校验
|   |   |   |-- mission/                # mission 存储
|   |   |   `-- task/                   # WCS task 状态和存储
|   |   `-- http/                       # 请求上下文
|   `-- src/test/java/org/opentcs/rcs/  # 新增模块测试
`-- start-opentcs-all.ps1               # 本地启动脚本
```

## 快速开始

### 1. 环境准备

需要本机具备：

- JDK 21
- Windows PowerShell 或可执行 Gradle Wrapper 的 shell
- 可选：openTCS HTTP 服务
- 可选：MQTT Broker，例如 EMQX、Mosquitto

### 2. 编译和测试

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:test
```

只编译新增模块：

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:compileJava
```

### 3. 启动 RCS 集成样例

默认端口启动：

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample
```

指定端口启动：

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

启动后可访问：

```text
http://127.0.0.1:8090/demo/wcs
http://127.0.0.1:8090/demo/agv-monitor
```

### 4. 连接 openTCS

如果需要将任务真实提交到 openTCS，需要配置 openTCS HTTP 服务地址：

```powershell
$env:RCS_OPENTCS_BASE_URL = "http://127.0.0.1:55200"
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

未配置 `RCS_OPENTCS_BASE_URL` 时，RCS 样例会使用内存 openTCS 客户端，适合本地演示和测试。

### 5. 启用 openTCS SSE 事件订阅

```powershell
$env:RCS_OPENTCS_BASE_URL = "http://127.0.0.1:55200"
$env:RCS_OPENTCS_SSE_ENABLED = "true"
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

也可以显式配置 SSE URL：

```powershell
$env:RCS_OPENTCS_SSE_URL = "http://127.0.0.1:55200/v1/sse?/events/transportOrders=true"
```

### 6. 启用 AGV MQTT 命令下发

```powershell
$env:RCS_AGV_COMMAND_ENABLED = "true"
$env:RCS_AGV_COMMAND_BROKER_URI = "tcp://127.0.0.1:1883"
$env:RCS_AGV_COMMAND_TOPIC = "robot_control"
$env:RCS_AGV_POINT_ID_MAP = "Point-0020=20,Point-0026=26"
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

`RCS_AGV_POINT_ID_MAP` 用于把 openTCS 点位名映射为 AGV 协议里的点位 ID。

### 7. 启用 AGV MQTT 状态订阅

```powershell
$env:RCS_AGV_MQTT_ENABLED = "true"
$env:RCS_AGV_MQTT_BROKER_URI = "tcp://127.0.0.1:1883"
$env:RCS_AGV_MQTT_TOPIC = "robot_status"
.\gradlew.bat :opentcs-rcs-integration-sample:runRcsSample -PrcsPort=8090
```

## API 接口

### 1. 创建 AGV mission

**POST** `/api/v1/wcs/agv/missions`

用于 WCS 创建 AGV mission，RCS 会转换为 openTCS transport order。

### 2. 查询 mission 列表

**GET** `/api/v1/wcs/agv/missions`

返回当前 RCS 已记录的 mission 摘要。

### 3. 查询 AGV mission

**GET** `/api/v1/wcs/agv/missions/{mission_no}`

根据 mission 编号查询任务状态。

### 4. 取消 AGV mission

**POST** `/api/v1/wcs/agv/missions/{mission_no}/cancel`

取消指定 mission，并同步 RCS 状态。

### 5. 查询 mission 对应的 AGV 命令

**GET** `/api/v1/wcs/agv/missions/{mission_no}/commands`

返回 mission 触发的 AGV command outbox 记录，便于排查命令是否已经发布、是否重试、是否失败。

### 6. 创建 WMS 入库任务

**POST** `/api/v1/wcs/inbound/tasks`

兼容 WMS/WCS 入库任务接口，内部映射为 RCS/openTCS mission。

### 7. 创建 WMS 出库任务

**POST** `/api/v1/wcs/outbound/tasks`

兼容 WMS/WCS 出库任务接口，内部映射为 RCS/openTCS mission。

### 8. 查询 WMS 任务

**GET** `/api/v1/wcs/tasks/{biz_task_no}`

根据业务任务号查询 RCS 任务状态。

### 9. 取消 WMS 任务

**POST** `/api/v1/wcs/tasks/{biz_task_no}/cancel`

根据业务任务号取消 RCS 任务。

### 10. 接收 openTCS transport order 事件

**POST** `/api/v1/opentcs/events/transport-orders`

用于手动注入或由外部系统转发 openTCS transport order 事件。启用 SSE 时，也可以由 RCS 主动订阅 openTCS 事件流。

### 11. Demo 页面和调试接口

```text
GET  /demo/wcs
GET  /demo/agv-monitor
GET  /api/v1/wcs/agv/runtime
POST /demo/wcs/callback
GET  /demo/wcs/callbacks
POST /demo/wcs/callbacks/clear
POST /demo/wms/inbound-results
POST /demo/wms/outbound-results
POST /api/v1/wms/inbound-results
POST /api/v1/wms/outbound-results
```

## 配置说明

### openTCS 配置

- `RCS_OPENTCS_BASE_URL` / `rcs.openTcs.baseUrl`：openTCS HTTP API 基础地址。
- `RCS_OPENTCS_TIMEOUT_MILLIS` / `rcs.openTcs.timeoutMillis`：HTTP 调用超时。
- `RCS_OPENTCS_MAX_ATTEMPTS` / `rcs.openTcs.maxAttempts`：openTCS 下单最大尝试次数。
- `RCS_OPENTCS_INITIAL_RETRY_DELAY_MILLIS` / `rcs.openTcs.initialRetryDelayMillis`：openTCS 下单初始重试间隔。
- `RCS_OPENTCS_TOKEN` / `rcs.openTcs.token`：openTCS HTTP Bearer Token。
- `RCS_OPENTCS_API_ACCESS_KEY` / `rcs.openTcs.apiAccessKey`：openTCS SSE API access key。
- `RCS_OPENTCS_SSE_ENABLED` / `rcs.openTcs.sse.enabled`：是否启用 openTCS SSE 订阅。
- `RCS_OPENTCS_SSE_URL` / `rcs.openTcs.sse.url`：显式配置 SSE URL。

### WCS 回调配置

- `RCS_WMS_BASE_URL` / `rcs.wms.baseUrl`：WMS 接口基础地址。
- `RCS_CALLBACK_BASE_URL` / `rcs.callback.baseUrl`：WCS callback 相对地址补全用基础地址。
- `RCS_CALLBACK_TIMEOUT_MILLIS` / `rcs.callback.timeoutMillis`：回调 HTTP 超时。
- `RCS_CALLBACK_TOKEN` / `rcs.callback.token`：回调 Bearer Token。
- `RCS_CALLBACK_SIGNING_SECRET` / `rcs.callback.signingSecret`：回调签名密钥。
- `RCS_CALLBACK_DISPATCH_BATCH_SIZE` / `rcs.callback.dispatchBatchSize`：每批回调重试数量。
- `RCS_CALLBACK_RETRY_TICK_MILLIS` / `rcs.callback.retryTickMillis`：回调重试扫描间隔。

### AGV 命令配置

- `RCS_AGV_COMMAND_ENABLED` / `rcs.agvCommand.enabled`：是否启用 AGV command outbox。
- `RCS_AGV_COMMAND_BROKER_URI` / `rcs.agvCommand.brokerUri`：AGV 命令 MQTT Broker。
- `RCS_AGV_COMMAND_CLIENT_ID` / `rcs.agvCommand.clientId`：AGV 命令 MQTT client id。
- `RCS_AGV_COMMAND_TOPIC` / `rcs.agvCommand.topic`：AGV 命令发布 topic。
- `RCS_AGV_COMMAND_QOS` / `rcs.agvCommand.qos`：MQTT QoS。
- `RCS_AGV_COMMAND_USERNAME` / `rcs.agvCommand.username`：MQTT 用户名。
- `RCS_AGV_COMMAND_PASSWORD` / `rcs.agvCommand.password`：MQTT 密码。
- `RCS_AGV_POINT_ID_MAP` / `rcs.agvCommand.pointIdMap`：openTCS 点位名到 AGV 点位 ID 的映射。
- `RCS_AGV_DEFAULT_RUN_SPEED` / `rcs.agvCommand.defaultRunSpeed`：默认运行速度。

### AGV 状态订阅配置

- `RCS_AGV_MQTT_ENABLED` / `rcs.agvMqtt.enabled`：是否启用 AGV 状态 MQTT 订阅。
- `RCS_AGV_MQTT_BROKER_URI` / `rcs.agvMqtt.brokerUri`：AGV 状态 MQTT Broker。
- `RCS_AGV_MQTT_CLIENT_ID` / `rcs.agvMqtt.clientId`：AGV 状态订阅 client id。
- `RCS_AGV_MQTT_TOPIC` / `rcs.agvMqtt.topic`：AGV 状态订阅 topic。
- `RCS_AGV_MQTT_QOS` / `rcs.agvMqtt.qos`：MQTT QoS。
- `RCS_AGV_MQTT_USERNAME` / `rcs.agvMqtt.username`：MQTT 用户名。
- `RCS_AGV_MQTT_PASSWORD` / `rcs.agvMqtt.password`：MQTT 密码。

### 存储配置

- `RCS_STORE_MODE` / `rcs.store.mode`：存储模式，支持 `memory` 和 `file`。
- `RCS_STORE_FILE_DIR` / `rcs.store.file.dir`：文件存储目录，默认 `.rcs-store`。

## 测试

运行新增 RCS 集成模块所有测试：

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:test
```

运行指定测试类：

```powershell
.\gradlew.bat :opentcs-rcs-integration-sample:test --tests org.opentcs.rcs.bridge.opentcs.OpenTcsPayloadMapperTest
```

测试覆盖重点：

- WCS mission 创建、查询、取消。
- WMS 入库/出库任务接口。
- 幂等冲突和重复请求。
- openTCS transport order 请求映射。
- openTCS SSE payload 解析和事件消费。
- WCS callback outbox 重试。
- AGV command outbox 生成、发布和重试。
- AGV MQTT 状态消息解析和事件消费。
- 文件存储读写。

## 任务流程说明

### WCS 创建 mission

1. WCS 调用 `/api/v1/wcs/agv/missions`。
2. RCS 校验请求并记录幂等信息。
3. RCS 将 mission 转换为 openTCS transport order。
4. RCS 调用 openTCS HTTP API 创建订单。
5. RCS 保存 mission 和 task 状态。
6. openTCS 状态变化后，RCS 通过 SSE 或 HTTP 事件接口接收事件。
7. RCS 将 openTCS 状态投影为 WCS 回调事件。
8. 如启用 AGV command，RCS 生成 AGV MQTT 控制命令并发布。
9. 如启用 AGV 状态订阅，RCS 根据 AGV MQTT 反馈继续更新状态。

### WMS 入库/出库任务

1. WMS 调用 `/api/v1/wcs/inbound/tasks` 或 `/api/v1/wcs/outbound/tasks`。
2. RCS 生成 mission/task 映射关系。
3. RCS 下发 openTCS transport order。
4. 任务状态变化后，RCS 回调 WMS 入库/出库结果接口。

## 注意事项

1. `opentcs-rcs-integration-sample` 是新增集成样例模块，不是原始 openTCS 官方模块。
2. 当前分支尽量保持 openTCS kernel 核心逻辑不变，集成逻辑集中在新增模块内。
3. 未配置 openTCS 地址时，服务会使用内存 openTCS 客户端，适合 demo，不代表真实调度。
4. 启用 AGV command 时必须配置点位映射，否则无法把 openTCS 点位转换为 AGV 协议点位 ID。
5. 文件存储模式适合本地联调，不建议直接作为生产级数据库使用。
6. MQTT Broker、openTCS HTTP 服务和 WMS 回调地址需要按现场网络环境配置。
7. 根目录的 `README.md` 和 `README.adoc` 都是当前扩展分支的中文说明；原始 openTCS 英文 README 可在 `main` 分支或官方仓库查看。

## 版本历史

- **feature/integration-sample**
  - 新增 `opentcs-rcs-integration-sample` Gradle 子模块。
  - 新增 WCS/RCS HTTP API。
  - 新增 WMS 入库/出库任务兼容接口。
  - 新增 openTCS transport order 映射和 HTTP 客户端。
  - 新增 openTCS SSE 订阅和事件消费。
  - 新增 WCS callback outbox 与重试机制。
  - 新增 AGV command outbox、MQTT 命令发布和重试机制。
  - 新增 AGV MQTT 状态订阅、解析和反馈消费。
  - 新增 WCS demo 页面和 AGV monitor 页面。
  - 新增本地启动、停止和端到端演示脚本。
  - 新增集成模块单元测试与联调文档。
