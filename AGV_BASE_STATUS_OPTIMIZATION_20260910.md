# AGV base_status 与任务容错优化建议

日期：2026-09-10  
适用模块：`opentcs-commadapter-mqtt`、Windows AGV 套件、RCS 任务链路

## 一、当前 base_status 示例

```json
{
  "local": {
    "amcl": 0.9220,
    "location": true
  },
  "magnetic": {
    "low": true,
    "material": false,
    "up": false
  },
  "pose": {
    "x": -2.10,
    "y": 0.2690,
    "yaw": -0.1360
  },
  "robot": {
    "mainerror": 3,
    "robot_status": 8,
    "suberror": 12
  }
}
```

字段含义：

- `local.amcl`：地图匹配度，通常最大约为 1，低于 0.5 时定位质量较低。
- `local.location`：地图定位标志，`true` 通常表示定位已成功或基本成功。
- `pose.x/y/yaw`：AGV 在地图坐标系中的实时位置和朝向。
- `robot.robot_status`：AGV 当前系统模式。
- `robot.mainerror/suberror`：AGV 当前主、子错误码。

## 二、推荐优化项

### 1. 导航前定位检查

普通导航或磁导航下发前，检查：

- `local.location == true`
- `amcl` 大于可配置阈值（建议默认 `0.5`）
- `robot_status` 不是错误、建图、回充或暂停模式
- 主/子错误码不属于阻断任务的错误

定位不满足时可以等待恢复；超过定位等待超时后，再将当前命令失败。

### 2. 导航过程实时错误监控

收到 `base_status` 时持续检查：

- `robot_status == 9`
- `mainerror != 0`
- `suberror != 0`

发现阻断性故障时，普通导航发送 `interest_point_control` 的 `stop`，当前命令失败，并清理适配器状态。错误码是否阻断应支持白名单或配置，避免把提示性代码误判为故障。

### 3. 根据 pose 同步 openTCS 位置

可以将 `base_status.pose.x/y` 与 openTCS 点位坐标进行最近点匹配：

```text
读取 AGV pose
→ 计算与各点位的距离
→ 最近点距离小于阈值时
→ 更新 VehicleProcessModel.position
```

建议：

- 距离阈值可配置，初始可使用 `0.5~1.2m`。
- 优先在车辆空闲或导航成功后更新，避免覆盖正在执行中的目标位置。
- 磁导航进出库时要排除库位内部辅助点，或使用独立点位白名单。
- 必须确认 AGV 地图坐标系与 openTCS 点位坐标系一致。

### 4. 使用 yaw 辅助点 1 磁导航

点 1 进入库位前，可以检查 `pose.yaw` 是否满足进入方向；方向不满足时暂缓磁导航，避免错误方向进入狭窄库位。

### 5. 增加定位质量超时

定位持续不满足：

```text
location=false 或 amcl 低于阈值
→ 等待定位恢复
→ 超过定位等待时间
→ 当前任务失败
```

日志中应明确记录是定位超时，而不是导航无反馈。

### 6. 按任务阶段校验 robot_status

| 任务阶段 | 期望模式 |
|---|---|
| 普通导航 | `1` 导航 |
| 点 1 磁导航 | `12` 磁导航 |
| 空闲 | `0` 空闲 |
| 定位等待 | `local.location=true` 且 `amcl` 达标 |
| 错误 | `9` 错误 |

该校验用于诊断和安全保护，不应替代 `task_feedback` 的成功/失败判断。

## 三、模式值说明

```text
0  空闲
1  导航
2  建图
3  顺序巡航
4  随机巡航
5  回充
6  轨迹巡航
7  多任务
8  多任务暂停
9  错误
10 定位模式
11 二维码定位模式
12 磁导航模式
```

## 四、当前示例状态的解释

```text
amcl=0.922、location=true：定位质量良好
material=false、up=false：当前空载且顶已下降
robot_status=8：多任务暂停
mainerror=3、suberror=12：存在错误码，需要结合 AGV 错误码定义判断是否阻断任务
```

当前适配器主要记录错误日志，不会仅凭这两个错误码自动失败任务。后续实现前应先确认错误码含义和可忽略错误码范围。

## 五、推荐实现顺序

1. 解析并缓存 `local`、`pose`、`robot_status`、错误码。
2. 导航中遇到明确错误模式时立即停止并失败。
3. 增加定位等待与定位质量超时。
4. 使用 pose 最近点匹配同步 openTCS 位置。
5. 最后再增加 yaw 方向校验和更细的模式校验。

## 六、主要风险

- AGV 不返回中间点时，pose 匹配是位置同步的重要补充，但不能替代最终任务反馈。
- 错误码可能包含告警、历史残留或非阻断状态，必须配置化处理。
- 超时后直接恢复 IDLE 便于继续接单，但应确认 stop 命令已经实际生效。
- 同一点导航可能被 AGV 特殊处理，需验证其是否返回 `task_feedback success`。
