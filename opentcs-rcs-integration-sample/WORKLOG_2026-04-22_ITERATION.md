# WORKLOG 2026-04-22（迭代记录）

## 1. 今日目标
- 真实打通 `demo-wcs -> RCS -> openTCS -> RCS回调 -> demo-wcs`，可在前端看到回调。
- 定位并修复 demo 前端体验问题（回调清空/刷新、布局比例抖动、中文可读性）。
- 回调中补齐点位信息，支持看到“当前所在点位”。

## 2. 今日完成

### 2.1 真实链路联调（非注入）
- 确认 `openTCS`（55200）和 `RCS/demo-wcs`（8090）可访问。
- 校正车辆可调度状态（`integrationLevel=TO_BE_UTILIZED`）后，验证真实订单执行与回调。
- 核实回调终态可见：`DROPPED`，并能在 demo-wcs 回调收件箱中查看。

### 2.2 回调 point_id 从 null 改为有值
- 之前问题：`ARRIVED_TO/ARRIVED_FROM/DROPPED` 的 `point_id` 固定为 `null`。
- 改造后：
  - 解析 openTCS SSE 的 `driveOrders/route/currentRouteStepIndex`。
  - 回调 `point_id` 优先映射“当前所在点位”，不再只是目标点。
  - 订单 `FINISHED` 时回填最终点位。
- 相关代码：
  - `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsTransportOrderEvent.java`
  - `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsSsePayloadParser.java`
  - `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsEventProjector.java`

### 2.3 demo-wcs 前端改造（中文 + 可读 + 可验证）
- 页面整体中文化，日志与操作区可读性增强。
- 回调收件箱改为卡片列表展示：`event_type/mission_no/task_no/agv_id/point_id/receivedAt`。
- `Clear` 后自动 `Refresh`，空状态明确提示“已清空，等待新回调”。
- 增加原始载荷折叠查看，便于定位问题。
- 相关代码：
  - `src/main/java/org/opentcs/rcs/api/wcs/WcsDemoHttpHandlers.java`

### 2.4 修复“刷新后比例变化”
- 问题：刷新后页面在临界宽度下出现布局跳变（看起来比例变化）。
- 修复：
  - 固定滚动条占位（`scrollbar-gutter: stable` + `overflow-y: scroll`）。
  - 网格子项加 `min-width: 0` 防止撑宽。
  - 回调收件箱卡片固定高度与内部滚动区域，刷新只更新内容不改外框比例。
  - 断点从 `1100` 调整到 `1024`，减少临界抖动。

## 3. 测试与验证

### 3.1 自动化验证
- 通过：`./gradlew.bat :opentcs-rcs-integration-sample:compileJava`
- 通过：
  - `OpenTcsSsePayloadParserTest`
  - `OpenTcsEventProjectorTest`
  - `OpenTcsSseEventConsumerTest`

### 3.2 联调样例（今日）
- 点位回传验证：
  - `mission_no=MPOINT1776866783626`
  - `event_type=DROPPED`
  - `point_id=Point-0026`
- 当前点位优先验证（非目标点）：
  - `mission_no=MCUR1776867120221`
  - 目标 `to_point=Point-0004`
  - 回调 `point_id=Point-0001`（符合“当前所在点位”）

## 4. 当前可用页面（新实例）
- `http://127.0.0.1:8096/demo/wcs`（含比例修复）
- 说明：旧端口进程可能仍是旧页面，验证请优先用新端口并强刷（Ctrl+F5）。

## 5. 明日建议迭代
1. 将最新页面和回调点位逻辑切回固定主端口（如 8090），并清理临时测试实例。
2. 增加回调去重展示（相同 `mission_no + event_type + event_time` 合并）。
3. 在回调卡片上增加筛选（按 mission_no / event_type / agv_id）。
4. 为“当前点位推断”补充更多边界测试（多段路径、重路由、失败态）。
5. 若接入真实 AGV，补充“车辆实时位置源”优先级策略（SSE/车辆接口/缓存）。

## 6. 风险与注意事项
- 当前同机可能存在多个 RCS 进程（8090/8091/8092/8094/8095/8096），联调时注意端口一致性。
- `point_id` 目前来自 openTCS 事件推断，若上游事件字段缺失，仍可能退化到目的地点位。

## 7. 相关修改文件（本次迭代）
- `src/main/java/org/opentcs/rcs/api/wcs/WcsDemoHttpHandlers.java`
  - demo-wcs 中文化、回调收件箱展示改造、Clear/Refresh 交互优化、比例抖动修复、point_id 展示。
- `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsTransportOrderEvent.java`
  - 增加 `currentPointId` 字段，承载事件当前点位。
- `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsSsePayloadParser.java`
  - 解析 `currentRouteStepIndex/driveOrders/route.steps`，推断“当前所在点位优先”的 `point_id`。
- `src/main/java/org/opentcs/rcs/bridge/opentcs/OpenTcsEventProjector.java`
  - 将 `event.currentPointId()` 投影到回调 `point_id`。
- `src/test/java/org/opentcs/rcs/bridge/opentcs/OpenTcsSsePayloadParserTest.java`
  - 新增/更新当前点位推断场景测试。
- `src/test/java/org/opentcs/rcs/bridge/opentcs/OpenTcsEventProjectorTest.java`
  - 增加 `point_id` 映射断言。
- `src/test/java/org/opentcs/rcs/bridge/opentcs/OpenTcsSseEventConsumerTest.java`
  - 同步事件模型字段变更，验证消费链路不回归。
- `WORKLOG_2026-04-22_ITERATION.md`
  - 今日总结与明日待办记录（本文件）。
