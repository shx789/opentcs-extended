# Web Config UI

## 用途

这个页面用于直接修改：

- `config/runtime_config.json`

适用场景：

- 现场边缘机本机调试
- 内网联调
- 演示环境快速改 MQTT 地址、openTCS 地址、RCS 地址

## 启动

```bat
start_config_web.bat
```

默认访问地址：

- `http://127.0.0.1:8091`

## 能力

- 查看当前配置
- 用表单修改 MQTT、topic、openTCS/RCS、车辆名、点位映射等常用配置
- 加载模板
- 保存配置
- 查看 adapter 状态
- 查看 no-car simulator 状态
- 查看 web 服务自身状态
- 从 ROS `map.yaml`/PGM 生成 openTCS 候选拓扑

## 地图拓扑生成

页面中的“地图拓扑生成”区域会调用：

```bat
generate_map_topology.bat
```

输入：

- `map.yaml`：ROS 地图描述文件
- 上传地图文件：分别选择 `map.yaml` 和 `map.pgm`，上传后页面会自动填入上传后的 `map.yaml` 路径
- 生成模式：`blackline-skeleton`、`free-space-skeleton`、`hough-line`、`free-space`
- 简化级别：`详细`、`均衡`、`简化`、`极简`，用于控制候选点和路径数量
- 补闭环路径：生成主干后自动补近邻边，减少断环
- 输出目录：默认 `config\generated`

输出：

- `agv_opentcs_mapping.json`
- `opentcs_plant_model_candidate.json`
- `plant_model.xml`
- `preview.png`

点击页面上的 `加载到 openTCS` 会把 `opentcs_plant_model_candidate.json` 写入 openTCS Kernel 的 `/v1/plantModel`。加载前会强制只保留一辆车，车辆名取当前配置中的 `vehicle_name`。

生成结果是候选拓扑，需要人工验证和调整后再用于真实小车。

## 注意

- 当前没有登录鉴权
- 不建议直接暴露到公网
- 页面可以触发本地拓扑生成脚本，如果要远程使用，建议放在受控 VPN 或内网后面
