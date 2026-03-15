3.1 Input 子系统是干什么的
专门为人机输入设备（键盘、鼠标、触摸屏、按键）设计的框架。
### 为什么需要 Input 子系统？
没有 Input 框架：
  /dev/mykey   → read() 返回 1字节：0x01=按下，0x00=松开
  /dev/joy     → read() 返回 结构体：{x, y, btn}
  /dev/ts      → read() 返回 结构体：{x, y, pressure}
  
  每个设备格式不一样，Qt/GTK要为每种设备单独写代码 😫

有了 Input 框架：
  所有输入设备统一用 struct input_event
  /dev/input/event0  → 键盘
  /dev/input/event1  → 鼠标
  /dev/input/event2  → 你的 Recovery 按键
  
  用户程序写一套代码，读所有设备 ✅
### 框架
  ┌──────────────────────────────────────────────────────────────┐
│  第三层：用户空间                                              │
│                                                              │
│  test_key.c / evtest / Qt                                    │
│  read(fd, &ev, sizeof(struct input_event))                   │
│  if (ev.type==EV_KEY && ev.code==KEY_ENTER && ev.value==1)   │
│      printf("按下！")                                         │
└─────────────────────┬────────────────────────────────────────┘
                      │ /dev/input/eventX
┌─────────────────────▼────────────────────────────────────────┐
│  第二层：事件处理层（内核已内建，你不用写）                      │
│                                                              │
│  evdev.c                                                     │
│  负责把内核事件打包成 struct input_event 给用户读              │
│  负责实现 read()/poll()/select() — 你不需要自己写这些！        │
└─────────────────────┬────────────────────────────────────────┘
                      │ input_report_key() / input_sync()
┌─────────────────────▼────────────────────────────────────────┐
│  第一层：驱动层（你写这层）                                     │
│                                                              │
│  key_drv.c                                                   │
│  读 ADC → 判断按键状态 → 调 input_report_key() 上报           │
│  你只管"上报"，不管用户怎么读                                   │
└──────────────────────────────────────────────────────────────┘

struct input_event {
    // ① 时间戳（内核自动填，你不用管）
    __kernel_ulong_t input_event_sec;   // 秒
    __kernel_ulong_t input_event_usec;  // 微秒

    // ② 事件类型：大分类
    __u16 type;
    //  EV_SYN = 0  → 同步事件（一组事件结束的标志）
    //  EV_KEY = 1  → 按键事件 ← 我们用这个
    //  EV_REL = 2  → 相对坐标（鼠标移动）
    //  EV_ABS = 3  → 绝对坐标（触摸屏）

    // ③ 事件码：小分类，type 决定 code 的含义
    __u16 code;
    //  当 type=EV_KEY 时，code 是键值：
    //    KEY_ENTER = 28
    //    KEY_1     = 2
    //    BTN_0     = 0x100

    // ④ 事件值
    __s32 value;
    //  当 type=EV_KEY 时：1=按下, 0=松开, 2=长按重复
};

### 按一下 Recovery 键，你的程序会收到三个 input_event：

    ① type=EV_KEY, code=KEY_ENTER, value=1   ← 按下
    ② type=EV_SYN, code=SYN_REPORT, value=0  ← 同步（一组事件结束）

    松开后：
    ③ type=EV_KEY, code=KEY_ENTER, value=0   ← 松开
    ④ type=EV_SYN, code=SYN_REPORT, value=0  ← 同步

### 你只需要关注 type=EV_KEY 的事件，其他的都可以忽略。
## 数据流
Recovery 按键硬件
      │ 按下/松开，ADC值从4090变到0
      ▼
SARADC 硬件寄存器
      │ 瑞芯微已经写好了 rockchip-saradc.c（IIO Provider）
      ▼
IIO 子系统框架
      │ 你调 iio_read_channel_raw()，或者直接读 sysfs 文件
      ▼
key_drv.c 里的 key_poll_func()
      │ 每50ms读一次ADC值
      │ adc_val < 2000 → pressed=1
      │ 状态变化时调：
      │   input_report_key(key_input_dev, KEY_ENTER, pressed)
      │   input_sync(key_input_dev)
      ▼
Input 子系统框架（evdev.c）
      │ 把事件打包成 struct input_event
      │ 放入等待队列，唤醒正在 read() 的用户进程
      ▼
/dev/input/eventX
      │ 用户 read(fd, &ev, sizeof(ev))
      ▼
test_key.c / evtest / Qt 应用
                            │ 
                            ▼
                            解析 ev.type/ev.code/ev.value，做出响应
