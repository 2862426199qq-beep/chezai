# DRM/KMS 驱动学习笔记

> 平台：RK3588S (鲁班猫4) | 内核版本：Linux 6.1 | 显示：MIPI DSI

---

## 目录

1. [DRM/KMS 框架概述](#1-drmkms-框架概述)
2. [核心概念与对象模型](#2-核心概念与对象模型)
3. [驱动初始化流程](#3-驱动初始化流程)
4. [帧缓冲与 GEM 内存管理](#4-帧缓冲与-gem-内存管理)
5. [用户态 libdrm 接口](#5-用户态-libdrm-接口)
6. [MIPI DSI 接入要点（RK3588S）](#6-mipi-dsi-接入要点rk3588s)
7. [调试技巧](#7-调试技巧)
8. [参考资料](#8-参考资料)
9. [TODO / 待研究](#9-todo--待研究)

---

## 1. DRM/KMS 框架概述

DRM (Direct Rendering Manager) / KMS (Kernel Mode Setting) 是 Linux 现代显示子系统，替代了传统的 fbdev。

```
用户态应用 / Qt (EGLFS / LinuxFB)
    │  libdrm / DRM ioctl
    ▼
/dev/dri/card0
    │
DRM 核心层 (drivers/gpu/drm/)
    │  drm_device / drm_crtc / drm_plane / drm_connector / drm_encoder
    ▼
厂商驱动 (drivers/gpu/drm/rockchip/)
    │  rockchip_drm_vop2.c / dw-mipi-dsi-rockchip.c
    ▼
硬件：VOP2 (视频输出处理器) → MIPI DSI 屏幕
```

---

## 2. 核心概念与对象模型

### 2.1 KMS 对象层次

```
drm_device
  └── drm_crtc        (显示控制器，对应 VOP2 通道)
        ├── drm_plane (图层：PRIMARY / OVERLAY / CURSOR)
        ├── drm_encoder (编码器：MIPI DSI / HDMI / eDP)
        └── drm_connector (连接器：DSI-1 / HDMI-A-1)
```

### 2.2 核心概念说明

| 对象 | 说明 | RK3588S 对应 |
|------|------|-------------|
| CRTC | 扫描控制器，读取帧缓冲输出时序 | VOP2 (支持4个CRTC) |
| Plane | 硬件图层（支持叠加、缩放） | VOP2 各 Win 窗口 |
| Encoder | 信号编码（数字→模拟/MIPI等） | DW-MIPI-DSI |
| Connector | 物理接口 | MIPI DSI 连接器 |
| Framebuffer | GEM BO 封装的显存 | — |

---

## 3. 驱动初始化流程

```
platform_driver.probe()
  ├── drm_dev_alloc()                 // 分配 drm_device
  ├── rockchip_drm_init_iommu()       // IOMMU 映射
  ├── component_bind_all()            // 绑定子组件（VOP2, MIPI DSI 等）
  │     ├── vop2_bind()               // 注册 CRTC + Plane
  │     └── dw_mipi_dsi_bind()        // 注册 Encoder + Connector
  ├── drm_mode_config_init()          // 初始化显示模式配置
  └── drm_dev_register()              // 注册 /dev/dri/card0
```

---

## 4. 帧缓冲与 GEM 内存管理

### 4.1 GEM (Graphics Execution Manager)

```c
// 分配连续物理内存（DMA-BUF）
struct drm_gem_cma_object *bo = drm_gem_cma_create(dev, size);

// 导出为 DMA-BUF fd（可与 V4L2 摄像头共享）
int dmabuf_fd = drm_gem_prime_handle_to_fd(dev, file, handle, 0);
```

### 4.2 Atomic Modesetting（推荐）

```c
// 原子提交：同时更新多个对象，避免撕裂
drmModeAtomicReqPtr req = drmModeAtomicAlloc();
drmModeAtomicAddProperty(req, crtc_id, ACTIVE_prop, 1);
drmModeAtomicAddProperty(req, plane_id, FB_ID_prop, fb_id);
drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
```

---

## 5. 用户态 libdrm 接口

### 5.1 最小可用显示程序框架

```c
#include <xf86drm.h>
#include <xf86drmMode.h>

int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

// 获取资源
drmModeRes *res = drmModeGetResources(fd);
drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[0]);

// 选择显示模式
drmModeModeInfo *mode = &conn->modes[0];  // 第一个（通常最高分辨率）

// 创建 dumb buffer（内核分配）
struct drm_mode_create_dumb creq = {
    .width  = mode->hdisplay,
    .height = mode->vdisplay,
    .bpp    = 32,
};
drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);

// mmap 到用户态，直接写像素
// ...
```

### 5.2 Qt 的 DRM/KMS 后端

Qt 在嵌入式 Linux 上通过 EGLFS 平台插件使用 DRM/KMS：

```bash
# 运行 Qt 应用时指定 KMS 后端
QT_QPA_PLATFORM=eglfs ./VehicleTerminal

# 或使用 linuxfb（不需要 GPU）
QT_QPA_PLATFORM=linuxfb ./VehicleTerminal
```

---

## 6. MIPI DSI 接入要点（RK3588S）

### 6.1 设备树配置

```dts
/* TODO: 补充具体屏幕型号的设备树节点 */
&dsi0 {
    status = "okay";
    panel@0 {
        compatible = "your-panel-vendor,model";
        reg = <0>;
        /* 时序参数 */
        display-timings {
            native-mode = <&timing0>;
            timing0: timing0 {
                clock-frequency = <148500000>;
                hactive = <1920>;
                vactive = <1080>;
                /* hfp, hbp, hsync, vfp, vbp, vsync */
            };
        };
    };
};
```

### 6.2 常见问题

| 问题 | 可能原因 | 解决方向 |
|------|----------|----------|
| 屏幕无输出 | 时序参数错误 / 驱动未加载 | `dmesg | grep drm` |
| 画面颜色异常 | 像素格式不匹配 | 检查 `DRM_FORMAT_XRGB8888` 等 |
| 卡顿/撕裂 | 未使用 vsync | 使用 Atomic Commit + DRM_MODE_PAGE_FLIP_EVENT |

---

## 7. 调试技巧

```bash
# 查看 DRM 设备
ls /dev/dri/

# 查看连接器状态
modetest -M rockchip

# 打印 DRM 状态（内核）
cat /sys/kernel/debug/dri/0/state

# 查看 VOP2 寄存器
cat /sys/kernel/debug/dri/0/vop_regs

# 内核日志
dmesg | grep -i "drm\|vop\|dsi\|mipi"
```

---

## 8. 参考资料

- [Linux DRM 内核文档](https://www.kernel.org/doc/html/latest/gpu/index.html)
- [Rockchip DRM 驱动源码](https://github.com/torvalds/linux/tree/master/drivers/gpu/drm/rockchip)
- [libdrm 示例](https://github.com/dvdhrm/docs/tree/master/drm-howto)
- [Qt EGLFS 文档](https://doc.qt.io/qt-5/embedded-linux.html)

---

## 9. TODO / 待研究

- [ ] 确认鲁班猫4 MIPI DSI 屏幕型号，获取初始化序列
- [ ] 在设备树中启用 DSI0 并配置屏幕面板驱动
- [ ] 测试 Qt EGLFS 后端在 RK3588S 上的运行
- [ ] DRM Atomic Modesetting + 双缓冲（避免撕裂）
- [ ] DMA-BUF 共享：摄像头帧 → DRM Overlay Plane（零拷贝显示）
- [ ] RK3588S NPU + DRM Overlay 用于 AI 检测结果叠加显示
