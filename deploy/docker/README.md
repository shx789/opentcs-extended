# openTCS Docker 部署

本地编译 Docker 镜像 → 上传服务器 → 启动容器。

## 架构

| 容器 | 镜像 | 端口 | 说明 |
|------|------|------|------|
| MQTT | `eclipse-mosquitto:2` | 1883 | AGV MQTT Broker |
| Kernel | `opentcs-kernel:1.0` | 55200 | openTCS 调度内核 |
| RCS | `opentcs-rcs:1.0` | 8090 | WCS/RCS 集成样例 |

## 前置条件

- 本地：Docker Desktop（含 buildx）、JDK 21、Gradle Wrapper
- 服务器：Docker、bash、`curl`
- 本地构建目标平台：`linux/amd64`（默认）

---

## 1. 本地构建镜像

### Windows

```powershell
cd <仓库根目录>

# 推荐：宿主机 Gradle 编译 + 打包（最稳，避免容器内下载 Gradle）
.\deploy\docker\scripts\build-images.ps1 -HostGradle

# 或：完全在 Docker 内编译（需良好网络）
# .\deploy\docker\scripts\build-images.ps1
```

### Linux / macOS

```bash
cd <仓库根目录>
chmod +x deploy/docker/scripts/*.sh

./deploy/docker/scripts/build-images.sh --host-gradle
# 或：./deploy/docker/scripts/build-images.sh
```

可选环境变量：`IMAGE_TAG`（默认 `1.0`）、`DOCKER_PLATFORM`（默认 `linux/amd64`）。

构建完成后确认：

```bash
docker images | grep -E 'opentcs|mosquitto'
```

---

## 2. 导出镜像包

### Windows

```powershell
.\deploy\docker\scripts\export-images.ps1
```

### Linux / macOS

```bash
./deploy/docker/scripts/export-images.sh
```

生成 `deploy/docker/opentcs-images.tar`（含 kernel、rcs、mosquitto 三个镜像）。体积较大时可压缩：

```bash
gzip deploy/docker/opentcs-images.tar
```

---

## 3. 上传到服务器

```bash
scp deploy/docker/opentcs-images.tar user@server:/opt/opentcs/
scp -r deploy/docker user@server:/opt/opentcs/
```

若使用了 `.tar.gz`：

```bash
scp deploy/docker/opentcs-images.tar.gz user@server:/opt/opentcs/
# 服务器上：gzip -d opentcs-images.tar.gz
```

---

## 4. 服务器启动

```bash
ssh user@server
cd /opt/opentcs/deploy/docker/scripts

chmod +x *.sh
sed -i 's/\r$//' *.sh ../.env.example 2>/dev/null

# 推荐：直接启动（docker run，适配云服务器）
bash start-containers.sh

# 或：导入镜像并启动
# bash server-up.sh ../opentcs-images.tar
```

若 `server-up.sh` 输出 `Using: docker-compose`，说明脚本版本过旧，请重新上传 `deploy/docker/scripts/` 后改用 `bash start-containers.sh`。

首次启动会自动从 `.env.example` 创建 `.env`，可按需修改 `RCS_AGV_POINT_ID_MAP` 等。

---

## 5. 验证

```bash
curl http://localhost:55200/v1/transportOrders
curl http://localhost:8090/api/v1/wcs/agv/missions
```

浏览器：

- http://\<服务器IP\>:8090/demo/wcs
- http://\<服务器IP\>:8090/demo/agv-monitor

---

## 6. 升级

本地重新构建并导出后，在服务器执行：

```bash
docker load -i /opt/opentcs/opentcs-images.tar
cd /opt/opentcs/deploy/docker/scripts
bash start-containers.sh
```

---

## 说明

- Plant model 需通过 Kernel HTTP API `PUT /v1/plantModel` 导入。
- ModelEditor / OperationsDesk 在开发机使用，不必进容器。
- `RCS_STORE_MODE=file` 适合联调，不建议作为生产数据库。

---

## FAQ

### 脚本 `$'\r': command not found`

Windows 上传的脚本或 `.env` 含 CRLF 换行。在服务器执行：

```bash
sed -i 's/\r$//' *.sh ../.env ../.env.example
bash start-containers.sh
```

请用 `bash` 运行脚本，不要用 `sh`。

### 容器名冲突（`container name already in use`）

旧容器未删除导致。先清理再启动：

```bash
docker rm -f opentcs-mqtt opentcs-kernel opentcs-rcs
bash start-containers.sh
```

新版 `server-up.sh` / `start-containers.sh` 会自动清理同名容器。

### `rcs-integration` 报错 / Kernel `unhealthy`

多见于旧版 `server-up.sh` 走 `docker-compose`，且 Kernel 未通过健康检查（缺 `seccomp`、keystore 或镜像内无 `curl`）。

**处理：不要用 compose，改用 `start-containers.sh`：**

```bash
docker rm -f opentcs-mqtt opentcs-kernel opentcs-rcs
docker-compose -f ../docker-compose.yml down 2>/dev/null || true

cd /opt/opentcs/deploy/docker/scripts
bash start-containers.sh
```

排查 Kernel：

```bash
docker logs opentcs-kernel --tail 50
curl http://localhost:55200/v1/transportOrders
```

Kernel 正常后再看 RCS：`docker logs opentcs-rcs --tail 30`。

### `docker compose up -d` 报错 `unknown shorthand flag: 'd'`

服务器 Docker 版本较旧、无 compose 插件。使用 `bash start-containers.sh`（内部用 `docker run`），或安装 `docker-compose` / 升级 Docker。

### 本地构建：基础镜像拉取失败（`eclipse-temurin:*-noble: not found`）

镜像加速器可能未同步 `noble` 标签。本项目默认使用 `jammy`；仍失败时可指定：

```powershell
$env:JDK_IMAGE = "eclipse-temurin:21-jdk"
$env:JRE_IMAGE = "eclipse-temurin:21-jre"
.\deploy\docker\scripts\build-images.ps1 -HostGradle
```

或在 Docker Desktop 中切换/关闭镜像加速器。

### 本地构建：Gradle 下载超时

推荐使用宿主机编译：

```powershell
.\deploy\docker\scripts\build-images.ps1 -HostGradle
```

若坚持容器内编译，可换 Gradle 镜像源：

```powershell
$env:GRADLE_DISTRIBUTION_URL = "https://repo.huaweicloud.com/gradle/gradle-8.14.4-bin.zip"
.\deploy\docker\scripts\build-images.ps1
```

### 端口 1883 已被占用

```bash
ss -tlnp | grep 1883

# 宿主机已有 MQTT：跳过 mqtt 容器
SKIP_MQTT=true bash start-containers.sh

# 或换映射端口（AGV 需连新端口）
MQTT_HOST_PORT=1884 bash start-containers.sh
```

`SKIP_MQTT=true` 时 RCS 通过 `172.17.0.1:1883` 访问宿主机 Broker，可用 `EXTERNAL_MQTT_HOST` 调整。

### Kernel 容器 Restarting

先看日志：

```bash
docker logs opentcs-kernel --tail 100
```

**`Could not find keytool in PATH`**：镜像内未预生成 keystore。重新本地构建（`-HostGradle` 会在镜像内生成证书），或启动时指定 PATH：

```bash
docker rm -f opentcs-kernel
docker run -d --name opentcs-kernel --network opentcs-net \
  --security-opt seccomp=unconfined \
  --ulimit nproc=65535:65535 \
  -p 55200:55200 \
  -v opentcs-kernel-data:/opt/opentcs-kernel/data \
  -e JAVA_TOOL_OPTIONS=-Xmx512m \
  --restart unless-stopped \
  --entrypoint sh opentcs-kernel:1.0 \
  -c 'export PATH=/opt/java/openjdk/bin:$PATH; cd /opt/opentcs-kernel; [ -f config/keystore.p12 ] || ./generateKeystores.sh; exec ./startKernel.sh'
```

**`pthread_create failed (EPERM)`**：云服务器 Docker 安全策略限制。新版 `start-containers.sh` 默认加 `--security-opt seccomp=unconfined`；手动启动时也需加上该参数。RCS 容器同样建议加上。

Kernel 正常后：`docker restart opentcs-rcs`。

### `lib/env.sh: No such file or directory`

上传完整 `deploy/docker/scripts/` 目录，或使用最新版 `start-containers.sh`（已内联 env 处理，不依赖 `lib/`）。

### 如何只重启 / 停止

```bash
docker restart opentcs-kernel opentcs-rcs opentcs-mqtt
docker stop opentcs-kernel opentcs-rcs opentcs-mqtt
docker rm -f opentcs-kernel opentcs-rcs opentcs-mqtt
```

### 环境变量参考

见 [`.env.example`](.env.example)：`IMAGE_TAG`、`MQTT_HOST_PORT`、`SKIP_MQTT`、`RCS_AGV_POINT_ID_MAP` 等。
