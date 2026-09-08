# AGV openTCS Linux 部署包说明

这个目录是 Linux 部署模板，独立放在 `opentcs/deploy-linux/` 下，不影响现有 Windows 包、`.bat`、`.ps1` 和本地开发文件。

## 1. 这个部署包包含什么

目录结构：

```text
deploy-linux/
  README_LINUX_DEPLOY.md
  config/
    agv-suite.env.example
    runtime_config.example.json
  scripts/
    common.sh
    check-env.sh
    install-python-env.sh
    start-kernel.sh
    start-rcs.sh
    start-config-web.sh
    start-dashboard.sh
    start-all.sh
    stop-all.sh
  systemd/
    opentcs-kernel.service
    opentcs-rcs.service
    agv-config-web.service
    agv-dashboard.service
  logs/
  runtime/
```

用途：

- `config/agv-suite.env.example`：Linux 现场部署的总配置模板。
- `config/runtime_config.example.json`：adapter / 8091 页面使用的运行配置模板。
- `scripts/*.sh`：手动启动、停止、检查环境。
- `systemd/*.service`：服务器长期运行时使用的 systemd 服务模板。
- `logs/`：手动脚本默认日志目录。
- `runtime/`：预留运行时文件目录。

## 2. Linux 服务器需要准备什么

建议环境：

- Linux x86_64 服务器或工控机。
- Java 17 或兼容 openTCS 当前版本的 JDK。
- Python 3.10+。
- `curl`、`nc`、`unzip`、`rsync`。
- 服务器和小车在同一个内网，能够访问小车 MQTT `1883` 端口。

如果用 systemd，建议创建专用用户：

```bash
sudo useradd -r -m -d /opt/opentcs-agv opentcs
sudo mkdir -p /opt/opentcs-agv
sudo chown -R opentcs:opentcs /opt/opentcs-agv
```

## 3. 推荐部署目录

推荐把项目放到：

```text
/opt/opentcs-agv/
  opentcs/                  # 当前 opentcs 仓库或打包后的源码/产物
  deploy-linux/             # 本目录，可以复制过去
  runtime/opentcs/          # openTCS Linux 运行包，可选
  logs/
  venv/
```

如果你直接把整个仓库复制到服务器，可以这样：

```bash
sudo mkdir -p /opt/opentcs-agv
sudo rsync -a ./opentcs/ /opt/opentcs-agv/opentcs/
sudo cp -a /opt/opentcs-agv/opentcs/deploy-linux /opt/opentcs-agv/deploy-linux
sudo chown -R opentcs:opentcs /opt/opentcs-agv
```

## 4. 修改配置

复制配置模板：

```bash
cd /opt/opentcs-agv/deploy-linux
cp config/agv-suite.env.example config/agv-suite.env
```

重点修改：

```bash
AGV_MQTT_HOST=小车或现场MQTT服务器IP
AGV_MQTT_PORT=1883
AGV_MQTT_URI=tcp://小车或现场MQTT服务器IP:1883
AGV_MQTT_URI_WEB=mqtt://小车或现场MQTT服务器IP:1883/

AGV_COMMAND_TOPIC=robot_control
AGV_FEEDBACK_TOPIC=task_feedback

OPENTCS_HTTP_URL=http://127.0.0.1:55200
RCS_HTTP_PORT=8090
RCS_HTTP_URL=http://127.0.0.1:8090

CONFIG_WEB_PORT=8091
DASHBOARD_PORT=8092
```

现场小车一般怎么填：

- 服务器连接小车 WiFi 或小车所在内网。
- 小车如果自己提供 MQTT Broker，`AGV_MQTT_HOST` 填小车 IP。
- 如果现场有单独 MQTT Broker，`AGV_MQTT_HOST` 填 Broker IP。
- 端口通常是 `1883`，除非现场协议文档另有说明。

点位映射示例：

```bash
AGV_POINT_ID_MAP=nav:0=ST_IN_01,point:0=ST_IN_01,nav:1=P_WAIT_IN_01,point:1=P_WAIT_IN_01
```

含义：

- 小车反馈 `{ "type": "nav", "id": 0 }` 时，映射到 openTCS 点位 `ST_IN_01`。
- 小车反馈 `{ "type": "point", "id": 1 }` 时，映射到 openTCS 点位 `P_WAIT_IN_01`。
- 现场不确定小车反馈 type 是 `nav` 还是 `point` 时，两个都配。

adapter 的 JSON 配置也要准备：

```bash
cp /opt/opentcs-agv/deploy-linux/config/runtime_config.example.json \
   /opt/opentcs-agv/opentcs/agv-opentcs-adapter-windows/config/runtime_config.json
```

然后可以通过 `http://服务器IP:8091/` 页面继续修改 MQTT、topic、openTCS、RCS、点位映射。

## 5. 安装 Python 依赖

```bash
cd /opt/opentcs-agv/deploy-linux
bash scripts/install-python-env.sh
```

它会创建：

```text
/opt/opentcs-agv/venv
```

并安装 adapter 的 `requirements.txt`。

## 6. 检查环境和网络

```bash
cd /opt/opentcs-agv/deploy-linux
bash scripts/check-env.sh
```

重点看：

```bash
nc -vz 小车IP 1883
```

如果不通，通常是：

- 服务器没连到小车 WiFi / 小车内网。
- 小车 IP 填错。
- 小车 MQTT 端口不是 `1883`。
- 小车防火墙或网络策略没放行。
- MQTT Broker 不在小车上，而是在另一台边缘服务器上。

## 7. 手动启动

```bash
cd /opt/opentcs-agv/deploy-linux
bash scripts/start-all.sh
```

启动内容：

1. openTCS Kernel。
2. RCS 服务，默认端口 `8090`。
3. 配置页面，默认端口 `8091`。
4. 联调看板，默认端口 `8092`。

访问：

```text
http://服务器IP:8091/
http://服务器IP:8092/
```

停止：

```bash
bash scripts/stop-all.sh
```

日志：

```text
/opt/opentcs-agv/logs/*.out.log
/opt/opentcs-agv/logs/*.err.log
```

## 8. systemd 部署

复制服务模板：

```bash
sudo cp /opt/opentcs-agv/deploy-linux/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
```

启动：

```bash
sudo systemctl start opentcs-kernel
sudo systemctl start opentcs-rcs
sudo systemctl start agv-config-web
sudo systemctl start agv-dashboard
```

设置开机启动：

```bash
sudo systemctl enable opentcs-kernel opentcs-rcs agv-config-web agv-dashboard
```

查看日志：

```bash
journalctl -u opentcs-kernel -f
journalctl -u opentcs-rcs -f
journalctl -u agv-config-web -f
journalctl -u agv-dashboard -f
```

## 9. 真实小车对接流程

整体流程：

1. openTCS 里有拓扑地图和一辆车，例如 `Vehicle-01`。
2. 8091 配置 MQTT 地址、topic、车辆名、点位映射。
3. RCS 接收 WCS/页面任务。
4. RCS 创建 openTCS 订单。
5. RCS 按任务向 MQTT `robot_control` 发布控制消息。
6. 小车执行后，向 MQTT `task_feedback` 发布反馈。
7. adapter 订阅 `task_feedback`，解析 `type/id/status/now_pose/goal_pose`。
8. adapter 根据 `point_id_map` 或坐标容差映射到 openTCS 点位。
9. adapter 更新 openTCS 车辆位置和状态。
10. openTCS/RCS 根据反馈推进订单状态，必要时给 WCS 回调。

协议文档里的反馈格式示例：

```json
{
  "cmd_type": "task_feedback",
  "dir": "",
  "goal_pose": {
    "x": 0.3,
    "y": -1.13,
    "yaw": 1.45
  },
  "id": 0,
  "now_pose": {
    "x": 0.29,
    "y": -1.141,
    "yaw": 1.439
  },
  "status": "success",
  "type": "nav"
}
```

这里关键字段是：

- `type`：`track` / `nav` / `charge` / `point`。
- `id`：轨迹 ID、巡航点 ID、回充点 ID 或目标点 ID。
- `status`：`start` / `process` / `try` / `success` / `failure` / `timeout` / `stop`。
- `now_pose`：真实小车当前位姿。
- `goal_pose`：任务目标位姿。

## 10. 地图生成、导出和加载

当前流程：

1. 在 8091 上传 `map.pgm` 和 `map.yaml`。
2. 生成 openTCS 候选拓扑。
3. 人工检查、删点、改名、补路径。
4. 保存为新版本。
5. 导出 `plant_model.xml`。
6. 在 openTCS Model Editor 或 Kernel 中加载这个模型。
7. 确认点位名称和 `point_id_map` 一致。

需要注意：

- PGM/YAML 给的是栅格地图和 ROS 坐标系信息，不等于 openTCS 业务点位。
- openTCS 拓扑点是调度用点位，需要人工确认哪些点是真正的入库点、出库点、等待点、充电点。
- 算法生成的点只能作为候选，现场仍需要人工校准。
- 如果改了 openTCS 模型点名，`point_id_map` 也要同步改。

## 11. 端口和防火墙

常用端口：

```text
55200  openTCS HTTP API，通常只给本机 RCS 访问
8090   RCS HTTP API
8091   配置页面
8092   联调看板
1883   小车或 MQTT Broker
```

如果页面要从办公电脑访问服务器，需要服务器防火墙放行：

```bash
sudo ufw allow 8090/tcp
sudo ufw allow 8091/tcp
sudo ufw allow 8092/tcp
```

如果 8092 页面无法从外部访问，先在服务器本机验证：

```bash
curl http://127.0.0.1:8092/
```

当前 8092 服务代码可能仍默认绑定本机地址，生产部署时可以用 SSH 隧道或 Nginx 反向代理暴露出来；如果要直接监听 `0.0.0.0`，需要后续改 dashboard server 的绑定地址。

## 12. 当前部署包的边界

这个目录先解决 Linux 部署结构和现场配置方式，不等于已经生成完整 Linux 二进制安装包。

还需要根据现场最终运行方式补齐：

- openTCS Linux 运行包或从源码构建的启动命令。
- RCS jar 是否已经构建好。
- 小车 MQTT Broker 的真实 IP、端口、topic。
- 现场点位 ID 和 openTCS 点名的最终映射。
- 是否使用 systemd 长期运行。

如果只是演示和联调，建议先用手动脚本启动；稳定后再切到 systemd。
