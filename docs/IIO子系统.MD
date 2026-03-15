IIO = Industrial I/O = 工业输入输出
专门为传感器、ADC、DAC 这类"测量物理量"的设备设计的框架。
## 概念理解
### 没有 IIO 框架时，你读 ADC 要这样做：
  ioremap(寄存器地址)
  → 配置时钟
  → 配置采样率
  → 触发一次转换
  → 等转换完成（查询/中断）
  → 读数据寄存器
  → 处理对齐/精度
  → iounmap()
  
  每块芯片寄存器不一样 → 每个人写一套 → 代码乱成一团

### 有了 IIO 框架，芯片厂商（瑞芯微）已经写好底层驱动：
  你只需要调一行：
  iio_read_channel_raw(channel, &value);
  → 框架帮你做上面所有事
### 驱动框架演变成了
┌─────────────────────────────────────────┐
│         IIO 子系统                       │
│                                         │
│  Provider（提供者）                      │
│  = 芯片驱动，负责操作硬件寄存器           │
│  例：rockchip-saradc.c                  │
│  它注册了 /sys/bus/iio/devices/iio:device0│
│                                         │
│  Consumer（消费者）                      │
│  = 你的驱动，只管调 API 读数据            │
│  例：你的 key_drv.c                     │
│  调 iio_read_channel_raw() 就完事        │
└─────────────────────────────────────────┘
 KEY 驱动就是一个 IIO Consumer（消费者），不需要知道 SARADC 寄存器怎么操作

 ## IIO 暴露给用户的接口
 IIO 框架自动在 sysfs 创建标准文件：
 /sys/bus/iio/devices/iio:device0/
    ├── name                      # 驱动名：fec10000.saradc
    ├── in_voltage0_raw           # 通道0的ADC原始值
    ├── in_voltage1_raw           # 通道1的ADC原始值 ← Recovery按键
    ├── in_voltage2_raw           # 通道2...
    └── ...

读取就是 cat，简单明了
cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw
输出：4090   ← 未按下
输出：0      ← 按下