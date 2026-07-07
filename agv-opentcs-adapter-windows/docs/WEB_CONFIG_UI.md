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
- 加载模板
- 保存配置
- 查看 adapter 状态
- 查看 no-car simulator 状态
- 查看 web 服务自身状态

## 注意

- 当前没有登录鉴权
- 不建议直接暴露到公网
- 如果要远程使用，建议放在受控 VPN 或内网后面
