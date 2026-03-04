# AI Smart Cockpit System Based on RK3588S
# 基于 RK3588S 的 AI 智能车载座舱系统

[![Platform](https://img.shields.io/badge/Platform-RK3588S%20%28鲁班猫4%29-blue)](https://www.ebf-electronics.com)
[![OS](https://img.shields.io/badge/OS-Linux%206.1-green)](https://www.kernel.org)
[![Qt](https://img.shields.io/badge/Qt-5.15-brightgreen)](https://www.qt.io)
[![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

---

## 项目简介

本项目是一套运行在 **鲁班猫4 (RK3588S)** 嵌入式开发板上的 AI 智能车载座舱系统。系统以 Qt 为 GUI 框架，集成了 FMCW 毫米波雷达、DHT11 温湿度传感器、百度语音识别、百度地图、天气预报、音乐播放、倒车监控等功能，并正在向端侧大模型语音交互（DeepSeek + Whisper.cpp）演进。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         GUI 层 (Qt 5.15)                         │
│   雷达PPI极坐标图 │ 摄像头预览 │ 时钟/天气/温湿度/地图/音乐     │
│   语音识别界面   │ 倒车监控   │ 系统状态监测 │ AI 语音交互      │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                   中间件 / AI 层                                  │
│   radar_processor（FFT/CFAR雷达信号处理）                        │
│   ai_voice（Whisper.cpp ASR + DeepSeek-1.5B LLM + espeak TTS）  │
│   百度 ASR / 百度地图 API / 天气 API                             │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                      驱动层 (Linux Kernel)                       │
│   fmcw_radar（SPI+DMA）│ dht（GPIO软件模拟）│ STM32串口协议     │
│   V4L2摄像头驱动（TODO）│ DRM/KMS显示驱动（TODO）               │
└─────────────────────────┬───────────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────────┐
│                      硬件层 (RK3588S)                            │
│   FMCW毫米波雷达(SPI) │ DHT11温湿度 │ STM32协处理器             │
│   IMX415 MIPI摄像头   │ MIPI DSI屏幕│ MPU6050六轴(TODO)         │
│   千兆网络 PHY(TODO)  │ RK3588S NPU │                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 已完成功能

- ✅ **FMCW 毫米波雷达 Linux 驱动**（SPI+DMA 传输，帧校验）
- ✅ **STM32 → RK3588S 数据协议**（自定义帧格式 + 校验）
- ✅ **Qt 雷达极坐标 PPI 实时扫描图**（距离/方位可视化）
- ✅ **Qt GUI 移植**（从 IMX6ULL 项目完整移植到 RK3588S）
- ✅ **DHT11 温湿度**（内核驱动 + 软件模拟 fallback）
- ✅ **百度语音识别 ASR**（在线识别，网络请求）
- ✅ **百度地图**（WebEngine 嵌入）
- ✅ **音乐播放**（本地 MP3）
- ✅ **天气预报**（实时天气 API）
- ✅ **时钟**（自定义模拟时钟控件）
- ✅ **倒车监控 Monitor**（摄像头 V4L2 帧预览）
- ✅ **雷达信号处理中间件**（FFT + CFAR 目标检测）

## TODO 功能

- 🔲 **DeepSeek 端侧离线大模型语音交互**（Whisper.cpp ASR + DeepSeek-1.5B LLM + espeak-ng TTS）
- 🔲 **IMX415 MIPI 摄像头 V4L2 驱动接入**
- 🔲 **DRM/KMS 显示驱动**（MIPI DSI 屏幕）
- 🔲 **雷达 + 摄像头融合标注**
- 🔲 **eBPF 系统性能监测**
- 🔲 **MPU6050 六轴 I2C 驱动**
- 🔲 **千兆网络 PHY 驱动**

---

## 仓库目录结构

```
chezai/
├── VehicleTerminal/          # Qt 主应用（三栏布局：雷达PPI | 摄像头 | 系统状态）
│   ├── Map/                  # 百度地图模块
│   ├── Music/                # 音乐播放模块
│   ├── Weather/              # 天气预报模块
│   ├── Monitor/              # 倒车监控模块
│   ├── Radar/                # 雷达 PPI 极坐标图控件
│   ├── mainwindow.h/cpp      # 主窗口（三栏布局核心）
│   ├── dht11.h/cpp           # DHT11 温湿度传感器
│   ├── clock.h/cpp           # 模拟时钟控件
│   ├── speechrecognition.*   # 百度语音识别（在线）
│   └── VehicleTerminal.pro   # Qt 项目文件
├── qt_app/                   # 独立雷达 Qt 测试应用
│   ├── radar_reader.*        # 雷达数据读取线程
│   ├── radar_widget.*        # 雷达极坐标 PPI 控件
│   └── main.cpp
├── driver/                   # Linux 内核驱动
│   ├── fmcw_radar/           # FMCW 毫米波雷达驱动（SPI+DMA）
│   ├── dht/                  # DHT11 GPIO 驱动
│   ├── spi/                  # SPI 测试工具
│   └── stm32/                # STM32 通信协议说明
├── middleware/               # 中间件
│   ├── radar_processor.h/cpp # 雷达信号处理（FFT/CFAR）
│   └── test_processor.cpp    # 处理器单元测试
├── ai_voice/                 # AI 离线语音交互模块（DeepSeek + Whisper）
│   ├── include/              # 头文件
│   ├── src/                  # 实现文件
│   ├── CMakeLists.txt
│   └── README.md
├── docs/                     # 驱动学习笔记
│   ├── V4L2_驱动学习笔记.md
│   └── DRM_驱动学习笔记.md
├── test_dht11.c              # DHT11 独立测试程序
└── README.md
```

---

## 技术栈

| 层次 | 技术 |
|------|------|
| GUI 框架 | Qt 5.15（QWidget + QWebEngineView） |
| 操作系统 | Linux 6.1（Rockchip BSP） |
| 语言 | C++17 / C（内核驱动） |
| AI 推理 | llama.cpp（DeepSeek-R1-1.5B GGUF，TODO） |
| 语音识别 | whisper.cpp（离线 ASR，TODO） / 百度 ASR（在线） |
| TTS | espeak-ng（离线，TODO） |
| 雷达信号 | FFTW3 / 自实现 FFT + CFAR |
| 构建工具 | qmake（Qt项目）/ CMake（AI模块）/ Kbuild（内核驱动） |

---

## 硬件平台

| 组件 | 型号 |
|------|------|
| 主板 | 鲁班猫4（LubanCat4） |
| SoC | Rockchip RK3588S（4×A76 + 4×A55，6 TOPS NPU） |
| 内存 | 8GB LPDDR4x |
| 存储 | 64GB eMMC |
| 雷达 | FMCW 毫米波雷达（SPI 接口） |
| 协处理器 | STM32（串口通信） |
| 传感器 | DHT11 温湿度（GPIO） |
| 摄像头 | IMX415 MIPI CSI（接入中） |
| 显示 | MIPI DSI 屏幕 |

---

## 构建与运行

### 构建 Qt 主应用（VehicleTerminal）

```bash
cd VehicleTerminal
qmake VehicleTerminal.pro
make -j$(nproc)
./VehicleTerminal
```

### 构建雷达测试应用（qt_app）

```bash
cd qt_app
qmake qt_app.pro
make -j$(nproc)
./radar_app
```

### 编译内核驱动

```bash
cd driver/fmcw_radar
# 需要配置 KDIR 为内核源码路径
make KDIR=/path/to/kernel ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
# 加载驱动
sudo insmod fmcw_radar.ko
```

### 构建雷达信号处理中间件

```bash
cd middleware
g++ -O2 -std=c++17 test_processor.cpp radar_processor.cpp -o test_processor
./test_processor
```

### 构建 AI 语音模块

详见 [ai_voice/README.md](ai_voice/README.md)。

---

## 开发路线图（Roadmap）

```
v0.1  [已完成] Qt GUI 移植 + DHT11 + 时钟 + 地图 + 音乐 + 天气
v0.2  [已完成] FMCW 雷达驱动 + PPI 极坐标图 + 语音识别 + 倒车监控
v0.3  [进行中] IMX415 摄像头 V4L2 接入 + DRM/KMS 显示驱动
v0.4  [计划中] DeepSeek 端侧离线大模型 + 离线 ASR/TTS
v0.5  [计划中] 雷达+摄像头融合 + MPU6050 + eBPF 性能监测
v1.0  [目标]   完整 AI 智能车载座舱系统，具备完全离线语音交互能力
```

---

## 参考资料

- [RK3588S 数据手册](https://www.rock-chips.com/a/cn/product/RK35xilie/2022/0926/1681.html)
- [鲁班猫4 官方文档](https://doc.embedfire.com/linux/rk356x/quick_start/zh/latest/)
- [Qt 5.15 文档](https://doc.qt.io/qt-5/)
- [whisper.cpp](https://github.com/ggerganov/whisper.cpp)
- [llama.cpp](https://github.com/ggerganov/llama.cpp)
- [espeak-ng](https://github.com/espeak-ng/espeak-ng)
