# Camera 开发笔记

> 平台：RK3588S LubanCat-4 | 传感器：imx415 (800万像素) | Qt 5.12.8
> 记录时间：2026-03-08

---

## 模块零：调试记录——"初次融合qt 运行不起来 OOM Killed 
### 0.1 `camera_thread.cpp` — 问题定位
Out of memory: Killed process camera_qt
anon-rss: 2844612kB  ← 用了将近 3GB
定位手段： dmesg | tail -20 看到 OOM Killer 日志，直接指向进程名和内存占用
### 0.2 `camera_thread.cpp` — 根因分析：setPixel 逐像素调用

**❌ 错误写法**（`QImage::setPixel`）：

```cpp
// setPixel 每次调用内部要做：
//   1. 检查坐标是否越界（条件判断）
//   2. 根据图像 Format 做分支判断
//   3. 写入像素值
// 1920×1080 = 207万像素 → 207万次上述开销 → 内存/CPU 双爆炸
img.setPixel(col, row, qRgb(r, g, b));
```

**✅ 正确写法**（`QImage::scanLine` 原始指针）：

```cpp
// scanLine(row)：只做一次指针偏移，返回第 row 行的起始内存地址
// 之后直接按字节写，没有任何额外开销
uint8_t *line = img.scanLine(row);  // 拿到这一行的原始指针
line[col * 3]     = r;              // 直接写内存，零额外开销
line[col * 3 + 1] = g;
line[col * 3 + 2] = b;
// 效果：内存占用从 3GB → 几十MB，CPU 省去 207万次函数调用
```

绕过 Qt 的逐像素封装，直接操作底层内存，性能差距数量级级别。

## 模块一：调试记录——"水印感"排查全过程

### 1.1 `camera_thread.cpp` — 现象

摄像头有画面，但看起来像蒙了一层透明薄膜（"水印感"），不清晰，人在动的时候有拖影。

### 1.2 `camera_thread.cpp` + `mainwindow.cpp` — 排查路径

| 步骤 | 假设 | 做了什么 | 结果 |
|---|---|---|---|
| 1 | 分辨率太低(640×480) | 改到 1920×1080 | ❌ 画质没变 |
| 2 | Qt 缩放算法差 | Fast→Smooth | ❌ 没区别 |
| 3 | stride 不对齐 | v4l2-ctl 查 bytesperline | ❌ stride=WIDTH，没问题 |
| 4 | ISP 色彩矩阵没生效 | 加饱和度×2、对比度×1.3 | ❌ 暗部更黑了但不清晰 |
| 5 | VNC 压缩 | scp PNG 到 PC 看 | ❌ PC 上也模糊 |
| 6 | **镜头失焦** | Laplacian 方差=94 | ✅ 确认！手动旋转镜头对焦后解决 |

### 1.3 Laplacian 方差 — 怎么量化"清不清晰"

```
原理：对图像做拉普拉斯卷积（每个像素 = 上+下+左+右 - 4×自己），
      然后算方差。边缘越锐利 → 差值越大 → 方差越高。

判定标准：
  < 100  → 严重失焦（我们测到 94）
  100-500 → 轻微模糊
  > 500  → 正常清晰
```

### 1.4 `camera_thread.cpp` + `mainwindow.cpp` — 每次修改及效果

**（1）分辨率：640×480 → 1920×1080 → 640×360 → 1280×720**

> ✅ **已验证（2026-03-08 对焦后重测，AE 对齐亮度差 2.9）**：分辨率越高，ISP 保留的细节越多，清晰度确实更高。
> 当时认为"分辨率没用"是因为**镜头处于失焦状态**，任何分辨率都糊，掩盖了分辨率的影响。
>
> 对焦后实测 Laplacian 方差（缩放到 640×480 同等条件对比）：
> - 640×480  → **232**（基准）
> - 1280×720 → **281**（+21%）
> - 1920×1080 → **386**（+67%）
>
> 但显示尺寸是 430×242，1920 传输和转换开销大，1280×720 是性价比最优选择。

```c
// camera_thread.h
#define WIDTH       1280    // 最终值（性价比最优：清晰度+帧率双平衡）
#define HEIGHT      720
```

为什么 1920×1080 没变清晰？因为 cameraLabel 只有约 430×280 像素大：
- 1920→430 缩小 4.5 倍 = 丢掉 80% 的像素信息
- 1280→430 缩小 3 倍 = 丢掉 67%，且帧率好很多

**（2）NV12→RGB 用整数替代浮点**
CPU 做浮点乘法比整数乘法慢很多，尤其嵌入式 ARM。
目标是计算 y + 1.402 × v，但 1.402 是小数，怎么用整数表达？
关键思路：把小数"放大"成整数，算完再"缩回去"
1.402 × v
= (1.402 × 256) × v ÷ 256
= 359 × v ÷ 256
= 359 * v >> 8        // ÷256 等价于右移8位
```c
// 慢版（浮点，ARM 上约 60ms/帧）：
line[idx] = qBound(0, (int)(y + 1.402f * v), 255);

// 快版（整数位移，约 15ms/帧）：
line[idx] = clamp8(y + (359 * v >> 8));
// 解释：1.402 × 256 = 358.9 ≈ 359
//        359 * v 得到放大 256 倍的结果
//        >> 8 就是 ÷256，等价于乘 1.402
//        全程只有整数乘法和位移，ARM NEON 极快
```

**（3）饱和度/对比度增强（后来删了）**

```c
// 加过又删了的代码：
const float SAT = 2.0f;       // UV ×2 → 颜色更鲜艳
const float Y_ALPHA = 1.3f;   // 亮度乘 1.3 → 加大明暗差
const int Y_BETA = -20;       // 整体压暗 → 暗部更黑

// 删除原因：根本问题是镜头失焦，不是色彩问题
// 教训：不要在软件层补偿硬件/光学问题
```

**（4）USM 锐化（后来也删了）**

```c
// Unsharp Mask 算法：
// sharp = original + strength × (original - blur)
// original - blur = 细节层（高频信息）
// strength × (细节层) = 放大细节
// original + 放大的细节 = 锐化结果
//original = blur（低频/轮廓） + detail（高频/边缘细节） 
// 用十字邻域（上下左右）当做 blur：均值滤波，得到模糊版本

// 删除原因：
// 镜头对焦后 Laplacian=200-300 就已经观感最佳
// 加锐化到 500-600 反而出现"锐光感"（边缘白色光晕）
// 教训：锐化是锦上添花，不是雪中送炭
```

**（5）msleep(33) 删除**

```c
// 之前：
msleep(33);  // 每帧后强制等 33ms

// 删除原因：
// DQBUF 本身是阻塞调用——硬件没填满下一帧时，它自己会等
// 加 msleep 等于"等了又等"，白白浪费 33ms 帧率
```

**（6）Fast vs Smooth Transformation**

```c
// 最终用 Fast：
Qt::FastTransformation   // 最近邻插值，快
Qt::SmoothTransformation // 双线性插值，慢 2-3 倍

// 选 Fast 的原因：
// 1280→430 是"缩小"，不是放大
// 缩小时两种算法视觉差异很小（缩小本身就在做平均）
// 放大时 Smooth 才明显更好
```

### 1.5 `camera_thread.cpp` — 最终结论

**根因是镜头物理失焦**，所有软件调整都是弯路。但这些弯路让我们搞清楚了每个环节的性能影响。

---

## 模块二：基础知识——光到像素的完整过程

### 2.1 光学成像原理

```
现实世界的光 → 镜头（透镜组） → 聚焦到传感器表面

关键概念：
┌─────────────────────────────────────────────────────┐
│  焦点（Focus）                                        │
│                                                       │
│  镜头把光折射汇聚到一个点。如果汇聚点正好落在           │
│  传感器表面 → 清晰。偏了 → 模糊（失焦）。               │
│                                                       │
│  物距（物体到镜头的距离）变化时，焦点位置也变。           │
│  所以需要"对焦"——调整镜头到传感器的距离。               │
│                                                       │
│  固定焦距模组：镜头可以旋转微调（就是我们做的操作）。     │
│  自动对焦模组：马达自动调，但 imx415 IRC 模组是手动的。   │
└─────────────────────────────────────────────────────┘
```

### 2.2 图像传感器（CMOS Sensor）

imx415 是索尼产的 **CMOS 图像传感器**：

```
传感器表面是一个 3864×2192 的像素阵列（约 800 万像素）。
每个像素是一个光电二极管：光子 → 电子 → 电压值。

        ┌──────────────────────────┐
        │  R  G  R  G  R  G  R  G │  ← Bayer 滤色阵列
        │  G  B  G  B  G  B  G  B │     每个像素只感应一种颜色
        │  R  G  R  G  R  G  R  G │     R=红，G=绿，B=蓝
        │  G  B  G  B  G  B  G  B │     绿色是红蓝的 2 倍（人眼对绿更敏感）
        └──────────────────────────┘

传感器输出的是 10-bit RAW 数据（每个像素 0~1023），
只有单通道（R 或 G 或 B），不是彩色图像。
```

### 2.3 `camera_thread.cpp` — ISP（Image Signal Processor）

RAW 数据不能直接看，需要 ISP 处理。RK3588 内置 RKISP v3.0 硬件：

```
RAW(10bit) → ISP 处理流水线 → NV12(8bit YUV) → 你的程序

ISP 做了什么（按顺序）：
┌────────────────────────────────────────────────┐
│ 1. 黑电平校正    去掉传感器暗电流噪声            │
│ 2. 坏点校正      屏蔽坏掉的像素                  │
│ 3. 去马赛克      Bayer → 每像素 RGB（插值）       │
│ 4. 白平衡(AWB)   让白色在任何光源下都是白的        │
│ 5. 曝光控制(AE)  自动调亮度（调传感器增益+快门）    │
│ 6. 色彩矩阵(CCM) 校准颜色准确性                   │
│ 7. 降噪(NR)      去掉随机噪点                     │
│ 8. 锐化(SHARP)   增强边缘清晰度                   │
│ 9. 色彩空间转换   RGB → YUV                       │
│ 10. 缩放          3864×2192 → 你要的 1280×720     │
└────────────────────────────────────────────────┘

rkaiq_3A_server 的作用：
  - 运行 3A 算法（AE 自动曝光 + AWB 自动白平衡 + AF 自动对焦）
  - 读取 /etc/iqfiles/ 下的 JSON 参数（IQ文件）配置 ISP 各模块
  - 我们遇到的 "invalid main scene len!" 就是 IQ 文件解析部分报错
    导致 CCM/锐化/降噪模块参数不完整，画面缺少后处理
```

> **▶ 关联函数**：`ioctl(fd, VIDIOC_S_FMT, &fmt)` — 告诉 ISP 输出分辨率和 NV12 格式；`ioctl(fd, VIDIOC_STREAMON)` — 触发 ISP 开始采集；`rkaiq_3A_server` 在后台通过私有接口持续更新 ISP 的曝光/白平衡寄存器

### 2.4 `camera_thread.cpp` — NV12 格式详解

ISP 输出的不是 RGB，而是 **NV12**（属于 YUV420 家族）：

```
为什么不直接输出 RGB？
  RGB 每个像素 3 字节 = 1280×720×3 = 2.76MB/帧
  NV12 每个像素平均 1.5 字节 = 1280×720×1.5 = 1.38MB/帧（省一半带宽）

YUV 的含义：
  Y = 亮度（Luminance）：人眼最敏感的信息，高分辨率保存
  U = 蓝色色差（Cb）：色彩信息，人眼不敏感，可以降分辨率
  V = 红色色差（Cr）：同上

NV12 内存布局（以 8×4 像素为例）：

  Y 平面（每个像素一个 Y 值，完整分辨率）：
  ┌──────────────────────────────────┐
  │ Y00 Y01 Y02 Y03 Y04 Y05 Y06 Y07 │  行 0
  │ Y10 Y11 Y12 Y13 Y14 Y15 Y16 Y17 │  行 1
  │ Y20 Y21 Y22 Y23 Y24 Y25 Y26 Y27 │  行 2
  │ Y30 Y31 Y32 Y33 Y34 Y35 Y36 Y37 │  行 3
  └──────────────────────────────────┘
  共 8×4 = 32 字节

  UV 平面（2×2 像素共享一组 UV，水平和垂直都减半）：
  ┌──────────────────────────────────┐
  │ U00 V00 U02 V02 U04 V04 U06 V06 │  行 0-1 共用
  │ U20 V20 U22 V22 U24 V24 U26 V26 │  行 2-3 共用
  └──────────────────────────────────┘
  共 8×2 = 16 字节（注意：U 和 V 是交替排列的）

  总大小 = 32 + 16 = 48 字节 = 8×4×1.5

对于 1280×720：
  Y 平面：1280 × 720 = 921,600 字节（地址 0 开始）
  UV 平面：1280 × 360 = 460,800 字节（地址 921600 开始）
  总大小：1,382,400 字节 ≈ 1.32MB
```

> **▶ 关联函数**：`mmap(NULL, buf_len, PROT_READ, MAP_SHARED, fd, offset)` — 把这块物理内存映射到用户空间；读 Y 平面：`uint8_t *y = (uint8_t*)buffers[i].start`；读 UV 平面：`uint8_t *uv = y + WIDTH * HEIGHT`

### 2.5 `camera_thread.cpp` — YUV → RGB 转换公式

```
BT.601 Full-Range（我们的代码用的标准）：

  R = Y + 1.402 × V
  G = Y - 0.344 × U - 0.714 × V
  B = Y + 1.772 × U

其中 U = U原始值 - 128，V = V原始值 - 128（居中到 ±128）

为什么要 -128？
  NV12 中 U/V 存储为 0~255，中间值 128 表示"无色偏"
  减掉 128 后，正值=偏暖/偏红，负值=偏冷/偏蓝

整数定点加速版本（我们最终用的）：
  1.402 × 256 = 358.9 ≈ 359
  0.344 × 256 = 88.1  ≈ 88
  0.714 × 256 = 182.8 ≈ 183
  1.772 × 256 = 453.6 ≈ 454

  R = Y + (359 × V >> 8)    // >> 8 就是 ÷256
  G = Y - (88 × U >> 8) - (183 × V >> 8)
  B = Y + (454 × U >> 8)

  为什么快？
  浮点乘法在 ARM 上需要 FPU 流水线切换，整数乘法+位移直接在 ALU 执行
  1280×720 = 92万像素，每像素省 3 次浮点 → 总共省约 45ms/帧
```
##### 插入说明 两个方向
RGB → YUV（编码方向）：省带宽、省存储
  摄像头/ISP 内部做的事
  把采集到的 RGB 信号压缩成 YUV 存储/传输

中间必须有人做转换

YUV → RGB（解码方向）：显示器只认RGB
  你的应用层代码做的事
  把 NV12 还原成 RGB 送给显示器



> **▶ 关联函数**：`clamp8(val)` — 把整数乘法结果限制在 [0, 255]（乘法可能超出 uint8 范围）；`img.scanLine(row)` — 写入 QImage 的目标行指针；`QImage(w, h, QImage::Format_RGB888)` — 创建承接 RGB 结果的图像

### 2.6 `v4l2_capture.c` — 代码中用到的所有系统函数速查

| 函数 | 头文件 | 作用 | 类比 |
|---|---|---|---|
| `open(path, O_RDWR)` | `<fcntl.h>` | 打开设备文件，返回文件描述符 fd | 像打开一个水龙头的阀门 |
| `close(fd)` | `<unistd.h>` | 关闭文件描述符 | 关阀门 |
| `ioctl(fd, cmd, &arg)` | `<sys/ioctl.h>` | 向设备驱动发送控制命令 | 对讲机：和驱动对话 |
                                              ioctl 本质是用户空间和内核驱动之间的桥梁
| `mmap(NULL, len, prot, flags, fd, offset)` | `<sys/mman.h>` | 把内核内存映射到用户空间 | 在你家墙上开个窗，直接看到隔壁（内核）的数据 |
| `munmap(addr, len)` | `<sys/mman.h>` | 解除 mmap 映射 | 封窗 |
| `memset(ptr, 0, size)` | `<string.h>` | 把内存块清零 | 擦黑板 |

### 2.7 `camera_thread.cpp` — 代码中用到的所有 V4L2 ioctl 命令

| 命令 | 作用 | 参数结构体 |
|---|---|---|
| `VIDIOC_S_FMT` | 设置视频格式（分辨率、像素格式） | `v4l2_format` |
| `VIDIOC_REQBUFS` | 请求内核分配 N 个 buffer | `v4l2_requestbuffers` |
| `VIDIOC_QUERYBUF` | 查询第 i 个 buffer 的大小和偏移量 | `v4l2_buffer` |
| `VIDIOC_QBUF` | 把 buffer 放入"待填充"队列（入队） | `v4l2_buffer` |
| `VIDIOC_DQBUF` | 从"已填充"队列取出一个 buffer（出队，阻塞） | `v4l2_buffer` |
| `VIDIOC_STREAMON` | 开始视频流（硬件开始采集） | `v4l2_buf_type` |
| `VIDIOC_STREAMOFF` | 停止视频流 | `v4l2_buf_type` |

### 2.8 `camera_thread.cpp` + `mainwindow.cpp` — 代码中用到的 Qt 类/函数

| 类/函数 | 作用 |
|---|---|
| `QThread` | 线程基类，`run()` 在子线程执行，不阻塞 UI |
| `QImage(w, h, Format_RGB888)` | 创建一张 RGB 图像（每像素 3 字节） |
| `img.scanLine(row)` | 返回第 row 行像素的内存指针（直接写，快） |
| `img.constScanLine(row)` | 同上但只读 |
| `QPixmap::fromImage(img)` | QImage → QPixmap（GPU 可加速的显示格式） |
| `pixmap.scaled(size, ratio, algo)` | 缩放到指定尺寸 |
| `emit frameReady(img)` | 发射信号，通知 UI 线程有新帧 |
| `connect(sender, signal, receiver, slot)` | 连接信号和槽（Qt 事件机制） |

---

## 模块三：数据链路——从 /dev/video11 到屏幕上的像素

### 3.1 `camera_thread.cpp` + `mainwindow.cpp` — 完整链路图

```
┌─────────────────────────────────── 硬件层 ──────────────────────────────────┐
│                                                                              │
│  物理光线 → [镜头] → [imx415 CMOS] → 10bit RAW → [MIPI CSI-2 总线]          │
│                          │                              │                    │
│                          │                    ┌─────────▼──────────┐         │
│                          │                    │  RK3588 ISP 硬件    │         │
│                          │                    │  (去马赛克/AWB/AE/  │         │
│                          │                    │   降噪/锐化/缩放)   │         │
│                          │                    └─────────┬──────────┘         │
│                          │                              │                    │
│                          │                    NV12 1280×720 写入 DMA buffer  │
└──────────────────────────│──────────────────────────────│────────────────────┘
                           │                              │
┌──────────────────────────│──── 内核驱动层 ──────────────│────────────────────┐
│                          │                              │                    │
│  rkaiq_3A_server ────────┘                              │                    │
│  (用户态守护进程,                           /dev/video11 (V4L2 设备节点)      │
│   通过 ioctl 控制                                       │                    │
│   ISP 的 AE/AWB 参数)                                  │                    │
│                                                         │                    │
│  V4L2 驱动管理 4 个 mmap buffer，形成环形队列：           │                    │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐                      │                    │
│  │buf 0│ │buf 1│ │buf 2│ │buf 3│   ← 内核物理内存       │                    │
│  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘                      │                    │
│     │ mmap  │ mmap  │ mmap  │ mmap                     │                    │
│     ▼       ▼       ▼       ▼                          │                    │
│  用户态虚拟地址（你的程序可以直接读的内存）                │                    │
└─────────────────────────────────────────────────────────┘                    │
                                                                               │
┌───────────────────── 应用层（camera_thread.cpp）─── CameraThread::run() ────┐ │
│                                                                              │ │
│  ① open("/dev/video11")           打开设备                                   │ │
│     │                                                                        │ │
│  ② ioctl(VIDIOC_S_FMT)           告诉 ISP：我要 1280×720 NV12               │ │
│     │                                                                        │ │
│  ③ ioctl(VIDIOC_REQBUFS)         请求 4 个 buffer                            │ │
│     │                                                                        │ │
│  ④ ioctl(VIDIOC_QUERYBUF) × 4    查询每个 buffer 的大小和偏移                │ │
│     │                                                                        │ │
│  ⑤ mmap() × 4                    把 4 个内核 buffer 映射到用户空间            │ │
│     │                             buffers[0].start ~ buffers[3].start        │ │
│     │                                                                        │ │
│  ⑥ ioctl(VIDIOC_QBUF) × 4       4 个 buffer 全部入队（交给硬件填数据）       │ │
│     │                                                                        │ │
│  ⑦ ioctl(VIDIOC_STREAMON)        开始！硬件开始采集                          │ │
│     │                                                                        │ │
│  ⑧ while(running) {              主循环                                      │ │
│     │                                                                        │ │
│     ├─ ioctl(VIDIOC_DQBUF)       从队列取出一个填好的 buffer（阻塞等待）     │ │
│     │  │                          返回 buf.index = 哪个buffer好了             │ │
│     │  │                                                                     │ │
│     │  ├─ nv12ToQImage()          逐像素 NV12 → RGB888                       │ │
│     │  │   │                                                                 │ │
│     │  │   ├─ 读 Y 平面           buffers[index].start[0 ~ W×H-1]           │ │
│     │  │   ├─ 读 UV 平面          buffers[index].start[W×H ~ W×H×1.5-1]     │ │
│     │  │   ├─ YUV→RGB 公式        整数定点，写入 QImage.scanLine()            │ │
│     │  │   └─ 返回 QImage         1280×720 RGB888 图像                       │ │
│     │  │                                                                     │ │
│     │  └─ emit frameReady(img)    发射信号给 UI 线程                          │ │
│     │                                                                        │ │
│     └─ ioctl(VIDIOC_QBUF)        把 buffer 还回队列，硬件继续填下一帧        │ │
│     }                                                                        │ │
│                                                                              │ │
│  ⑨ ioctl(VIDIOC_STREAMOFF)       停止采集                                   │ │
│  ⑩ munmap() × 4 + close(fd)      释放资源                                   │ │
└──────────────────────────────────────────────────────────────────────────────┘ │
                                                                                 │
┌───────────────────── UI 线程（mainwindow.cpp）──────────────────────────────┐  │
│                                                                              │  │
│  connect(cameraThread, &CameraThread::frameReady,                            │  │
│          this, [this](QImage img) {                                          │  │
│                                                                              │  │
│      QPixmap::fromImage(img)      QImage → QPixmap（可送 GPU）               │  │
│          │                                                                   │  │
│          .scaled(cameraLabel->size(),                                        │  │
│                  Qt::KeepAspectRatio,                                        │  │
│                  Qt::FastTransformation)                                     │  │
│          │                        1280×720 → 约 430×242（适配 label 大小）    │  │
│          │                                                                   │  │
│          ▼                                                                   │  │
│      cameraLabel->setPixmap(...)  显示在界面上                                │  │
│                                                                              │  │
│      如果用 VNC：Qt → VNC 编码 → 网络 → VNC 客户端 → 你的屏幕                │  │
│      如果用 eglfs：Qt → DRM/KMS → HDMI/MIPI DSI → 物理屏幕                  │  │
└──────────────────────────────────────────────────────────────────────────────┘  │
```

### 3.2 `camera_thread.cpp` — Buffer 环形队列详解

这是整个 V4L2 采集的核心机制，理解它就理解了视频采集：

```
初始状态（STREAMON 之后）：
  硬件队列：[buf0] [buf1] [buf2] [buf3]   ← 4 个 buffer 都等着被填
  用户手里：(空)

第 1 帧完成：
  硬件队列：[buf1] [buf2] [buf3]           ← 硬件继续填这些
  已填好：  [buf0]                          ← ISP 填好了，等你取

你调 DQBUF：
  硬件队列：[buf1] [buf2] [buf3]
  你手里：  [buf0]  → 正在做 NV12→RGB 转换

你调 QBUF（归还 buf0）：
  硬件队列：[buf1] [buf2] [buf3] [buf0]   ← buf0 回到队尾，循环利用
  你手里：  (空)

为什么需要 4 个 buffer？
  如果只有 1 个：你在处理时硬件没地方写 → 丢帧
  如果 2 个：你处理慢了还是丢
  4 个：给了足够的缓冲余量，硬件和软件互不等待
```

### 3.3 `camera_thread.cpp` — mmap 零拷贝原理

```
传统方式（read）：
  内核 buffer → [拷贝] → 用户 buffer       每帧 1.38MB 的内存拷贝

mmap 方式（零拷贝）：
  内核 buffer ←──────→ 用户虚拟地址          同一块物理内存，两个名字

  ┌────────────────────────────────────────────┐
  │           物理内存（DDR）                    │
  │                                            │
  │  ┌──────────────────────┐                  │
  │  │  buffer 0 的数据      │ ← ISP 硬件直接写 │
  │  │  (1,382,400 bytes)   │                  │
  │  └──────────────────────┘                  │
  │         ▲           ▲                      │
  │         │           │                      │
  │    内核看到的     你的程序看到的              │
  │    物理地址       虚拟地址                   │
  │                  (mmap 返回值)              │
  └────────────────────────────────────────────┘

  数据零拷贝：ISP 写完，你直接读，没有任何复制操作
  这就是为什么 mmap 方式是视频采集的标准做法
```

### 3.4 `camera_thread.cpp` + `mainwindow.cpp` — 线程模型

```
┌──────────────────────┐    frameReady(QImage)    ┌──────────────────────┐
│  CameraThread        │ ══════════════════════▶  │  MainWindow (UI)     │
│  (子线程)             │      信号-槽机制         │  (主线程)             │
│                      │      Qt 自动排队          │                      │
│  - 阻塞等 DQBUF     │      跨线程安全           │  - setPixmap 显示    │
│  - NV12→RGB 转换     │                          │  - 处理用户点击      │
│  - 不碰任何 UI 控件  │                          │  - 刷新界面          │
└──────────────────────┘                          └──────────────────────┘

为什么要分线程？
  DQBUF 是阻塞调用 → 如果在 UI 线程做，等待期间界面卡死
  NV12→RGB 1280×720 约 15ms → 如果在 UI 线程做，界面每帧卡 15ms
  子线程处理完通过 emit 发信号，UI 线程收到后只做一次 setPixmap（<1ms）
```

### 3.5 `camera_thread.h` + `camera_thread.cpp` + `mainwindow.cpp` — 每个环节的耗时和清晰度影响

```
环节              耗时          对清晰度的影响
─────────────────────────────────────────────────
镜头对焦          -             ★★★★★ 失焦 = 全完（我们的根因）
ISP 缩放          硬件,0ms      ★★★☆☆ 3864→1280 丢细节
rkaiq 3A          后台运行      ★★☆☆☆ 没 3A 时色彩差、无锐化
NV12→RGB          ~15ms         ★☆☆☆☆ 整数定点精度损失 <1 灰阶
Qt scaled()       ~2ms          ★☆☆☆☆ 1280→430 缩小，差异小
VNC 编码          ~5ms          ★★★☆☆ 有损压缩，运动画面明显
```

### 3.6 `camera_thread.h` — 关键数字速查

| 参数 | 值 | 来源 |
|---|---|---|
| 传感器原始分辨率 | 3864 × 2192 | imx415 spec |
| ISP 输出分辨率 | 1280 × 720 | camera_thread.h WIDTH/HEIGHT |
| 像素格式 | NV12 (YUV420) | VIDIOC_S_FMT |
| 每帧数据量 | 1,382,400 字节 (1.32MB) | W×H×1.5 |
| Buffer 数量 | 4 | BUF_COUNT |
| NV12→RGB 耗时 | ~15ms | 整数定点 |
| cameraLabel 显示尺寸 | ~430 × 242 | 窗口 1024×600 三列布局 |
| 显示帧率 | ~30fps | ISP 输出帧率 |

## 模块 四：加上开关

### 4.1 需求
- 默认打开 Qt 时摄像头自动采集（和之前一样）
- 界面上有个按钮，点击后停止采集并关闭摄像头，再点一次重新开启

### 4.2 修改记录

**（1）`mainwindow.h` — 新增成员**

```cpp
QPushButton *btnCameraToggle;   // 摄像头开关按钮
bool cameraRunning = true;      // 跟踪当前摄像头状态
void onBtnCameraToggle();       // 按钮槽函数
```

**（2）`mainwindow.cpp` — `setupUI()` 中添加按钮**

在 `cameraLabel` 下方、导航按钮行上方插入一个开关按钮：

```cpp
btnCameraToggle = new QPushButton("■ 关闭摄像头");
btnCameraToggle->setObjectName("navBtn");
btnCameraToggle->setFixedHeight(32);
// 默认红色底线（表示"点击即关闭"）
btnCameraToggle->setStyleSheet(
    "QPushButton#navBtn { border-bottom: 3px solid #ff4444; font-size: 13px; }"
);

// 布局顺序：camTitle → cameraLabel → btnCameraToggle → navRow
midCol->addWidget(cameraLabel, 1);
midCol->addWidget(btnCameraToggle);    // ← 新增
midCol->addSpacing(6);
midCol->addLayout(navRow);
```

**（3）`mainwindow.cpp` — `setupConnections()` 中连接信号**

```cpp
connect(btnCameraToggle, &QPushButton::clicked,
        this, &MainWindow::onBtnCameraToggle);
```

**（4）`mainwindow.cpp` — `onBtnCameraToggle()` 实现**

```cpp
void MainWindow::onBtnCameraToggle()
{
    if (cameraRunning) {
        // 关闭：stop() 置 running=false → run() 退出 → wait() 等线程结束
        cameraThread->stop();
        cameraThread->wait();
        cameraRunning = false;

        // UI 反馈：按钮变绿（表示"点击即开启"），label 显示提示文字
        btnCameraToggle->setText("▶ 开启摄像头");
        btnCameraToggle->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #4caf50; font-size: 13px; }"
        );
        cameraLabel->clear();
        cameraLabel->setText("摄像头已关闭\n\n点击下方按钮重新开启");
    } else {
        // 开启：QThread::start() → 重新执行 run() → initDevice() + 采帧循环
        cameraThread->start();
        cameraRunning = true;

        // UI 反馈：按钮变红（表示"点击即关闭"）
        btnCameraToggle->setText("■ 关闭摄像头");
        btnCameraToggle->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #ff4444; font-size: 13px; }"
        );
    }
}
```

### 4.3 工作原理

```
点击"■ 关闭摄像头"：
  onBtnCameraToggle()
    → cameraThread->stop()      // running = false
    → while(running) 退出       // run() 自然结束，内部调 closeDevice()
    → cameraThread->wait()      // 主线程等子线程真正结束
    → 更新按钮文字和颜色

点击"▶ 开启摄像头"：
  onBtnCameraToggle()
    → cameraThread->start()     // QThread 重新执行 run()
    → run() 里 initDevice()     // 重新 open + mmap + STREAMON
    → while(running) 开始采帧   // emit frameReady → UI 恢复显示
    → 更新按钮文字和颜色
```

关键点：`QThread::start()` 可以在线程结束后再次调用，Qt 会重新执行 `run()`。
`stop()` + `wait()` 确保线程完全退出后才更新 UI，避免竞态。

## 模块 五：技术点体现
一、多线程
主线程（UI）          子线程（CameraThread）
    │                       │
    │  cameraThread->start()│
    │ ─────────────────────▶│
    │                       │  run()：
    │                       │  ① open("/dev/video11")
    │                       │  ② DQBUF（阻塞等硬件）
    │                       │  ③ nv12ToQImage()  ← 耗时 ~15ms
    │                       │  ④ emit frameReady(img)
    │ ◀─────────────────────│
    │  slot: setPixmap()     │

二、解耦
V4L2 驱动层          CameraThread（采集+转换）      MainWindow（显示）
───────────           ─────────────────────────      ─────────────────
不知道 Qt 存在         不碰任何 Label/Widget          不知道 V4L2 存在
只操作 /dev/video11   只 emit frameReady(QImage)     只收 QImage 显示

CameraThread 和 MainWindow 之间唯一的接口就是 frameReady(QImage) 这一个信号。换掉任何一侧都不影响另一侧：
想换成 USB 摄像头？只改 CameraThread，MainWindow 一行不动
想换成别的显示方式？只改 MainWindow，CameraThread 一行不动

三、线程安全
<!-- camera_thread.h -->
volatile bool running;   // ← volatile 关键字
<!-- mainwindow.cpp -->
connect(cameraThread, &CameraThread::frameReady,
        this, [this](QImage img) {
            cameraLabel->setPixmap(...);   // 在 UI 线程执行
        });
<!-- volatile bool running：禁止编译器把 running 缓存到寄存器。外部调用 stop() 置 false，子线程的 while(running) 一定能看到这个变化，不会死循环 -->
<!-- Qt 信号-槽的跨线程队列：emit frameReady(img) 在子线程发出，但 setPixmap 在 UI 主线程执行（Qt 自动把信号投递到目标线程的事件队列）。QImage 传递时做了隐式共享（Copy-on-Write），不需要手动加锁-->

四、总结：这实际上是一个经典的生产者-消费者模型 
CameraThread（生产者）──emit──▶ Qt 事件队列 ──▶ MainWindow（消费者）
    硬件采集 + 格式转换              线程安全缓冲区              UI 显示

## 模块 六：添加抓拍功能

### 6.1 需求
- 界面上有个"抓拍"按钮，和摄像头开关并排
- 点击后把当前摄像头画面保存为 PNG 到 `/home/cat/chezai/VehicleTerminal/picture/`
- 文件名带时间戳，格式 `cap_20260308_153012.png`

### 6.2 思路

```
问题：CameraThread 每秒发 30 帧，抓拍是"某一刻"用户点按钮
方案：用一个 QImage lastFrame 成员，每帧都更新它
      点击抓拍时，直接拿 lastFrame 调 save() 保存

为什么不在 CameraThread 里保存？
  save() 要编码 PNG（压缩耗时），如果在采集线程做会卡住采帧
  而 lastFrame 是通过信号-槽传到 UI 线程的 QImage（隐式共享）
  在 UI 线程 save 不影响采集
```

### 6.3 修改记录

**（1）`mainwindow.h` — 新增 3 个成员**

```cpp
QPushButton *btnCapture;   // 抓拍按钮
QImage lastFrame;          // 缓存最新一帧（每帧更新）
void onBtnCapture();       // 抓拍槽函数
```

**（2）`mainwindow.cpp` — `setupUI()` 中创建按钮并横排布局**

```cpp
// 创建抓拍按钮
btnCapture = new QPushButton("📸 抓拍");
btnCapture->setObjectName("navBtn");
btnCapture->setFixedHeight(32);
btnCapture->setStyleSheet(
    "QPushButton#navBtn { border-bottom: 3px solid #2196f3; font-size: 13px; }"
);

// 和摄像头开关按钮横排
QHBoxLayout *camBtnRow = new QHBoxLayout;
camBtnRow->addWidget(btnCameraToggle);
camBtnRow->addWidget(btnCapture);
midCol->addLayout(camBtnRow);    // 替换原来的 midCol->addWidget(btnCameraToggle)
```

**（3）`mainwindow.cpp` — `setupConnections()` 中做两件事**

```cpp
// ① 在 frameReady lambda 里缓存每一帧
connect(cameraThread, &CameraThread::frameReady,
        this, [this](QImage img) {
    lastFrame = img;      // ← 每帧更新，QImage 隐式共享，零拷贝
    cameraLabel->setPixmap(
        QPixmap::fromImage(img).scaled(
            cameraLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation)
    );
});

// ② 连接抓拍按钮
connect(btnCapture, &QPushButton::clicked, this, &MainWindow::onBtnCapture);
```

**（4）`mainwindow.cpp` — `onBtnCapture()` 实现**

```cpp
void MainWindow::onBtnCapture()
{
    if (lastFrame.isNull()) return;   // 没有帧就不保存

    QString dir = "/home/cat/chezai/VehicleTerminal/picture";
    QString filename = QString("%1/cap_%2.png")
        .arg(dir)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    lastFrame.save(filename, "PNG");
}
```

> 注意：需要在 `mainwindow.cpp` 顶部加 `#include <QDateTime>`

### 6.4 工作原理

```
每一帧的数据流：
  CameraThread::run()
    → nv12ToQImage()          生成 QImage
    → emit frameReady(img)    信号发到 UI 线程
    → lambda 里：
        lastFrame = img       缓存（QImage 隐式共享，不真正拷贝）
        setPixmap(...)        显示

用户点击"📸 抓拍"：
  onBtnCapture()
    → lastFrame.isNull()?    检查有没有帧
    → QDateTime 生成时间戳    "20260308_153012"
    → lastFrame.save(path, "PNG")
        ↓
      QImage 内部调 libpng 编码器
        ↓
      RGB888 → PNG 压缩 → 写入磁盘
      1280×720 RGB888 ≈ 2.76MB → PNG 压缩后约 1-2MB
```

### 6.5 关键点

| 问题 | 答案 |
|---|---|
| `lastFrame = img` 会不会拷贝 2.76MB？ | 不会。QImage 用 Copy-on-Write（写时复制），赋值只是引用计数+1 |
| PNG 编码慢不慢？ | 约 50-100ms，在 UI 线程执行。因为是用户手动点，偶尔一次没问题 |
| 如果 picture 目录不存在？ | `save()` 会失败返回 false。目录已手动创建 |
| 文件名会重复吗？ | 精确到秒，1秒内只能点一次，不会重复 |

---

## 数据流框架
用户空间
┌─────────────────────────────────────────────────────────────────────┐
│                        用户空间                                      │
│                    camera_thread.cpp                                 │
│                                                                      │
│   open()  →  S_FMT  →  REQBUFS  →  mmap  →  STREAMON  →  DQBUF    │
│   打开       设格式    申请buffer  映射内存   开始采集     取一帧      │
└───────────────────────────┬─────────────────────────────────────────┘
                            │ ioctl 系统调用
━━━━━━━━━━━━━━━━━━━━━━━━━━━━│━━━━━━━━━━━━━ 用户/内核边界 ━━━━━━━━━━━━━━
                            │
┌───────────────────────────▼─────────────────────────────────────────┐
│                    RKCIF 驱动（/dev/video11）                        │
│                                                                      │
│   • 管理 4 个 DMA buffer（环形队列）                                  │
│   • REQBUFS/QBUF/DQBUF 都在这里处理                                  │
│   • 硬件填满一帧 → 产生中断 → 唤醒 DQBUF                             │
│                                                                      │
│   控制流向下 ↓                          数据流向上 ↑                  │
│             │                          NV12图像经DMA写入buffer        │
└─────────────┼──────────────────────────────────────────────────────┘
              │
┌─────────────▼──────────────────────────────────────────────────────┐
│                       RKISP 驱动                                    │
│                                                                      │
│   RAW10 → 黑电平校正 → 去马赛克 → AWB → CCM → 降噪 → 锐化 → NV12   │
│                                                                      │
│   rkaiq_3A_server（后台守护进程）                                     │
│   持续读取统计数据 → 计算AE/AWB → 通过ioctl更新ISP参数                │
│                                                                      │
│   控制流向下 ↓                          数据流向上 ↑                  │
│             │                          RAW10经MIPI传入               │
└─────────────┼──────────────────────────────────────────────────────┘
              │
┌─────────────▼──────────────────────────────────────────────────────┐
│                   imx415.c（sensor子设备驱动）                       │
│                                                                      │
│   S_FMT → imx415_set_fmt()      选分辨率模式（1280×720等）           │
│                                                                      │
│   STREAMON → imx415_s_stream()                                       │
│     ├─ pm_runtime_get_sync()    确保传感器已上电                      │
│     ├─ write(global_reg_list)   写校准参数（ADC/时序）                │
│     ├─ write(reg_list)          写分辨率/帧率配置                     │
│     └─ write(0x3000 = 0x00)     解除standby，开始出图 ✓              │
│                                                                      │
│   STREAMOFF → imx415_s_stream()                                      │
│     ├─ write(0x3000 = 0x01)     进入standby，停止出图                │
│     └─ pm_runtime_put()         引用计数-1，框架决定是否断电           │
│                                                                      │
│   s_ctrl → imx415_set_ctrl()    动态调参                             │
│     ├─ 曝光: SHR0 = VTS - 曝光行数  → 写L/M/H三个寄存器             │
│     ├─ 增益: 直接写增益值            → 写H/L两个寄存器               │
│     └─ 帧率: 写VTS寄存器            → 帧率=像素时钟÷VTS÷行宽         │
│                                                                      │
│             ↓ imx415_write_reg() → i2c_master_send()                │
└─────────────┼──────────────────────────────────────────────────────┘
              │ I2C总线（16bit地址 + 数据，400KHz）
━━━━━━━━━━━━━━│━━━━━━━━━━━━━━━━━━━━ 软件/硬件边界 ━━━━━━━━━━━━━━━━━━━━
              │
┌─────────────▼──────────────────────────────────────────────────────┐
│                    IMX415 物理芯片                                   │
│                                                                      │
│   光子 → RGGB Bayer阵列（3864×2192）→ 10bit RAW数据                 │
│                                                                      │
│   MIPI CSI-2（4-Lane 差分信号，1.5GHz）→ 往上推RAW数据              │
└─────────────────────────────────────────────────────────────────────┘

## 用户态（可以看v2l42_capture.c和camera_thread.cpp）------------ imx415 驱动（可以看imx415.c）-
┌─────────────────────────────────────────────────────────────────────────────┐
│              用户态 ioctl  →  V4L2框架  →  imx415.c 完整对应表              │
└─────────────────────────────────────────────────────────────────────────────┘

用户态调用                      V4L2框架做了什么              imx415.c落地函数
─────────────────────────────────────────────────────────────────────────────

open("/dev/video11")
  │                             probe()已在启动时完成          无
  │                             框架直接返回fd                 （probe是模块加载时跑的）
  ▼

ioctl(VIDIOC_S_FMT)
  │  告诉驱动：               先调enum_frame_sizes            imx415_enum_frame_sizes()
  │  我要1280×720 NV12        确认分辨率支持                   └─ 查supported_modes[]表
  │                            再调enum_mbus_code              imx415_enum_mbus_code()
  │                            确认像素格式支持                 └─ 返回MEDIA_BUS_FMT_SGRBG10
  │                            最后调set_fmt                   imx415_set_fmt()
  │                                                             ├─ imx415_find_best_fit()
  │                                                             │   遍历supported_modes[]
  │                                                             │   找最接近的分辨率
  │                                                             └─ 更新cur_mode指针
  ▼                                                                 （此时还没写寄存器）

ioctl(VIDIOC_REQBUFS)
  │  申请4个buffer             RKCIF分配DMA物理内存            无
  ▼                            imx415.c不参与

ioctl(VIDIOC_QUERYBUF)
  │  查询buffer信息            RKCIF返回size和offset           无
  ▼                            imx415.c不参与

mmap()
  │  映射buffer到用户空间      RKCIF处理内存映射               无
  ▼                            imx415.c不参与

ioctl(VIDIOC_QBUF) ×4
  │  buffer入队                RKCIF管理buffer队列             无
  ▼                            imx415.c不参与

ioctl(VIDIOC_STREAMON)
  │  开始采集                  框架调s_stream(on=1)            imx415_s_stream(on=1)
  │                                                             ├─ mutex_lock()
  │                                                             ├─ pm_runtime_get_sync()
  │                                                             │   → __imx415_power_on()
  │                                                             │     ① 开电源(dvdd/dovdd/avdd)
  │                                                             │     ② 开时钟(xvclk 37.125MHz)
  │                                                             │     ③ 拉高reset_gpio
  │                                                             ├─ write(global_reg_list)
  │                                                             │   ADC/时序校准参数（I2C）
  │                                                             ├─ write(reg_list)
  │                                                             │   1280×720分辨率/帧率配置
  │                                                             ├─ v4l2_ctrl_handler_setup()
  │                                                             │   把曝光/增益当前值同步写寄存器
  │                                                             ├─ write(0x3000 = 0x00)
  │                                                             │   解除standby，sensor开始出RAW
  │                                                             └─ mutex_unlock()
  ▼

ioctl(VIDIOC_DQBUF)  ←── 循环
  │  取一帧（阻塞）            RKCIF等中断                     无
  │                            sensor出RAW → MIPI              imx415.c不参与数据流
  │                            → RKISP处理 → NV12              只负责控制sensor出图
  │                            → DMA写入buffer
  │                            → 产生中断 → 唤醒DQBUF
  ▼

ioctl(VIDIOC_S_CTRL, EXPOSURE=xxx)    ← rkaiq自动调或手动调
  │                            框架调s_ctrl                    imx415_set_ctrl()
  │                                                             ├─ pm_runtime_get_if_in_use()
  │                                                             │   设备没开则直接返回
  │                                                             ├─ case V4L2_CID_EXPOSURE:
  │                                                             │   SHR0 = cur_vts - ctrl->val
  │                                                             │   write(EXPO_REG_L, SHR0低8位)
  │                                                             │   write(EXPO_REG_M, SHR0中8位)
  │                                                             │   write(EXPO_REG_H, SHR0高4位)
  │                                                             ├─ case V4L2_CID_ANALOGUE_GAIN:
  │                                                             │   write(GAIN_REG_H, 增益高位)
  │                                                             │   write(GAIN_REG_L, 增益低位)
  │                                                             └─ case V4L2_CID_VBLANK:
  │                                                                 VTS = 消隐行 + 有效行
  │                                                                 write(VTS_REG_L/M/H)
  ▼

ioctl(VIDIOC_STREAMOFF)
  │  停止采集                  框架调s_stream(on=0)            imx415_s_stream(on=0)
  │                                                             ├─ write(0x3000 = 0x01)
  │                                                             │   进入standby，停止出RAW
  │                                                             └─ pm_runtime_put()
  ▼                                                                 引用计数-1，框架决定是否断电

munmap() + close(fd)
                              框架清理fd                        无
                              RKCIF释放DMA buffer               imx415.c不参与

─────────────────────────────────────────────────────────────────────────────
规律总结：
  buffer相关（REQBUFS/QUERYBUF/QBUF/DQBUF）→ 全部由 RKCIF 处理，imx415.c不参与
  控制相关（S_FMT/STREAMON/S_CTRL）        → 经框架路由到 imx415.c 对应函数
  数据流（sensor出RAW到buffer）            → 纯硬件DMA，imx415.c只负责"开关"


## 启动流程

必须先启动 rkaiq_3A_server（ISP 的 3A 算法服务），否则画面偏青色（CCM/AWB 不生效）。

```bash
# 1. 启动 ISP 3A 服务（后台运行）
sudo rkaiq_3A_server &

# 2. 等待 ISP 初始化完成
sleep 2

# 3. 进入项目目录，启动 Qt 应用
cd /home/cat/chezai/VehicleTerminal
./VehicleTerminal -platform vnc:port=5902
```

启动顺序原因：
```
rkaiq_3A_server 启动后会：
  ① 扫描 /dev/media* 找到 RKISP 设备
  ② 读取 /etc/iqfiles/ 下的 IQ 参数文件
  ③ 配置 ISP 的 AE/AWB/CCM/降噪/锐化 模块
  ④ 进入循环，持续根据画面亮度调整曝光和白平衡

如果不启动 rkaiq_3A_server：
  ISP 只做最基本的 RAW → NV12 转换
  没有白平衡校正 → 偏色（青色）
  没有色彩矩阵 → 颜色不准
  没有自动曝光 → 可能过亮或过暗
```

VNC 访问：在 PC 上用 VNC 客户端连接 `板子IP:5902`


---

## 模块七：RGA 硬件加速 NV12→RGB 转换

### 背景

之前的 `nv12ToQImage()` 用 CPU 做浮点 YUV→RGB 转换，640×480 每帧约 15ms（整数优化后）。
RK3588 内置 **RGA**（Raster Graphic Acceleration Unit）硬件 2D 加速器，可以在硬件层面完成色彩空间转换、缩放、旋转等操作，转换耗时降至 ~1-2ms。

### RGA 环境确认

```bash
# 检查头文件
ls /usr/include/rga/
# → im2d.h  im2d.hpp  rga.h  RgaUtils.h  im2d_buffer.h  im2d_type.h ...

# 检查动态库
ls /usr/lib/aarch64-linux-gnu/librga*
# → librga.so  librga.so.2  librga.so.2.1.0

# API 版本
grep "RGA_API_MAJOR_VERSION\|RGA_API_MINOR_VERSION" /usr/include/rga/im2d_version.h
# → RGA_API_MAJOR_VERSION 1, RGA_API_MINOR_VERSION 10
```

### 改动内容

#### 1. camera_qt.pro 添加 RGA 链接

```diff
+ # RGA 硬件加速（NV12→RGB 色彩空间转换）
+ INCLUDEPATH += /usr/include/rga
+ LIBS += -lrga
```

#### 2. camera_thread.h 添加 RGA 头文件和成员

```diff
  #include <QThread>
  #include <QImage>
  #include <linux/videodev2.h>
+ #include "im2d.hpp"
+ #include "rga.h"

  ...

      int fd;
      CamBuffer buffers[BUF_COUNT];
      volatile bool running;
+     uint8_t *rgbBuffer;            // RGA 输出 RGB buffer
```

构造函数初始化 `rgbBuffer(nullptr)`，`closeDevice()` 中 `delete[] rgbBuffer`。

#### 3. nv12ToQImage() 替换为 RGA 版本

**旧版（CPU 浮点运算）：**
```cpp
// 双层 for 循环，逐像素 Y/U/V → R/G/B，qBound 裁剪
// 640×480 约 15ms/帧
```

**新版（RGA 硬件加速）：**
```cpp
QImage CameraThread::nv12ToQImage(void *data, int width, int height)
{
    if (!rgbBuffer)
        rgbBuffer = new uint8_t[width * height * 3];

    // 包装源和目标 buffer
    rga_buffer_t src = wrapbuffer_virtualaddr(data, width, height,
                                               RK_FORMAT_YCbCr_420_SP);
    rga_buffer_t dst = wrapbuffer_virtualaddr(rgbBuffer, width, height,
                                               RK_FORMAT_RGB_888);

    // RGA 硬件转换
    IM_STATUS status = imcvtcolor(src, dst,
                                   RK_FORMAT_YCbCr_420_SP,
                                   RK_FORMAT_RGB_888);
    if (status != IM_STATUS_SUCCESS) {
        qDebug() << "RGA imcvtcolor failed:" << imStrError(status);
        return QImage();
    }

    return QImage(rgbBuffer, width, height, width * 3,
                  QImage::Format_RGB888).copy();
}
```

### 关键 API 说明

| API | 作用 |
|-----|------|
| `wrapbuffer_virtualaddr(addr, w, h, fmt)` | 把用户空间虚拟地址包装成 `rga_buffer_t` |
| `imcvtcolor(src, dst, sfmt, dfmt)` | 硬件色彩空间转换 |
| `imStrError(status)` | 错误码转可读字符串 |
| `RK_FORMAT_YCbCr_420_SP` | NV12 格式（Y + 交错 UV） |
| `RK_FORMAT_RGB_888` | RGB888 格式（R:G:B 各 8bit） |

### 数据流对比

```
旧路径：V4L2 DMA buffer (NV12) → CPU逐像素转换 → QImage (RGB888)
新路径：V4L2 DMA buffer (NV12) → RGA硬件转换 → rgbBuffer (RGB888) → QImage.copy()

性能：CPU ~15ms/帧 → RGA ~1-2ms/帧（约10倍提升）
CPU占用：从单核30-40% 降至几乎为零（DMA搬运，不占CPU）
```

### 注意事项

1. **virtualaddr 模式**：当前使用虚拟地址模式，RGA 内部会做 cache flush，有少量开销。
   如果后续要极致优化，可改用 DMABUF fd 模式（需重构 V4L2 buffer 管理），但当前 1280×720 完全够用。
2. **QImage.copy()**：必须深拷贝，因为 `rgbBuffer` 下一帧会被覆盖。
3. **编译验证**：`qmake && make -j4` 通过，零错误零警告。

### 踩坑记录：msleep(33) 导致延迟 & 每帧 new 导致延迟回升

#### Bug 1：msleep(33) 引入额外一帧延迟

之前 `run()` 循环末尾有 `msleep(33)` 用于"限速 30fps"。
但 `DQBUF` 本身就是阻塞调用（fd 没开 `O_NONBLOCK`），硬件 30fps 自然限速 ~33ms/帧。
额外 sleep 33ms = 每帧凭空多一帧延迟（~66ms 总延迟 vs ~33ms）。

旧 CPU 转换耗 15ms 时延迟感不明显（15ms+33ms≈48ms），换 RGA 后只要 2ms（2ms+33ms≈35ms 但体感有卡顿），
因为 msleep 的作用从"补齐帧间隔"变成了"纯粹等待"。

**修法**：直接删掉 `msleep(33)`，让 DQBUF 阻塞自然限速。

```diff
- // debug1:限速 30fps，让UI有时间渲染
- msleep(33);
+ // DQBUF 本身是阻塞的，硬件帧率自然限速，无需 msleep
```

#### Bug 2：每帧 new/delete 2.76MB 导致延迟回升

第一版 RGA 修复时，为了避免 `.copy()` 的 memcpy 开销，改成了每帧 `new uint8_t[w*h*3]`，
通过 QImage 的 cleanup 回调在 QImage 销毁时自动 `delete[]`。

**看起来很优雅，实际有坑**：
- 1280×720×3 = **2.76MB/帧**，30fps = **~83MB/s** 堆分配/释放
- 运行一段时间后内存碎片化，`new` 变慢
- 如果 UI 线程稍有卡顿（scaled 缩放），QImage 在信号队列中堆积，每个占 2.76MB
- cleanup 回调延迟释放 → 内存持续增长 → 系统内存压力 → 延迟回升

**修法**：回归复用 `rgbBuffer` + `.copy()`。

```
复用 rgbBuffer（只分配一次，2.76MB）→ RGA 写入 → .copy() 深拷贝（~0.5ms memcpy）→ emit
```

`.copy()` 在 1280×720 下只是 2.76MB memcpy，ARM 上约 0.5ms，代价可忽略。
但换来的是：**稳定的内存占用、零碎片化、长时间运行无延迟回升**。

**教训**：高频大块内存分配（>1MB × 30fps）不要用 per-frame new/delete，用复用 buffer + copy 更稳。

### CPU 占用率实测（1280×720）

| 状态 | CPU 占用 | 说明 |
|------|---------|------|
| 摄像头关闭 | ~9% | 雷达扫描 + 时钟 + DHT11 + 系统监控 |
| 摄像头打开（RGA） | ~19% | 新增：DQBUF + RGA + .copy() + UI scaled 显示 |
| **摄像头增量** | **~10%** | 其中 RGA 转换几乎不占 CPU，主要是 .copy() memcpy 和 UI 线程 scaled() 缩放 |

RGA 的价值在高分辨率时更明显（1080p/4K），640×480 时 CPU 转换本来就快，差异不大。
当前 1280×720 下增量 10% 是合理水平。

### 零拷贝机制回顾

整条数据流中的"零拷贝"仍然在：

```
sensor 出 RAW → MIPI CSI 传输 → RKISP 硬件处理 → DMA 写入内核 buffer
      ↓ 零拷贝 ↓
mmap() 映射到用户空间（buffers[i].start 直接指向 DMA buffer）
      ↓ 零拷贝 ↓
wrapbuffer_virtualaddr() 包装给 RGA（直接用 mmap 地址，不拷贝）
      ↓ RGA 硬件 DMA ↓
rgbBuffer（RGA 输出）
      ↓ 唯一一次 CPU 拷贝 ↓
QImage.copy()（2.76MB memcpy，~0.5ms）
```

从 sensor 到 RGA 输出，全程零 CPU 拷贝。唯一的 memcpy 是最后的 `.copy()`，
这是必要的——因为 rgbBuffer 会被下一帧覆盖，必须让 QImage 持有独立数据。

### RGA API 扩展知识

RGA（Raster Graphic Acceleration Unit）是 Rockchip SoC 内置的 2D 硬件加速器。
RK3588 有 **3 个 RGA 核心**（2×RGA3 + 1×RGA2），支持并行处理。

#### im2d API 全家桶

librga 提供的 im2d API 是对 RGA 驱动的用户空间封装，主要函数：

| API | 功能 | 典型用途 |
|-----|------|---------|
| `imcopy(src, dst)` | 图像拷贝（DMA 搬运） | 替代 memcpy，零 CPU 占用 |
| `imresize(src, dst)` | 图像缩放 | 替代 Qt 的 scaled()，硬件加速 |
| `imcvtcolor(src, dst, sfmt, dfmt)` | 色彩空间转换 | NV12→RGB（本项目在用） |
| `imrotate(src, dst, rotation)` | 旋转（90°/180°/270°） | 画面旋转 |
| `imflip(src, dst, mode)` | 水平/垂直翻转 | 镜像 |
| `imcrop(src, dst, rect)` | 图像裁剪 | ROI 区域提取 |
| `imblend(fg, bg, mode)` | Alpha 混合 | OSD 叠加 |
| `imfill(dst, rect, color)` | 矩形填充 | 画框标注 |
| `immosaic(img, rect, mode)` | 马赛克 | 隐私遮挡 |
| `improcess(src, dst, pat, ...)` | 复合操作 | 一次调用完成缩放+转换+旋转 |

#### 三种 buffer 传入方式

```cpp
// 方式1：虚拟地址（当前使用）—— 最简单，RGA 内部做 cache flush
rga_buffer_t buf = wrapbuffer_virtualaddr(ptr, w, h, fmt);

// 方式2：物理地址 —— 需要 CMA/DMA 分配器，无 cache 问题
rga_buffer_t buf = wrapbuffer_physicaladdr(phy_addr, w, h, fmt);

// 方式3：DMA-BUF fd —— 零拷贝最优解，V4L2 EXPBUF 导出 fd 直接传入
rga_buffer_t buf = wrapbuffer_fd(dma_fd, w, h, fmt);
```

| 方式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| virtualaddr | 最简单，改动最小 | RGA 内部需 cache flush | 中小分辨率，快速集成 |
| physicaladdr | 无 cache 问题 | 需 CMA 分配器 | 特殊内存管理 |
| DMA-BUF fd | 全链路零拷贝，最高性能 | 需重构 V4L2 buffer 导出 | 高分辨率/极致性能 |

#### RK3588 RGA 硬件规格

| 参数 | RGA3 (×2) | RGA2 (×1) |
|------|-----------|-----------|
| 最大分辨率 | 8176×8176 | 8128×8128 |
| 支持格式 | RGB/YUV 各种 | RGB/YUV 各种 |
| 色彩空间转换 | ✅ | ✅ |
| 缩放 | ✅ (1/8~8x) | ✅ (1/16~16x) |
| 旋转 | ✅ | ✅ |
| Alpha 混合 | ✅ | ✅ |
| 马赛克 | ✅ | ❌ |

#### 潜在优化方向：用 RGA 替代 Qt scaled()

当前 UI 线程用 Qt 的 `scaled()` 做 1280×720 → cameraLabel 尺寸的缩放，这是 CPU 操作。
可以在 `nv12ToQImage()` 中用 RGA 的 `imresize()` 一步完成缩放+转换，进一步降低 CPU 负载：

```cpp
// 未来优化思路（不急）：RGA 一步完成 NV12→RGB + 缩放
rga_buffer_t src = wrapbuffer_virtualaddr(data, 1280, 720, RK_FORMAT_YCbCr_420_SP);
rga_buffer_t dst = wrapbuffer_virtualaddr(rgbSmall, 430, 242, RK_FORMAT_RGB_888);
improcess(src, dst, {}, {}, {}, {}, IM_HAL_TRANSFORM_ROT_0, IM_SYNC);
// → 硬件同时完成色彩转换 + 缩放，UI 线程不再需要 scaled()
```

### USE_RGA 宏开关

为了方便对比测试，在 `camera_thread.h` 中加了编译期开关：

```cpp
// ========== RGA 开关 ==========
// 注释掉下面这行则回退到纯 CPU 整数定点 NV12→RGB 转换
#define USE_RGA
// ==============================
```

- `#define USE_RGA`：使用 RGA 硬件加速 `imcvtcolor()` + `.copy()`
- 注释掉：回退到 CPU 整数定点（`359*v>>8`）版本，无 `.copy()`

两套代码用 `#ifdef USE_RGA ... #else ... #endif` 隔开，切换后 `make clean && qmake && make -j4` 即可。

### 多分辨率实测对比

在 VehicleTerminal 完整界面下实测（含雷达、温湿度、时钟等模块），摄像头开关前后 CPU 占用对比：

#### 720p（1280×720）

| 配置 | 摄像头关 | 摄像头开 | 增量 | 流畅度 |
|------|---------|---------|------|--------|
| CPU 整数定点 | ~9% | ~19% | +10% | ★★★★★ 非常流畅 |
| RGA 硬件加速 | ~9% | ~19% | +10% | ★★★★ 流畅 |

720p 结论：**CPU 版和 RGA 版差异极小。** CPU 整数定点在 720p 下每帧只要 ~15ms，足够快。
RGA 反而因为 cache flush + ioctl 上下文切换 + `.copy()`，体感略不如 CPU 版流畅。

#### 1080p（1920×1080）—— 有 15fps 限速

| 配置 | 摄像头关 | 摄像头开 | 增量 |
|------|---------|---------|------|
| CPU 整数定点 | ~9% | ~25% | +16% |
| RGA 硬件加速 | ~9% | ~22% | +13% |

1080p 结论：RGA 开始体现价值，CPU 占用降低约 3%。

#### 4K（3840×2160）—— 有 15fps 限速

| 配置 | 摄像头关 | 摄像头开 | 增量 | 延迟 |
|------|---------|---------|------|------|
| CPU 整数定点 | ~9% | ~42% | +33% | 明显 |
| RGA 硬件加速 | ~9% | ~32% | +23% | 也明显 |

4K 结论：**RGA 省了 ~10% CPU**（33%→23%），但延迟改善不明显。原因是瓶颈不在色彩转换：

### 4K 下 RGA 效果有限的瓶颈分析

```
4K 帧处理链耗时分析（3840×2160）：

步骤                    RGA 版          CPU 版           说明
─────────────────────────────────────────────────────────────────
① NV12→RGB 转换        ~2ms ✅         ~150ms          RGA 确实快了 75 倍
② .copy() 深拷贝       ~12ms ❌        0ms             24.8MB memcpy！（RGA 独有开销）
③ UI scaled() 缩放     ~30-50ms ❌     ~30-50ms ❌     CPU 缩放 24.8MB→430×242
④ 15fps 限速           66ms            66ms            QElapsedTimer 限速
─────────────────────────────────────────────────────────────────
总耗时                  ~45-65ms        ~150ms+         但都受 66ms 限速约束
```

RGA 优化了步骤①，但步骤②③才是 4K 下的真正瓶颈：
- `.copy()`：3840×2160×3 = **24.8MB** memcpy，每帧 ~12ms
- `scaled()`：UI 线程 CPU 缩放 24.8MB 到 430×242，每帧 ~30-50ms
- 两者加起来 ~50ms，和省掉的 ~148ms CPU 转换比，"搬运+缩放"成了新瓶颈

**根本解法**：用 RGA 的 `imresize()` 同时做色彩转换+缩放，输出直接就是 cameraLabel 尺寸。
这样 `.copy()` 从 24.8MB 降到 ~300KB，`scaled()` 可以完全去掉。但这需要重构显示逻辑。

### 最终选择

当前项目使用 **1280×720 + CPU 整数定点**版本（`USE_RGA` 注释掉），理由：
1. 720p 下 CPU 版和 RGA 版性能几乎一样（都是 +10% 增量）
2. 1080p 下 RGA 仅省 3% CPU（25%→22%），但延迟体感反而略高（cache flush + ioctl + `.copy()` 开销）
3. CPU 版代码更简单，无 librga 依赖，无 `.copy()` 开销
4. CPU 版帧间隔更稳定（纯计算，无 ioctl/cache flush 抖动）
5. RGA 的价值要在"RGA 同时做缩放"（`imresize` + `imcvtcolor` 合并）的完整方案下才能充分体现，
   单独只替换色彩转换这一步，收益被 `.copy()` 和 `scaled()` 的 CPU 开销抵消

**RGA 学习价值总结**：整个 RGA 集成过程加深了对 Rockchip 2D 加速器、DMA buffer 管理、
cache 一致性问题的理解，但在当前分辨率（720p/1080p）和场景下，CPU 整数定点已经够用，
不值得为了微小的 CPU 占用差异引入额外复杂度。


## 模块八：4K 模式 ISP 调优

### 问题

4K 模式下噪点明显比 720p/1080p 多。这是物理特性：
- 720p 时 ISP 做 binning/下采样，~9个像素合并为1个，噪声被 √9=3 倍抑制
- 4K 时几乎 1:1 映射原始像素，每个像素的读出噪声都保留
- 暗光时 rkaiq_3A_server 自动拉高 gain（增益），4K + 高 gain = 噪点爆炸

### rkaiq_3A_server 启动方式

rkaiq_3A_server 是**系统服务自动启动**的，不需要手动运行：

```bash
# 系统服务配置
/usr/lib/systemd/system/rkaiq_3A.service
/etc/init.d/rkaiq_3A.sh

# 验证运行状态
systemctl status rkaiq_3A
ps aux | grep rkaiq
# → /usr/bin/rkaiq_3A_server（PID 374，开机自启）
```

所以直接 `./VehicleTerminal` 就行，不需要先 `sudo rkaiq_3A_server &`。

### IMX415 sensor 可调参数

通过 `/dev/v4l-subdev2`（IMX415 sensor subdev）查询：

```bash
v4l2-ctl -d /dev/v4l-subdev2 --list-ctrls
```

| 参数 | 范围 | 默认 | 说明 |
|------|------|------|------|
| exposure | 4~2242 | 2242 | 曝光行数（rkaiq 自动控制） |
| analogue_gain | 0~240 | 0 | 模拟增益（rkaiq 自动控制，值越大噪点越多） |
| vertical_blanking | 58~30575 | 58 | 帧间隔行数（**不受 rkaiq 控制，可安全手动设**） |
| horizontal_flip | 0/1 | 0 | 水平翻转 |
| vertical_flip | 0/1 | 0 | 垂直翻转 |
| link_frequency | 0~4 | 0 | MIPI 链路频率（297/446/743/891/1188 MHz） |

### 调优策略

**核心思路**：增大 vblank → 降低帧率 → 每帧更多曝光时间 → 3A 不需要拉高 gain → 噪点降低

```
vblank=58（默认）    → 4K@30fps，暗处 gain 拉满，噪点多
vblank=500           → 4K@~25fps，3A 有更多曝光余量，gain 更低
vblank=2000          → 4K@~15fps，曝光充足，噪点明显减少
```

**为什么不直接设 exposure/gain？** 因为 rkaiq_3A_server 每帧都在自动调 exposure 和 gain，
你手动设的值会被下一帧立刻覆盖。但 **vblank 不受 3A 控制**，手动设了就生效。

### 实现代码

在 `camera_thread.h` 中定义 sensor subdev 路径：
```cpp
#define SENSOR_DEV  "/dev/v4l-subdev2"   // IMX415 sensor subdev
```

在 `camera_thread.cpp` 中添加 `tuneFor4K()` 函数，`initDevice()` 末尾自动调用：
```cpp
// initDevice() 末尾：
if (WIDTH >= 3840) {
    tuneFor4K();
}

void CameraThread::tuneFor4K()
{
    int sfd = open(SENSOR_DEV, O_RDWR);
    ...
    struct v4l2_control ctrl;
    ctrl.id    = V4L2_CID_VBLANK;
    ctrl.value = 500;  // ★ 调试参数：可在 58~30575 范围内调整 ★
    ioctl(sfd, VIDIOC_S_CTRL, &ctrl);
    close(sfd);
}
```

### 调试方法

编译后运行，通过 qDebug 输出确认参数生效：
```
tuneFor4K: vblank = 500
tuneFor4K: 4K tuning applied
```

调试时修改 `ctrl.value` 的值，重新编译观察画面噪点变化：
- `vblank = 58`：默认，帧率最高，噪点最多
- `vblank = 500`：帧率降到 ~25fps，噪点适中（推荐起点）
- `vblank = 2000`：帧率降到 ~15fps，噪点少但画面不如之前流畅
- `vblank = 5000+`：帧率 <10fps，噪点很少但卡顿明显

找到帧率 vs 画质的平衡点后，把值固化在代码里。








这不是代码任务，直接给你面试回答框架。

---

## 面试回答模板："Camera 部分做了什么"

### 30秒版（先总后分）

> "在 RK3588S 平台上，用 V4L2 MPLANE API 驱动 IMX415 摄像头，实现了从 sensor 采集到 Qt 界面显示的完整链路。核心工作是 **mmap 零拷贝 buffer 管理**、**NV12→RGB 整数定点色彩转换**、以及**多线程生产者-消费者架构**保证 UI 不卡顿。"

### 面试官追问时，按层次展开

**第一层：数据链路（体现你懂全貌）**

> "数据流是：IMX415 出 10-bit RAW → MIPI CSI-2 四车道传入 → RKISP 硬件做去马赛克、AWB、AE、降噪、锐化等十步处理 → 输出 NV12 格式到 DMA buffer → 我的程序通过 mmap 零拷贝读取 → 软件做 NV12→RGB888 转换 → 信号发到 UI 线程显示。"

**第二层：关键优化（体现你有性能意识）**

> "色彩转换最初用浮点运算，每帧 60ms+，后来用**整数定点替代**——把 1.402 放大 256 倍变成 359，乘完右移 8 位，全程只有整数乘法和位移，降到 ~15ms。ARM 上浮点要走 FPU 流水线切换，整数直接在 ALU，92 万像素每个省 3 次浮点，差距很大。"

**第三层：调试经验（体现你能解决问题）**

> "遇到过两个典型问题：
> 1. **OOM Killed**——最初用 `QImage::setPixel` 逐像素写入，207 万次函数调用导致内存爆到 3GB，改成 `scanLine` 直接拿行指针写内存，降到几十 MB。
> 2. **画面模糊**——排查了分辨率、缩放算法、stride 对齐、ISP 参数都没用，最后用 **Laplacian 方差**量化清晰度，测到只有 94（正常 >500），确认是**镜头物理失焦**，手动旋转对焦后解决。这个经历让我明白不要用软件补偿硬件问题。"

**第四层：架构设计（体现你懂工程）**

> "采集线程和 UI 线程完全解耦，是标准的**生产者-消费者模型**。采集线程只负责 DQBUF + 转换 + emit 信号，UI 线程只负责 setPixmap 显示。中间靠 Qt 信号-槽的跨线程队列保证线程安全，QImage 用 Copy-on-Write 隐式共享，信号传递不真正拷贝数据。采集线程用 `volatile bool running` 控制退出，DQBUF 本身是阻塞调用所以不需要额外 sleep 限速。"

---

### 高频追问 & 参考答案

| 问题 | 关键词回答 |
|------|-----------|
| 为什么用 NV12 不用 RGB？ | 带宽省一半（1.5 vs 3 字节/像素），YUV 利用人眼对亮度比色彩敏感的特性，UV 分辨率减半 |
| mmap 比 read 好在哪？ | 零拷贝——同一块物理内存映射两个虚拟地址，ISP 写完用户直接读，无 memcpy |
| 为什么 4 个 buffer？ | 环形队列缓冲——1 个你在处理，硬件可以同时填其他 3 个，避免丢帧 |
| DQBUF 阻塞是什么意思？ | 内核里的等待队列，硬件填满一帧触发中断，唤醒 DQBUF 返回。没帧时线程休眠不占 CPU |
| volatile 够吗？不用互斥锁？ | 这里只有一个布尔标志、单写单读，volatile 防编译器优化到寄存器就够。复杂共享数据才需要 mutex |
| 为什么不在采集线程直接操作 UI？ | Qt 规则：所有 UI 操作必须在主线程。跨线程操作 QWidget 会崩 |

---

### 不建议讲的

- **RGA**：你自己实测结论是 720p/1080p 下收益不明显，讲了容易被追问"为什么用了效果不好"然后陷入被动。除非面试官主动问"了解 RGA 吗"，再简要提"做过对比测试，720p 下 CPU 整数定点已经够快，RGA 的 cache flush + ioctl 开销抵消了转换省下的时间"。
- **4K 调优**：vblank 调优太细节，面试时间有限，不是亮点。

### 建议主打的亮点排序

1. **Laplacian 方差调试故事** — 有方法论、有数据、有反思（"不要用软件补偿硬件"），面试官印象最深
2. **整数定点优化** — 展示嵌入式性能意识，能讲清原理（×256 再 >>8）
3. **V4L2 全链路理解** — 从 sensor 到屏幕每一层都能说清楚，体现系统级思维
4. **setPixel→scanLine OOM 修复** — 说明你知道 API 背后的成本