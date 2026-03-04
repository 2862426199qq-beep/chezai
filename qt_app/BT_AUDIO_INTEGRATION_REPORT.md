# LubanCat-4 (RK3588 + ES8388) 蓝牙音乐播放（A2DP Sink）集成报告

## 1. 实现目标与结果

已按“三层架构”完成可运行实现：

1. **底层服务层（独立进程）**
   - 新增：`bt_audio_service.py`
   - 能力：
     - BlueZ 管理（可发现 / 可配对 / 扫描开关）
     - A2DP Sink 方向处理（通过 PulseAudio 蓝牙卡 profile 与路由）
     - 音频输出路由到 ES8388（通过 `pactl` 选择 sink 并迁移流）
     - 对外 Socket API（Unix Domain Socket）
       - `start()`
       - `stop()`
       - `getStatus()`
     - 状态变化通知（`statusChanged` 事件）

2. **Qt 接口封装层**
   - 新增：`BluetoothAudio.h` / `BluetoothAudio.cpp`
   - 提供接口与信号：
     - `start()`
     - `stop()`
     - `QString getStatus()`
     - `deviceConnected(QString deviceName)`
     - `deviceDisconnected()`
   - 与服务通信方式：`QLocalSocket + JSON Line` 协议。

3. **Qt 应用集成层**
   - `main.cpp` 已创建 `BluetoothAudio` 实例并在应用启动时 `start()`，退出时 `stop()`。
   - `qt_app.pro` 已加入 `BluetoothAudio` 源文件，并开启 `network` 模块。

---

## 2. 代码改动清单

- 新增：`qt_app/bt_audio_service.py`
- 新增：`qt_app/BluetoothAudio.h`
- 新增：`qt_app/BluetoothAudio.cpp`
- 修改：`qt_app/qt_app.pro`
- 修改：`qt_app/main.cpp`

---

## 3. 服务接口协议说明（Socket）

- Socket 路径（默认）：`/tmp/bt_audio_service.sock`
- 传输格式：**每行一条 JSON**（UTF-8）

### 3.1 请求

```json
{"method":"start"}
{"method":"stop"}
{"method":"getStatus"}
```

### 3.2 典型响应

```json
{"ok": true, "result": "started"}
{"ok": true, "result": "stopped"}
{"ok": true, "status": {"state":"connected","deviceName":"iPhone","deviceMac":"AA:BB:CC:DD:EE:FF"}}
```

### 3.3 事件通知

```json
{"event":"statusChanged","payload":{"state":"connected","deviceName":"iPhone","deviceMac":"AA:BB:CC:DD:EE:FF"}}
{"event":"statusChanged","payload":{"state":"disconnected","deviceName":"","deviceMac":""}}
```

---

## 4. 依赖与系统准备

在 LubanCat-4 Ubuntu 上建议安装：

```bash
sudo apt update
sudo apt install -y bluez pulseaudio pulseaudio-module-bluetooth pavucontrol python3
```

检查服务：

```bash
systemctl status bluetooth
pulseaudio --check || pulseaudio --start
```

确认 ES8388 sink（名称可能随驱动/声卡不同而变化）：

```bash
pactl list short sinks
```

若 sink 名不包含 `es8388`，启动服务时可指定关键字：

```bash
python3 bt_audio_service.py --sink-keyword rockchip
```

---

## 5. 编译步骤（Qt 侧）

在仓库根目录执行：

```bash
cd qt_app
qmake qt_app.pro
make -j4
```

产物：`qt_app/radar_app`

---

## 6. 单独测试底层服务（推荐先测）

### 6.1 启动服务

```bash
cd qt_app
python3 bt_audio_service.py --socket /tmp/bt_audio_service.sock
```

### 6.2 用 Python 客户端发命令（示例）

```bash
python3 - <<'PY'
import json, socket, time
c=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
c.connect('/tmp/bt_audio_service.sock')
print(c.recv(4096).decode().strip())
for m in ['getStatus','start']:
    c.sendall((json.dumps({'method':m})+'\n').encode())
    time.sleep(0.2)
    print(c.recv(4096).decode().strip())
PY
```

---

## 7. 端到端联调步骤（手机 -> 蓝牙 -> 开发板 -> ES8388）

1. 启动 Qt 应用：
   ```bash
   cd qt_app
   ./radar_app
   ```
2. 应用已自动调用 `BluetoothAudio::start()`，开发板进入可发现/可配对状态。
3. 手机蓝牙搜索并连接开发板（首次配对会保存）。
4. 手机播放音乐。
5. 开发板检查：
   ```bash
   pactl list short sink-inputs
   pactl list short sinks
   ```
6. 若无声，打开 `pavucontrol` 检查播放流是否已路由到 ES8388 对应输出。

---

## 8. 验证结果（本次实现已完成）

已在当前环境完成：

1. `bt_audio_service.py` 语法校验通过：
   - `python3 -m py_compile qt_app/bt_audio_service.py`
2. `qt_app` 编译通过：
   - `qmake + make`
3. Socket API 联通测试通过：
   - `ping/getStatus/start/stop` 均返回成功 JSON。

> 注：真实手机 A2DP 播放效果依赖现场蓝牙适配器、BlueZ/PulseAudio 配置和 ES8388 驱动状态，需要在板端实机按第 7 节验证。

---

## 9. 常见问题与排查

1. **`bluetoothctl` 卡住或超时**
   - 先检查 `bluetooth` 服务状态；
   - 检查是否有可用蓝牙控制器：`bluetoothctl list`。

2. **已连接但没声音**
   - 检查蓝牙卡 profile 是否为 A2DP：
     ```bash
     pactl list cards short
     ```
   - 检查默认 sink 是否为 ES8388；必要时手动：
     ```bash
     pactl set-default-sink <es8388_sink_name>
     ```

3. **Qt 侧无法连接服务**
   - 确认 Socket 文件存在：`ls -l /tmp/bt_audio_service.sock`
   - 确认服务进程存活：`ps -ef | grep bt_audio_service.py`

---

## 10. 建议的量产化下一步

1. 将 `bt_audio_service.py` 配置为 `systemd` 常驻服务（开机自启）。
2. Qt 端改为“状态展示 + 手动开关”UI 控件（按钮/状态标签）。
3. 若后续你希望更强稳定性，可把底层服务改为 C++ daemon + 原生 D-Bus API（替代 `bluetoothctl` 子进程方式）。

---

## 11. 简易 systemd 示例（可选）

`/etc/systemd/system/bt-audio.service`：

```ini
[Unit]
Description=Bluetooth A2DP Sink Audio Service
After=bluetooth.service sound.target

[Service]
Type=simple
User=cat
WorkingDirectory=/home/cat/chezai/qt_app
ExecStart=/usr/bin/python3 /home/cat/chezai/qt_app/bt_audio_service.py --socket /tmp/bt_audio_service.sock --sink-keyword es8388
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

启用：

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now bt-audio.service
sudo systemctl status bt-audio.service
```
