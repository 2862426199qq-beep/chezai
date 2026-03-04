# VehicleTerminal 蓝牙音频（A2DP Sink）集成详细报告

## 1. 目标

在 LubanCat-4（RK3588 + ES8388）上实现：
- 手机通过蓝牙连接开发板；
- 手机音乐通过开发板 ES8388 播放；
- 功能集成进 `VehicleTerminal` Qt 应用；
- 提供可控的服务层 + Qt 封装层。

---

## 2. 架构实现

### 2.1 底层服务层（独立进程）

文件：`VehicleTerminal/bt_audio_service.py`

职责：
1. BlueZ 管理
   - `start`：打开 `power/agent/default-agent/pairable/discoverable/scan`
   - `stop`：关闭 `pairable/discoverable/scan`
   - 轮询连接设备（`bluetoothctl devices Connected`）
2. PulseAudio 路由
   - 将蓝牙 card profile 调整为 A2DP sink
   - 默认 sink 指向 ES8388（关键字匹配 + fallback）
   - 迁移当前 sink-input 到目标 sink
3. 对外接口（Unix Socket）
   - `start`
   - `stop`
   - `getStatus`
4. 事件通知
   - `statusChanged`
   - `serviceError`

协议：每行一条 JSON。

---

### 2.2 Qt 封装层

文件：
- `VehicleTerminal/BluetoothAudio.h`
- `VehicleTerminal/BluetoothAudio.cpp`

对外接口：
- `void start();`
- `void stop();`
- `QString getStatus();`

信号：
- `void deviceConnected(QString deviceName);`
- `void deviceDisconnected();`

实现要点：
- 使用 `QLocalSocket` 连接 `/tmp/bt_audio_service.sock`。
- 优先连接“已存在服务”；若不存在则自动拉起 `bt_audio_service.py`。
- 自动解析服务事件，转发为 Qt 信号。

---

### 2.3 主应用集成层（VehicleTerminal）

变更文件：
- `VehicleTerminal/mainwindow.h`
- `VehicleTerminal/mainwindow.cpp`
- `VehicleTerminal/VehicleTerminal.pro`

集成点：
1. 新增 UI 控件
   - 导航按钮：`BT Audio`
   - 系统状态标签：`BT Audio: ...`
2. 生命周期
   - 应用启动：`btAudio->start()`
   - 应用退出：`btAudio->stop()`
3. 交互逻辑
   - 点击 `BT Audio`：手动开/关服务
   - 每 3 秒 `getStatus()` 刷新一次状态
   - 收到 `deviceConnected/deviceDisconnected` 实时更新 UI
4. 工程接入
   - `VehicleTerminal.pro` 新增 `BluetoothAudio.cpp/.h`
   - `DISTFILES` 新增 `bt_audio_service.py`

---

## 3. 编译与验证记录

### 3.1 语法检查

```bash
python3 -m py_compile /home/cat/chezai/VehicleTerminal/bt_audio_service.py
```

结果：通过。

### 3.2 Qt 编译

```bash
cd /home/cat/chezai/VehicleTerminal
make -j4
```

结果：通过（含 `BluetoothAudio` 相关 `moc` 与链接）。

---

## 4. 运行测试建议（板端）

### 4.1 依赖

```bash
sudo apt update
sudo apt install -y bluez pulseaudio pulseaudio-module-bluetooth pavucontrol
```

### 4.2 启动应用

```bash
cd /home/cat/chezai/VehicleTerminal
./VehicleTerminal
```

### 4.3 联调步骤

1. 手机搜索并连接开发板蓝牙。
2. 手机播放音乐。
3. 检查：

```bash
pactl list short cards
pactl list short sinks
pactl list short sink-inputs
```

4. Qt 界面观察：右侧状态显示 `BT Audio: 已连接 <设备名>`。

---

## 5. 常见问题

1. 蓝牙可见但不能连
   - 检查 `bluetooth.service` 是否正常；
   - 查看 `bluetoothctl list` 是否识别控制器。

2. 已连接但无声
   - 检查 profile 是否为 A2DP；
   - 检查默认 sink 是否落在 ES8388。

3. 状态一直“未连接”
   - 确认服务 socket：`ls -l /tmp/bt_audio_service.sock`；
   - 确认服务进程：`ps -ef | grep bt_audio_service.py`。

---

## 6. 后续建议

1. 将 `bt_audio_service.py` 配置为 `systemd` 开机自启；
2. 将 `BT Audio` 开关与连接状态放入设置页，支持手动重连；
3. 若后续要提升实时性和稳定性，可升级为 C++ + DBus 的常驻 daemon。
