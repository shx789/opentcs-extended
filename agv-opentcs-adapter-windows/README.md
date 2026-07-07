# AGV openTCS Adapter for Windows

这个目录是独立的 Windows 部署版，不修改当前 Linux 版目录。

目录位置：

- `opentcs/opentcs/agv-opentcs-adapter-windows`

用途：

- 作为 **现场边缘机上的 Windows 适配服务**
- 或作为 **无车演示/联调版 Windows 适配服务**

它的目标是不改第三方 AGV/ROS 代码，只消费既有 MQTT topic，把 AGV 的反馈同步到 openTCS。

## 目录说明

- `bin/agv_native_feedback_adapter.py`：主适配器
- `bin/agv_no_car_feedback_simulator.py`：无车反馈模拟器
- `config/runtime_config.json`：运行配置，支持热加载
- `config/runtime_config.example.json`：配置模板
- `config/agv_opentcs_business_topology_draft.json`：AGV 到 openTCS 点位映射
- `config/interest_point.sample.json`：无车演示点位样例
- `start_adapter.bat`：启动适配器
- `stop_adapter.ps1`：停止适配器
- `status_adapter.bat`：查看适配器状态
- `start_no_car_sim.bat`：启动无车模拟器
- `stop_no_car_sim.ps1`：停止无车模拟器
- `status_no_car_sim.bat`：查看无车模拟器状态
- `install_service_nssm.ps1`：用 NSSM 安装 Windows 服务
- `uninstall_service_nssm.ps1`：卸载 Windows 服务
- `requirements.txt`：Python 依赖
- `requirements-build.txt`：打包依赖
- `build_windows_exe.ps1`：构建 `exe`
- `package_release.ps1`：把 `release/` 打成 zip
- `validate_config.bat`：配置校验入口
- `bin/validate_runtime_config.py`：配置校验程序
- `bin/agv_config_web.py`：配置网页服务
- `web/`：网页静态资源
- `start_config_web.bat`：启动配置网页
- `stop_config_web.ps1`：停止配置网页
- `status_config_web.bat`：查看配置网页状态

## 两种使用方式

### 1）真实小车接入

推荐部署方式：

- `openTCS/RCS/Web` 放在服务器
- `agv-opentcs-adapter-windows` 放在现场 Windows 边缘机
- 边缘机同时连：
  - 小车内网/WiFi
  - 业务网或公网

只启动：

- `start_adapter.bat`

配置重点：

- `config/runtime_config.json`
  - `mqtt_uri`：小车 MQTT 地址，例如 `mqtt://192.168.254.100:1883/`
  - `source_topic`：通常 `task_feedback`
  - `command_topic`：通常 `robot_control`
  - `open_tcs_base_url`：openTCS HTTP 地址
  - `rcs_base_url`：RCS 地址
  - `vehicle_name`：例如 `Vehicle-01`
  - `point_id_map`：AGV 点位 ID 到 openTCS 点名的映射

### 2）无车演示 / 联调

同时启动：

- `start_adapter.bat`
- `start_no_car_sim.bat`

用途：

- 没有真实小车时，模拟 `robot_control -> task_feedback`
- 用于演示 openTCS/RCS/adapter 全链路

## 快速启动

先安装 Python 依赖：

```bat
cd agv-opentcs-adapter-windows
python -m pip install -r requirements.txt
```

校验配置：

```bat
validate_config.bat
```

启动适配器：

```bat
start_adapter.bat
```

启动配置网页：

```bat
start_config_web.bat
```

打开浏览器：

- `http://127.0.0.1:8091`

查看状态：

```bat
status_adapter.bat
```

停止适配器：

```powershell
powershell -ExecutionPolicy Bypass -File .\stop_adapter.ps1
```

## 安装为 Windows 服务

当前推荐：

- `Python + NSSM`

前提：

- 机器上已安装 Python
- 你自己准备 `nssm.exe`
- 放到 `tools/nssm.exe`

安装适配器服务：

```powershell
powershell -ExecutionPolicy Bypass -File .\install_service_nssm.ps1
```

安装“适配器 + 无车模拟器”两个服务：

```powershell
powershell -ExecutionPolicy Bypass -File .\install_service_nssm.ps1 -InstallNoCarSimulator
```

如果已经打包成 `exe`，建议安装服务时直接使用可执行文件：

```powershell
powershell -ExecutionPolicy Bypass -File .\install_service_nssm.ps1 -UseExe
```

如果希望网页配置也作为服务常驻：

```powershell
powershell -ExecutionPolicy Bypass -File .\install_service_nssm.ps1 -InstallConfigWeb
```

卸载：

```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall_service_nssm.ps1
```

## 热加载

`bin/agv_native_feedback_adapter.py` 会定期检查：

- `config/runtime_config.json`

当前支持：

- 改 `mqtt_uri` 后自动重连
- 改 `point_id_map` 后自动刷新映射
- 改 `open_tcs_base_url` / `rcs_base_url` 后自动使用新地址

所以现场改 IP 时，通常不需要重启整个 openTCS/RCS。

## 网页配置

网页服务默认监听：

- `127.0.0.1:8091`

页面能力：

- 查看当前 `runtime_config.json`
- 加载真实小车/无车模板
- 在线编辑并保存配置
- 查看 adapter / no-car simulator / web 自身状态

保存后：

- 实际写入 `config/runtime_config.json`
- adapter 继续使用已有热加载逻辑自动重读

注意：

- 当前网页是本地轻量服务，不带登录鉴权
- 更适合边缘机本机使用或内网受控环境使用
- 如果后续要开放给远程访问，必须再加认证和访问控制

## 打包成 exe

先安装打包依赖：

```bat
python -m pip install -r requirements-build.txt
```

只打包适配器：

```powershell
powershell -ExecutionPolicy Bypass -File .\build_windows_exe.ps1
```

打包适配器和无车模拟器：

```powershell
powershell -ExecutionPolicy Bypass -File .\build_windows_exe.ps1 -IncludeNoCarSimulator
```

输出目录默认：

- `release/`

输出内容包括：

- `agv-native-feedback-adapter.exe`
- `validate-runtime-config.exe`
- `agv-config-web.exe`
- 可选 `agv-no-car-feedback-simulator.exe`
- `config/`
- `docs/`
- `web/`
- 启停脚本
- `logs/`

打包发布目录：

```powershell
powershell -ExecutionPolicy Bypass -File .\package_release.ps1
```

## 当前这版和 Linux 版的差异

这套 Windows 目录已经做了这些调整：

- 去掉 Linux 启动脚本
- 改成 Windows `.bat` / PowerShell
- 默认配置改成相对路径
- 不再依赖 `/home/shx/...`
- 增加了服务安装脚本

## 这版还没有做的事情

这套已经进入 **Windows 可打包版**，但还不是最终产品包。还没完成：

- 正式日志轮转
- 安装包生成
- 自动检查 Python/NSSM 是否存在
- 真实小车现场联调验证

## 推荐下一步

如果目标是继续推进，我建议按下面顺序做：

1. 先把这套目录在 Windows 上跑通
2. 再在真实 Windows 机上执行一次 `build_windows_exe.ps1`
3. 再决定是否加网页改配置
4. 最后再做正式安装包
