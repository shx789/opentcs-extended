# Windows Deployment Notes

## 场景 A：真实小车

部署位置：

- 现场 Windows 边缘机

网络要求：

- 能访问小车 MQTT，例如 `192.168.254.100:1883`
- 能访问 openTCS HTTP
- 能访问 RCS HTTP

启动组件：

- `agv_native_feedback_adapter.py`

不启动：

- `agv_no_car_feedback_simulator.py`

## 场景 B：无车演示

部署位置：

- 任意 Windows 机器

启动组件：

- `agv_native_feedback_adapter.py`
- `agv_no_car_feedback_simulator.py`

用途：

- 做演示
- 做接口联调
- 做培训

## 建议的发布目录

- `bin/`
- `config/`
- `logs/`
- `tools/`
- `README.md`
- `requirements.txt`

## 后续建议

- 增加 `build_windows_exe.ps1`
- 增加 `tools/nssm.exe` 说明
- 增加配置校验脚本
- 增加简单 Web 配置页
