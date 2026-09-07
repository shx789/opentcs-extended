# Windows 现场接车流程说明

本文说明 `agv-opentcs-adapter-windows` 在现场 Windows 电脑上的启动顺序、配置方式、数据流和排查方法。

## 组件关系

真实接车时，openTCS 不直接连接小车。Windows 电脑上的 adapter 负责在小车 MQTT 和 openTCS HTTP 之间做转换。

```text
小车 / AGV 控制器
  |
  | MQTT: task_feedback / robot_control
  v
agv-opentcs-adapter-windows
  |
  | HTTP: PUT /v1/vehicles/{vehicle}/position
  v
openTCS Kernel
  |
  v
Operations Desk / 联调大屏 / RCS
```

## 页面入口

- 联调大屏：`http://127.0.0.1:8092`
- 配置网页：`http://127.0.0.1:8091`
- openTCS Kernel HTTP：`http://127.0.0.1:55200`

两个页面定位不同：

- `8092` 是联调大屏，适合看车辆、拓扑、任务、回调、真实小车连接配置。
- `8091` 是轻量配置页，适合直接改 `config/runtime_config.json`、看 adapter 状态、生成候选拓扑。

## 转移到其他电脑

服务运行时按自身目录定位文件，不依赖开发机上的 `E:\workspace\opentcs`。

可以整体拷贝这个目录到其他 Windows 电脑：

```text
agv-opentcs-adapter-windows
```

需要重新确认的配置：

- `mqtt_uri`：目标电脑连接小车 WiFi 后，小车 MQTT Broker 的 IP。
- `open_tcs_base_url`：openTCS Kernel 地址。如果 openTCS 也在本机，一般是 `http://127.0.0.1:55200`。
- `rcs_base_url`：RCS 地址。如果本机不跑 RCS，要改成实际服务器地址。
- `vehicle_name`：必须和 openTCS 模型里的车辆名一致。
- `point_id_map`：必须和目标小车反馈的点位 ID 一致。

启动脚本会优先使用同目录下的 exe。如果没有 exe，则使用系统 `python`。如果目标电脑 Python 不在 PATH，可以先设置：

```bat
set PYTHON_EXE=C:\path\to\python.exe
```

再执行启动脚本。

## 启动顺序

### 1. 启动 openTCS

先启动 openTCS Kernel，确保 HTTP 端口可用：

```text
http://127.0.0.1:55200/v1/kernel/version
```

如果这个接口不通，adapter 即使收到小车反馈，也无法把位置同步到 openTCS。

### 2. 启动配置网页

在 `agv-opentcs-adapter-windows` 目录执行：

```bat
start_config_web.bat
```

访问：

```text
http://127.0.0.1:8091
```

### 3. 修改真实小车配置

主要配置文件：

```text
config/runtime_config.json
```

关键字段：

```json
{
  "mqtt_uri": "mqtt://192.168.xxx.xxx:1883/",
  "source_topic": "task_feedback",
  "command_topic": "robot_control",
  "open_tcs_base_url": "http://127.0.0.1:55200",
  "rcs_base_url": "http://127.0.0.1:8080",
  "vehicle_name": "Vehicle-01",
  "point_id_map": "nav:0=ST_IN_01,nav:1=P_WAIT_IN_01"
}
```

现场电脑连接小车 WiFi 后，`mqtt_uri` 不能继续用 `127.0.0.1`，要改成小车 MQTT Broker 在 WiFi 网段里的 IP。

### 4. 启动 adapter

确认配置后执行：

```bat
start_adapter.bat
```

adapter 会订阅 `source_topic`，收到小车反馈后解析点位，并调用 openTCS 更新车辆位置。

## 热加载机制

adapter 会检查：

```text
config/runtime_config.json
```

保存配置后，不需要重启 openTCS。以下配置支持热加载：

- `mqtt_uri`
- `source_topic`
- `command_topic`
- `forward_command_topic`
- `open_tcs_base_url`
- `rcs_base_url`
- `vehicle_name`
- `point_id_map`
- `distance_tolerance_m`
- `publish_rcs_event`

如果改了 MQTT 地址或 topic，adapter 会自动重连。

如果 MQTT 当前连不上，adapter 也会在下一轮重试前重新读取配置，方便现场不断调整 IP 和 topic。

## 点位映射

小车反馈通常会带点位 ID、导航点 ID 或坐标。adapter 会按以下优先级解析：

1. `type:id` 映射，例如 `nav:0=ST_IN_01`
2. 普通 `id` 映射，例如 `0=ST_IN_01`
3. 坐标距离匹配，使用 `mapping_file` 和 `distance_tolerance_m`

推荐现场显式配置 `point_id_map`，不要只依赖坐标匹配。

示例：

```text
nav:0=ST_IN_01,nav:1=P_WAIT_IN_01,point:0=ST_OUT_01,point:1=P_SHIP_01
```

## 日志和状态

启动脚本会写日志到：

```text
logs/agv_config_web.out.log
logs/agv_config_web.err.log
logs/agv_native_feedback_adapter.out.log
logs/agv_native_feedback_adapter.err.log
logs/agv_no_car_feedback_simulator.out.log
logs/agv_no_car_feedback_simulator.err.log
```

状态文件：

```text
config/agv_config_web_status.json
logs/agv_native_feedback_adapter_status.json
logs/agv_no_car_feedback_simulator_status.json
```

查看状态：

```bat
status_config_web.bat
status_adapter.bat
status_no_car_sim.bat
```

## 常见问题

### 页面能打开，但小车状态不动

检查：

- openTCS Kernel 是否通：`http://127.0.0.1:55200/v1/kernel/version`
- `vehicle_name` 是否和 openTCS 模型里的车辆名一致
- 小车 MQTT IP 是否能从 Windows 电脑访问
- `source_topic` 是否是小车实际发布反馈的 topic
- `point_id_map` 里是否有小车反馈里的 `type/id`

### MQTT 显示不通

如果真实小车是 MQTT Broker，本机 `127.0.0.1:1883` 不通是正常的。需要把 `mqtt_uri` 改成小车 WiFi 地址，例如：

```text
mqtt://192.168.254.100:1883/
```

### 改了配置没生效

优先看：

```text
logs/agv_native_feedback_adapter_status.json
```

里面会记录最近一次 reload 时间、MQTT 地址、topic 和错误信息。

### openTCS 里没看到车辆位置

检查：

- openTCS 是否已经加载正确地图
- `vehicle_name` 是否存在
- adapter 日志里 `openTcsResponse` 是否成功
- 点位名是否存在于 openTCS 模型中

如果 openTCS 返回类似 `404 ["POINT01"]`，说明请求里的点位名不在当前模型里，需要改 `point_id_map` 或模型点位名。

## 地图拓扑生成

配置页支持从 ROS `map.yaml` / PGM 生成候选 openTCS 拓扑。

入口：

```text
http://127.0.0.1:8091
```

页面可以直接上传地图文件。需要分别选择：

- `map.yaml`
- `map.pgm`

上传成功后，页面会自动把上传后的 `map.yaml` 路径填入生成表单。

生成时可以选择简化级别：

- `详细`：保留更多候选点，适合先观察地图细节。
- `均衡`：默认推荐，点和路径数量适中。
- `简化`：减少候选点，适合快速做业务点位草图。
- `极简`：只保留更粗的主干候选路径。

如果生成结果路径太少或没有闭环，打开 `补闭环路径`。它会在主干路径生成后补近邻边，减少断环。

输出目录默认：

```text
config/generated
```

输出文件：

- `agv_opentcs_mapping.json`
- `opentcs_plant_model_candidate.json`
- `plant_model.xml`
- `preview.png`

点击 `加载到 openTCS` 时，页面会把 `opentcs_plant_model_candidate.json` 通过 openTCS HTTP API 写入 Kernel：

```text
PUT /v1/plantModel
```

加载前会强制只保留一辆车，车辆名使用配置里的 `vehicle_name`，默认 `Vehicle-01`。

生成结果只是候选拓扑。真实接车前必须人工检查点位、路径、方向、库位、block、防火门等业务规则。

## 推荐现场操作顺序

1. Windows 电脑连接小车 WiFi。
2. 启动 openTCS Kernel。
3. 启动 `start_config_web.bat`。
4. 打开 `http://127.0.0.1:8091`。
5. 修改 `mqtt_uri`、topic、车辆名、`point_id_map`。
6. 保存配置。
7. 启动 `start_adapter.bat`。
8. 打开 `http://127.0.0.1:8092` 看车辆和任务状态。
9. 小车发一条反馈后，确认 openTCS 车辆位置是否更新。
10. 如果不更新，先看 adapter 状态和日志，再看 openTCS 点位名是否匹配。
