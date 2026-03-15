# KEY 按键驱动开发教程 (IIO + Input 子系统)

## 硬件信息

- 板子: LubanCat-4 (RK3588S)
- 按键: **Recovery 按键**，接 SARADC 通道 1
- SARADC: `rockchip,rk3588-saradc`，12-bit，IIO 驱动已内建 (CONFIG_ROCKCHIP_SARADC=y)
- 按键未按下: ADC ≈ 4090 (接上拉到 VRef)
- 按键按下: ADC ≈ 0 (拉到 GND)

### 三个按键分析

| 按键 | 接口 | Linux 中状态 |
|------|------|-------------|
| 电源键 | RK806 PMIC (SPI) | 系统占用 — 关机/休眠 |
| **Recovery键** | **SARADC 通道1** | **可用！Linux 启动后无人使用** |
| MaskRom键 | 直连 SoC eFuse | 不可用 — 纯硬件级 |

## 设计思路

### 为什么不用 ioremap 直接操作 SARADC 寄存器？

之前尝试 ioremap + 直接读 SARADC 寄存器，踩了坑（寄存器偏移不确定、时钟/中断配置不对导致读出来全是 0）。
这是**绕过框架**的野路子——面试时不加分，反而暴露工程意识不足。

### 正确做法：使用两个 Linux 子系统

1. **IIO consumer API** — 在内核中通过 `iio_channel_get()` + `iio_read_channel_raw()` 读 ADC
   - IIO 驱动已经帮我们处理好了寄存器操作、时钟使能、转换触发、数据读取
   - 我们只需要当"消费者"调用 API 即可
   
2. **Input 子系统** — 通过 `input_report_key()` 上报按键事件到 `/dev/input/eventX`
   - 和野火教程的方式一致
   - 用户空间用 `read(fd, &ev, sizeof(ev))` 读取 `struct input_event`
   - 所有输入设备（按键、触摸屏、鼠标）统一接口

### 新知识点（相比 DHT11/LED）

| 知识点 | DHT11/LED | KEY 驱动 |
|--------|-----------|---------|
| 硬件接口 | GPIO (直接操作) | **IIO consumer API** (调内核框架) |
| 设备模型 | misc device | **Input 子系统** (input_dev) |
| 轮询机制 | 用户触发 | **内核工作队列** (delayed_work) |
| 事件模型 | 无 | **input_report_key** (标准按键事件) |
| 用户接口 | /dev/xxx (自定义 read/write) | **/dev/input/eventX** (标准 input_event) || 设备树 | 无 (硬编码 GPIO 号) | **设备树 Overlay** (动态加载) |
| 驱动框架 | module_init/exit | **platform_driver** (probe/remove) |
| 资源管理 | 手动 alloc/free | **devm_ 托管** (自动释放) |
---

## Input 子系统知识点 (面试高频考点)

### 三层架构

Linux Input 子系统分为三层，所有输入设备（键盘、鼠标、触摸屏、按键）统一管理：

```
┌──────────────────────────────────────────────────┐
│                  用户空间                          │
│   read(/dev/input/eventX) → struct input_event   │
├──────────────────────────────────────────────────┤
│              Handlers (事件处理层)                  │
│   evdev.c — 将事件打包为 input_event 供用户读取    │
│   内核已内建，不需要我们写                           │
├──────────────────────────────────────────────────┤
│              Input Core (核心层)                   │
│   input.c — 承上启下                              │
│   提供 input_register_device / input_report_key   │
│   负责将驱动层的事件分发给 handler 层               │
│   内核已内建，不需要我们写                           │
├──────────────────────────────────────────────────┤
│              Drivers (驱动层) ← 我们写这一层        │
│   负责读硬件 + 上报事件                             │
│   野火: GPIO 中断触发 → input_report_key           │
│   我们: ADC 轮询触发 → input_report_key            │
└──────────────────────────────────────────────────┘
```

> **面试怎么说**: "Input 子系统三层分离 — 我只负责驱动层，读 ADC 判断按键状态后调 `input_report_key` 上报；核心层自动将事件分发给 evdev handler；用户空间通过 `/dev/input/eventX` 用标准 `struct input_event` 读取。三层解耦，驱动不关心用户怎么读，用户不关心硬件怎么触发。"

### 核心数据结构

#### 1. `struct input_dev` — 代表一个输入设备 (内核空间)

```c
// 简化版，完整定义在 include/linux/input.h
struct input_dev {
    const char *name;       // 设备名，出现在 /proc/bus/input/devices
    const char *phys;       // 物理路径，给开发者看的

    unsigned long evbit[];  // 支持的事件类型位图 (EV_KEY, EV_ABS, ...)
    unsigned long keybit[]; // 支持的键值位图 (KEY_ENTER, BTN_0, ...)
    unsigned long relbit[]; // 支持的相对坐标位图 (鼠标用)
    unsigned long absbit[]; // 支持的绝对坐标位图 (触摸屏用)
    // ...
};
```

按键驱动只需要设置 `evbit` 和 `keybit` 两个：
- `evbit` 设置 `EV_KEY`（按键事件类型）
- `keybit` 设置具体键值（如 `KEY_ENTER`、`BTN_0`）

#### 2. `struct input_event` — 用户空间读到的事件 (用户空间)

```c
// include/uapi/linux/input.h
struct input_event {
    struct timeval time;  // 事件产生的时间戳 (内核自动填)
    __u16 type;           // 事件类型: EV_KEY=1, EV_SYN=0, ...
    __u16 code;           // 键值: KEY_ENTER=28, BTN_0=0x100, ...
    __s32 value;          // 按键: 1=按下, 0=松开, 2=长按重复
};
```

> type 和 code 的关系: type 决定大类（按键/鼠标/触摸屏），code 决定具体是哪个键

### 常用 API 速查

| 函数 | 作用 | 我们在哪用 |
|------|------|-----------|
| `input_allocate_device()` | 申请 input_dev | key_init |
| `input_set_capability(dev, EV_KEY, KEY_ENTER)` | 标记支持的事件和键值 | key_init |
| `input_register_device(dev)` | 注册到 Input Core | key_init |
| `input_report_key(dev, code, value)` | 上报按键事件 | key_poll_func |
| `input_sync(dev)` | 标记一组事件上报结束 | key_poll_func |
| `input_unregister_device(dev)` | 注销 (内部自动 free) | key_exit |
| `input_free_device(dev)` | 释放 (仅注册失败时调用) | key_init 错误路径 |

> **注意**: 注册成功后，卸载时只需调 `input_unregister_device`，**不要**再调 `input_free_device`（内部已释放）。野火教程 remove 函数里两个都调了，其实 `input_free_device` 那行是多余的。

### 事件类型速查 (EV_xxx)

| 宏定义 | 值 | 含义 | 典型设备 |
|--------|-----|------|---------|
| `EV_SYN` | 0x00 | 同步事件 | 所有设备 (每组事件结束后发) |
| `EV_KEY` | 0x01 | 按键事件 | 键盘、按钮、按键 ← 我们用这个 |
| `EV_REL` | 0x02 | 相对坐标 | 鼠标移动 |
| `EV_ABS` | 0x03 | 绝对坐标 | 触摸屏触点 |

### 与野火教程 (H618/LubanCat) 的对比

野火 H618 篇的 Input 实验用的是 **GPIO 中断**方式：

```
野火方案:
  设备树插件(.dts) → platform_driver(probe) → gpiod_get → gpiod_to_irq
  → request_irq → 中断触发 → gpiod_get_value → input_report_key

我们的方案:
  设备树 Overlay(.dts) → platform_driver(probe) → devm_iio_channel_get
  → delayed_work 轮询 → iio_read_channel_raw → 阈值判断 → input_report_key
```

| 维度 | 野火 (GPIO 中断) | 我们 (ADC 轮询) |
|------|-----------------|----------------|
| 触发机制 | 硬件中断 (IRQ) — 即时响应 | delayed_work 轮询 — 50ms 周期 |
| 设备树 | 需要写 `.dts` 插件并重启 | **同样写 `.dts` Overlay** |
| 驱动框架 | platform_driver (probe/remove) | **同样用 platform_driver** |
| GPIO 操作 | `gpiod_get` + `gpiod_get_value` | 不涉及 GPIO |
| ADC 操作 | 不涉及 ADC | **devm_iio_channel_get** (IIO consumer API) |
| 资源管理 | 手动 alloc/free | **devm_ 托管** (自动释放) |
| 额外知识 | 中断子系统 | **IIO 子系统 + 工作队列** |
| 适用场景 | GPIO 按键 (有中断线) | ADC 按键 (无中断线) |
| 用户测试 | 硬编码 `/dev/input/event1` | **自动发现设备名** |

> **核心思想完全一致**: 两种方案框架一样 — 都写设备树 Overlay + platform_driver + Input 子系统上报。区别只在于触发方式（GPIO 中断 vs ADC 轮询）和底层 API（gpiod vs IIO）。
>
> **我们方案的面试优势**: 同时涉及 **设备树 Overlay + platform_driver + IIO consumer + Input 子系统 + 工作队列 + devm 资源管理** 六个知识点。

---

## 第一步：验证硬件

先确认 Recovery 按键确实能通过 IIO 读到：

```bash
# 未按下时读 (期望 ~4090)
cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw

# 按住 Recovery 按键的同时读 (期望 ~0)
cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw
```

如果按下时值从 ~4090 变到 ~0，说明硬件没问题，继续。

---

## 第二步：编写设备树 Overlay

### 设备树 Overlay 原理

设备树 Overlay（设备树插件）可以在**不修改主设备树**的前提下，动态添加新的设备节点。
就像野火教程里给 GPIO 按键写的 overlay 一样，我们也要给 ADC 按键写一个。

我们需要告诉内核："有一个设备叫 `adc-key`，它使用 SARADC 的通道 1"。

### 确认前提

```bash
# 确认主 DTB 导出了 saradc 符号 (overlay 需要引用 &saradc)
cat /proc/device-tree/__symbols__/saradc
# → /saradc@fec10000

# 确认 SARADC 支持 io-channels 引用
xxd -p /proc/device-tree/saradc@fec10000/'#io-channel-cells'
# → 00000001  (= 1，说明引用时需要一个参数: 通道号)

# 确认没有其他消费者在用通道 1
find /proc/device-tree/ -name "io-channels" 2>/dev/null
# → 只有 es8388-sound (音频)，没有占用通道 1
```

### 编写 Overlay 源码

创建 `~/chezai/driver/key/rk3588s-lubancat-4-adc-key-overlay.dts`：

```dts
/*
 * rk3588s-lubancat-4-adc-key-overlay.dts
 *
 * 设备树 Overlay — 在根节点下添加 ADC 按键设备节点
 *
 * 作用:
 *   告诉内核 "有一个设备叫 adc-key，它使用 SARADC 通道 1"
 *   当 key_drv.ko 加载时，内核通过 compatible 字符串匹配到这个节点，
 *   自动调用 key_probe()，probe 中通过 io-channels 属性获取 IIO 通道
 *
 * 对比野火教程:
 *   野火: button-gpios = <&pio PH 9 GPIO_ACTIVE_HIGH>;  ← GPIO 引用
 *   我们: io-channels = <&saradc 1>;                     ← IIO 通道引用
 *
 * 格式说明:
 *   /dts-v1/;  — 设备树版本声明
 *   /plugin/;  — 标记这是一个 overlay (不是完整设备树)
 *   fragment@0 — overlay 片段，target-path="/" 表示挂载到根节点
 *   __overlay__ — 里面的内容会被合并到目标节点
 */
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            adc-key {
                compatible = "recovery-adc-key";
                /*
                 * io-channels: 引用 SARADC 的通道 1
                 *
                 * &saradc: 引用主设备树中的 saradc 节点
                 *          (label = "saradc", 地址 fec10000)
                 * 1:       通道号 (#io-channel-cells = <1>，所以需要 1 个参数)
                 *
                 * 内核解析后会关联到 iio:device0 的 channel 1
                 * 驱动中调 devm_iio_channel_get(&pdev->dev, "buttons")
                 * 就能拿到这个通道的句柄
                 */
                io-channels = <&saradc 1>;
                io-channel-names = "buttons";
                status = "okay";
            };
        };
    };
};
```

### 编译 Overlay

```bash
cd ~/chezai/driver/key

# 编译 .dts → .dtbo
dtc -@ -I dts -O dtb -o rk3588s-lubancat-4-adc-key-overlay.dtbo \
    rk3588s-lubancat-4-adc-key-overlay.dts

# -@ : 生成 __symbols__，允许被其他 overlay 引用
# -I dts : 输入格式是源码
# -O dtb : 输出格式是二进制
```

### 部署 Overlay

```bash
# 1. 拷贝到 overlay 目录
sudo cp rk3588s-lubancat-4-adc-key-overlay.dtbo /boot/dtb/overlay/

# 2. 添加到 uEnv 配置 (在 #overlay_end 之前加一行)
sudo nano /boot/uEnv/uEnvLubanCat4.txt
# 在 #overlay_end 之前添加:
# dtoverlay=/dtb/overlay/rk3588s-lubancat-4-adc-key-overlay.dtbo

# 3. 重启使 overlay 生效
sudo reboot
```

### 验证 Overlay 加载成功

```bash
# 重启后检查设备树中是否有 adc-key 节点
ls /proc/device-tree/adc-key/
# → compatible  io-channel-names  io-channels  name  status

# 确认 compatible 值
cat /proc/device-tree/adc-key/compatible
# → recovery-adc-key
```

---

## 第三步：编写 KEY 驱动代码

创建 `~/chezai/driver/key/key_drv.c`：

```c
/*
 * key_drv.c — ADC 按键驱动 (设备树 + platform_driver + IIO consumer + Input 子系统)
 *
 * 硬件: LubanCat-4 Recovery 按键 → SARADC 通道 1
 * 接口: /dev/input/eventX (Linux Input 子系统标准接口)
 *
 * 架构:
 *
 *   设备树 Overlay                    platform_driver
 *   ┌──────────────┐                 ┌───────────────┐
 *   │ adc-key 节点  │ ── compatible ──→│  key_probe()  │
 *   │ io-channels  │                 │               │
 *   │ = <&saradc 1>│                 │  iio_channel  │
 *   └──────────────┘                 │  + input_dev  │
 *                                    │  + delayed_   │
 *         ┌─────────────┐           │    work       │
 *         │  SARADC 硬件  │←── IIO ──│               │
 *         └─────────────┘           └───────┬───────┘
 *                                           │
 *                                    /dev/input/eventX
 *
 * 对比野火教程 (H618):
 *   野火: 设备树(.dts) → platform_driver → GPIO 中断 → input_report_key
 *   我们: 设备树(.dts) → platform_driver → IIO ADC 轮询 → input_report_key
 *   相同: 都用设备树 + platform_driver + Input 子系统
 *   不同: 野火用 GPIO 中断触发，我们用 delayed_work 轮询 (ADC 没有按键中断线)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>            /* input_dev, input_report_key */
#include <linux/iio/consumer.h>     /* devm_iio_channel_get, iio_read_channel_raw */
#include <linux/workqueue.h>        /* delayed_work */
#include <linux/of.h>               /* of_device_id, of_match_table */

#define ADC_THRESHOLD       2000
#define POLL_INTERVAL_MS    50

/*
 * 驱动私有数据结构
 *
 * 和野火教程的 button_data 类似，用 devm_kzalloc 在 probe 中分配。
 * 对比 DHT11/LED 用全局变量: platform_driver 推荐用私有数据结构，
 * 因为理论上一个驱动可能管理多个设备实例。
 *
 * container_of 面试考点:
 *   在 work 回调中，通过 container_of(work, struct key_data, poll_work.work)
 *   从 work 指针反推出包含它的 key_data 结构体 — Linux 内核最常见的技巧之一
 */
struct key_data {
    struct iio_channel *adc_channel;    /* IIO 通道句柄 */
    struct input_dev *input_dev;        /* Input 设备 */
    struct delayed_work poll_work;      /* 轮询工作队列 */
    int key_pressed;                    /* 当前按键状态 */
};

/*
 * ========== 轮询工作函数 ==========
 *
 * 每 50ms 执行一次:
 *   1. 通过 IIO consumer API 读 ADC 值
 *   2. 阈值判断按下/松开
 *   3. 状态变化时通过 Input 子系统上报事件
 *   4. 重新调度自己
 *
 * 对比野火教程:
 *   野火: button_irq_handler() 中断触发 → input_report_key
 *   我们: key_poll_func() 定时轮询 → input_report_key
 *
 * 为什么用轮询不用中断？
 *   Recovery 按键接的是 SARADC，不是 GPIO，没有"电压变化"中断线。
 *   SARADC 中断只在"转换完成"时触发，不会在按键按下时主动中断。
 *   轮询 50ms 对按键来说完全够用 (人手 100~200ms 一次操作)
 */
static void key_poll_func(struct work_struct *work)
{
    struct key_data *kd = container_of(work, struct key_data, poll_work.work);
    int adc_val = 0;
    int pressed;
    int ret;

    /*
     * ★ 核心: 一行代码读 ADC — 正规的 IIO consumer API ★
     *
     * devm_iio_channel_get() 在 probe 时已经通过设备树的
     * io-channels = <&saradc 1> 获取了通道句柄，
     * 这里直接调 iio_read_channel_raw() 即可。
     *
     * 对比之前的 filp_open 方案:
     *   之前: filp_open("/sys/.../in_voltage1_raw") → kernel_read → kstrtoint
     *   现在: iio_read_channel_raw(kd->adc_channel, &adc_val) — 一行搞定
     */
    ret = iio_read_channel_raw(kd->adc_channel, &adc_val);
    if (ret < 0)
        goto reschedule;

    pressed = (adc_val < ADC_THRESHOLD) ? 1 : 0;

    /* 边沿检测: 状态变化时才上报 */
    if (pressed != kd->key_pressed) {
        kd->key_pressed = pressed;

        /*
         * Input 子系统上报 — 和野火教程完全一致:
         *   input_report_key(dev, 键值, 1/0);
         *   input_sync(dev);
         */
        input_report_key(kd->input_dev, KEY_ENTER, pressed);
        input_sync(kd->input_dev);

        pr_info("key: %s (ADC=%d)\n",
                pressed ? "PRESSED" : "RELEASED", adc_val);
    }

reschedule:
    schedule_delayed_work(&kd->poll_work,
                          msecs_to_jiffies(POLL_INTERVAL_MS));
}

/*
 * ========== probe 函数 ==========
 *
 * 当内核发现设备树中有节点的 compatible 和我们的 of_match_table 匹配时，
 * 自动调用 probe 函数。这是 "设备和驱动分离" 的核心体现。
 *
 * 对比:
 *   DHT11/LED (module_init): 手动硬编码 GPIO 号，没有设备树绑定
 *   野火 (platform_driver):  probe 中用 gpiod_get + request_irq
 *   我们 (platform_driver):  probe 中用 devm_iio_channel_get + delayed_work
 *
 * devm_ 系列函数: device-managed 资源管理
 *   好处: remove 时自动释放，不需要手动 free/unregister
 *   面试说: "使用 devm 托管资源，避免忘记释放导致内存泄漏"
 */
static int key_probe(struct platform_device *pdev)
{
    struct key_data *kd;
    int ret;

    /* 1. 分配私有数据 — devm 版本，remove 时自动释放 */
    kd = devm_kzalloc(&pdev->dev, sizeof(*kd), GFP_KERNEL);
    if (!kd)
        return -ENOMEM;

    /*
     * 2. 获取 IIO 通道 — 通过设备树 io-channels 属性
     *
     * devm_iio_channel_get 参数:
     *   &pdev->dev: 我们的 platform_device
     *   "buttons":  对应设备树的 io-channel-names
     *
     * 内核会自动:
     *   → 解析设备树 io-channels = <&saradc 1>
     *   → 找到 saradc IIO provider
     *   → 返回通道 1 的 iio_channel 句柄
     *
     * 对比之前的 filp_open 方案:
     *   之前: filp_open("/sys/.../in_voltage1_raw") → kernel_read → kstrtoint
     *   现在: devm_iio_channel_get() 一行搞定
     */
    kd->adc_channel = devm_iio_channel_get(&pdev->dev, "buttons");
    if (IS_ERR(kd->adc_channel)) {
        dev_err(&pdev->dev, "failed to get IIO channel: %ld\n",
                PTR_ERR(kd->adc_channel));
        return PTR_ERR(kd->adc_channel);
    }

    /*
     * 3. 注册 Input 设备 — devm 版本
     *
     * devm_input_allocate_device: remove 时自动 unregister + free
     * 对比手动版: input_allocate_device + input_register_device
     *            + input_unregister_device + input_free_device
     *
     * 和野火教程的 input 设备注册流程完全一致:
     *   allocate → 设置 name/evbit → set_capability → register
     */
    kd->input_dev = devm_input_allocate_device(&pdev->dev);
    if (!kd->input_dev)
        return -ENOMEM;

    kd->input_dev->name = "recovery_key";
    kd->input_dev->phys = "adc-key/input0";
    kd->input_dev->dev.parent = &pdev->dev;

    /* 设置支持的事件: EV_KEY 类型, KEY_ENTER 键值 */
    input_set_capability(kd->input_dev, EV_KEY, KEY_ENTER);

    ret = input_register_device(kd->input_dev);
    if (ret) {
        dev_err(&pdev->dev, "input_register_device failed: %d\n", ret);
        return ret;
    }

    /* 4. 启动轮询工作队列 */
    kd->key_pressed = 0;
    INIT_DELAYED_WORK(&kd->poll_work, key_poll_func);
    schedule_delayed_work(&kd->poll_work,
                          msecs_to_jiffies(POLL_INTERVAL_MS));

    /* 保存私有数据，remove 时通过 platform_get_drvdata 取回 */
    platform_set_drvdata(pdev, kd);

    dev_info(&pdev->dev, "driver loaded (SARADC ch1, poll %dms)\n",
             POLL_INTERVAL_MS);
    return 0;
}

/*
 * ========== remove 函数 ==========
 *
 * 只需要停止工作队列 — 其他资源都是 devm 管理的，自动释放:
 *   devm_kzalloc              → 自动 kfree
 *   devm_iio_channel_get      → 自动 iio_channel_release
 *   devm_input_allocate_device → 自动 input_unregister + input_free
 *
 * 对比野火教程的 remove 需要手动释放 4 个资源:
 *   input_unregister_device / input_free_device / free_irq / gpiod_put
 * 用 devm 就干净多了。
 */
static int key_remove(struct platform_device *pdev)
{
    struct key_data *kd = platform_get_drvdata(pdev);

    cancel_delayed_work_sync(&kd->poll_work);
    dev_info(&pdev->dev, "driver unloaded\n");
    return 0;
}

/*
 * ========== 设备树匹配表 ==========
 *
 * compatible 必须和设备树 overlay 中的一致: "recovery-adc-key"
 *
 * 匹配流程:
 *   1. 内核启动时 (或 overlay 加载后) 扫描设备树
 *   2. 发现 adc-key 节点的 compatible = "recovery-adc-key"
 *   3. insmod key_drv.ko 时，内核发现 of_match_table 中有相同字符串
 *   4. 自动调用 key_probe()
 *
 * MODULE_DEVICE_TABLE: 让 modprobe 能自动加载 (根据设备树 compatible 自动匹配)
 */
static const struct of_device_id key_of_match[] = {
    { .compatible = "recovery-adc-key" },
    { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, key_of_match);

/*
 * ========== platform_driver 结构体 ==========
 *
 * 对比 module_init/exit:
 *   module_init:     insmod 时执行 — 手动的、一次性的
 *   platform_driver: 设备树匹配时自动 probe — 框架驱动的、可热插拔
 *
 * module_platform_driver 宏:
 *   展开为 module_init(platform_driver_register) + module_exit(platform_driver_unregister)
 *   省去了手写 init/exit 的样板代码
 */
static struct platform_driver key_driver = {
    .probe = key_probe,
    .remove = key_remove,
    .driver = {
        .name = "recovery_key",
        .of_match_table = key_of_match,
    },
};
module_platform_driver(key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("ADC key driver for LubanCat-4 Recovery button");
MODULE_VERSION("3.0");
```

## 第四步：编写 Makefile

创建 `Makefile`:

```makefile
obj-m += key_drv.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## 第五步：编译并加载

```bash
cd ~/chezai/driver/key
make
sudo insmod key_drv.ko
dmesg | tail -5
# → 应该看到 "driver loaded (SARADC ch1, poll 50ms)"

# 查看新创建的 input 设备
cat /proc/bus/input/devices | grep -A5 "recovery_key"
```

> **注意**: 如果 dmesg 没有 "driver loaded"，说明 probe 没触发。
> 检查: `ls /proc/device-tree/adc-key/` 确认 overlay 生效了。
> 确认 overlay 中的 `compatible = "recovery-adc-key"` 和驱动代码一致。

## 第六步：用 evtest 测试（标准 Input 子系统调试工具）

```bash
# 安装 evtest (如果没有)
sudo apt install evtest 2>/dev/null || true

# 列出所有 input 设备，找到 recovery_key
sudo evtest
# 选择 recovery_key 对应的 /dev/input/eventX 编号
# 然后按 Recovery 按键，应该看到:
#   Event: type 1 (EV_KEY), code 28 (KEY_ENTER), value 1  ← 按下
#   Event: type 1 (EV_KEY), code 28 (KEY_ENTER), value 0  ← 松开
```

evtest 是 Linux 标准的 Input 子系统调试工具，面试时说"用 evtest 验证了按键事件上报"加分。

## 第七步：编写测试程序 (读 struct input_event)

创建 `test_key.c`：

```c
/*
 * test_key.c — Input 子系统按键测试程序
 *
 * 和野火教程的测试程序结构一致:
 *   open(/dev/input/eventX) → read(struct input_event) → 判断按下/松开
 *
 * 区别: 我们自动查找 recovery_key 设备，不需要手动指定 event 编号
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>

/* 自动查找 recovery_key 对应的 /dev/input/eventX */
static int find_event_dev(char *path, size_t len)
{
    DIR *dir;
    struct dirent *ent;
    char name_path[256];
    char name[64];
    int fd;

    dir = opendir("/sys/class/input");
    if (!dir) return -1;

    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        snprintf(name_path, sizeof(name_path),
                 "/sys/class/input/%s/device/name", ent->d_name);
        fd = open(name_path, O_RDONLY);
        if (fd < 0) continue;

        memset(name, 0, sizeof(name));
        read(fd, name, sizeof(name) - 1);
        close(fd);

        /* 去掉换行符 */
        char *nl = strchr(name, '\n');
        if (nl) *nl = '\0';

        if (strcmp(name, "recovery_key") == 0) {
            snprintf(path, len, "/dev/input/%s", ent->d_name);
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return -1;
}

int main(void)
{
    char dev_path[64];
    int fd;
    struct input_event ev;

    if (find_event_dev(dev_path, sizeof(dev_path)) < 0) {
        printf("找不到 recovery_key 设备，请先加载 key_drv.ko\n");
        return 1;
    }

    printf("找到设备: %s\n", dev_path);

    fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("监听 Recovery 按键 (Ctrl+C 退出)...\n");

    /*
     * 和野火教程一样: 循环 read struct input_event
     *
     * read() 会阻塞直到有新的输入事件
     * 不需要自己实现 poll — Input 子系统内部自带
     *
     * struct input_event {
     *     struct timeval time;  // 事件时间戳
     *     __u16 type;           // 事件类型 (EV_KEY = 1)
     *     __u16 code;           // 键值 (KEY_ENTER = 28)
     *     __s32 value;          // 1=按下, 0=松开
     * };
     */
    while (1) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) {
            perror("read");
            break;
        }

        /* 只关心 EV_KEY 事件，忽略 EV_SYN 等 */
        if (ev.type == EV_KEY && ev.code == KEY_ENTER) {
            printf("[%ld.%06ld] Recovery 按键: %s\n",
                   ev.input_event_sec, ev.input_event_usec,
                   ev.value ? "按下 ↓" : "松开 ↑");
        }
    }

    close(fd);
    return 0;
}
```

编译并测试:

```bash
gcc -o test_key test_key.c

# 运行测试 (需要 root 权限读 /dev/input/eventX)
sudo ./test_key
# 按 Recovery 按键，看输出
# Ctrl+C 退出
```

## 第八步：卸载

```bash
sudo rmmod key_drv
dmesg | tail -3
```

> Overlay 不需要卸载 — 它只描述硬件信息，驱动卸载后节点还在但无人使用。
> 如果想永久移除 overlay，注释掉 uEnv 中的 dtoverlay 行并重启。

---

## 四个驱动对比总结 (面试必备)

| 维度 | DHT11 | LED | KEY |
|------|-------|-----|-----|
| 硬件接口 | GPIO (单线协议) | GPIO (输出) | **ADC (IIO consumer API)** |
| 设备模型 | misc device | misc device | **Input 子系统** (input_dev) |
| 驱动框架 | module_init/exit | module_init/exit | **platform_driver** (probe/remove) |
| 设备树 | 无 (硬编码 GPIO) | 无 (硬编码 GPIO) | **设备树 Overlay** |
| 资源管理 | 手动 alloc/free | 手动 alloc/free | **devm_ 托管** |
| 数据方向 | 内核→用户 (read) | 双向 (read+write) | **input_report_key** (标准事件) |
| 时序要求 | 严格 us 级(关中断) | 无 | 由 IIO 驱动处理 |
| 轮询机制 | 用户触发 (read 时采集) | 无 (被动控制) | **delayed_work** (内核工作队列) |
| 定时器 | 无 | timer_list (闪烁) | delayed_work (轮询) |
| 底层 API | gpio_get/set_value | gpio_set_value | **iio_read_channel_raw** |
| 用户接口 | /dev/dht11 (自定义) | /dev/lubancat_led (自定义) | **/dev/input/eventX (标准)** |
| 并发保护 | mutex + 关中断 | mutex + timer | workqueue (单线程序列化) |
| ioctl | 无 | 有 (4个命令) | 无 (Input 子系统不需要) |

### 为什么按键用 Input 子系统而不是 misc device？ (面试必答)

1. **标准化**: 所有输入设备统一用 `struct input_event`，Qt/GTK/Wayland 都能直接使用
2. **不需要自己实现 poll**: Input core 内部已经实现了等待队列
3. **工具生态**: evtest、libinput、xinput 等标准工具直接可用
4. **多路复用**: 一个应用可以同时监听多个 input 设备

### 为什么用 platform_driver 而不是 module_init？ (面试加分)

1. **设备和驱动分离**: 硬件信息在设备树，驱动只关心逻辑 — Linux 设备模型核心思想
2. **自动匹配**: compatible 字符串匹配，不需要硬编码硬件参数
3. **资源获取规范**: 通过设备树属性 (io-channels) 获取硬件资源，而不是硬编码路径
4. **热插拔支持**: overlay 加载/卸载时自动 probe/remove
5. **生产级实践**: 实际 Linux 驱动开发几乎都用 platform_driver

---

## 常见问题

**Q: dmesg 没有 "driver loaded"，probe 没触发？**
A: 检查 `ls /proc/device-tree/adc-key/`。如果目录不存在，overlay 没加载成功 — 检查 uEnv 配置并重启。如果目录存在，检查 `cat /proc/device-tree/adc-key/compatible` 是否为 `recovery-adc-key`。

**Q: probe 报 "failed to get IIO channel"？**
A: SARADC IIO 驱动可能晚于我们的驱动加载。试试先 rmmod 再 insmod，或在 probe 返回 `-EPROBE_DEFER` 让内核稍后重试。

**Q: 按键没反应？**
A: 先验证硬件 `cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw`，按住 Recovery 键看值是否变化。

**Q: evtest 看到事件但 test_key 没反应？**
A: 检查 test_key 里的 `ev.code == KEY_ENTER`，确认和驱动里的键值一致。

**Q: 为什么不用内核自带的 adc-keys 驱动？**
A: 内核确实有 `drivers/input/keyboard/adc-keys.c`，只需设备树配置就能工作。但手写驱动是为了学习 platform_driver + IIO consumer + Input 子系统的完整流程。面试时说出 "我知道有通用驱动，但为了深入理解子系统交互选择手写" 即可。
