# LED 字符设备驱动开发教程

## 硬件信息

- 板子: LubanCat-4 (RK3588S)
- LED: 板载 sys_led，接在 **GPIO3_B5**
- GPIO 编号: `3×32 + 1×8 + 5 = 109`
- 电气特性: **低电平亮** (active-low)，GPIO 输出 0 = LED 亮，输出 1 = LED 灭
- 当前被内核 `gpio-leds` 驱动占用，加载我们的驱动前需要先解绑

## 目标

写一个 misc 字符设备驱动，创建 `/dev/lubancat_led`，支持：
- `write()`: 写 `"1"` 亮灯，写 `"0"` 灭灯
- `read()`: 读取当前状态
- `ioctl()`: ON / OFF / TOGGLE / BLINK 四种命令

风格和你的 DHT11 驱动保持一致。

---

## 第一步：创建项目目录

```bash
mkdir -p ~/chezai/driver/led
cd ~/chezai/driver/led
```

## 第二步：编写驱动代码

创建 `led_drv.c`：

```c
/*
 * led_drv.c — LED 字符设备驱动 (misc device)
 *
 * 硬件: LubanCat-4 板载 sys_led → GPIO3_B5 (编号 109)
 * 设备节点: /dev/lubancat_led (misc device, 自动创建)
 *
 * 接口:
 *   write(): 写 "1" 亮灯, 写 "0" 灭灯
 *   read():  返回当前状态 "1\n" 或 "0\n"
 *   ioctl(): LED_IOC_ON / LED_IOC_OFF / LED_IOC_TOGGLE / LED_IOC_BLINK
 *
 * 使用前需先解绑默认驱动:
 *   echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/unbind
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

/*
 * ========== GPIO 定义 ==========
 *
 * GPIO3_B5 编号计算:
 *   bank=3, group=B(1), pin=5
 *   编号 = 3*32 + 1*8 + 5 = 109
 *
 * 对比你 DHT11 的 GPIO3_A6 = 102，同一个 GPIO bank，只是不同引脚
 */
#define LED_GPIO        109
#define LED_DEV_NAME    "lubancat_led"

/*
 * ========== ioctl 命令定义 ==========
 *
 * _IO(type, nr):  无数据传输的命令
 * _IOW(type, nr, datatype): 用户→内核传数据
 *
 * 'L' 是 magic number，自定义，避免和其他驱动冲突
 * 你的 DHT11 没用 ioctl，这里多学一个接口
 */
#define LED_IOC_MAGIC   'L'
#define LED_IOC_ON      _IO(LED_IOC_MAGIC, 0)       /* 亮灯 */
#define LED_IOC_OFF     _IO(LED_IOC_MAGIC, 1)       /* 灭灯 */
#define LED_IOC_TOGGLE  _IO(LED_IOC_MAGIC, 2)       /* 翻转 */
#define LED_IOC_BLINK   _IOW(LED_IOC_MAGIC, 3, int) /* 闪烁，参数=周期ms */

static DEFINE_MUTEX(led_lock);

/* LED 当前状态: 0=灭, 1=亮 */
static int led_state;

/* 闪烁定时器 — 内核定时器，DHT11 里没用到，这里多学一个机制 */
static struct timer_list blink_timer;
static int blink_period_ms;  /* 0 = 不闪烁 */

/*
 * ========== 设置 LED 物理状态 ==========
 *
 * 关键点: sys_led 是 active-low (低电平亮)
 *   on=1 → gpio_set_value(109, 0) → LED 亮
 *   on=0 → gpio_set_value(109, 1) → LED 灭
 *
 * 你之前测试验证过:
 *   echo 1 | sudo tee .../brightness  → 亮
 *   echo 0 | sudo tee .../brightness  → 灭
 * gpio-leds 驱动内部已经帮你做了 active-low 翻转，
 * 我们自己写驱动就得自己处理这个翻转
 */
static void led_set(int on)
{
    led_state = !!on;
    gpio_set_value(LED_GPIO, !led_state);  /* active-low: 取反 */
}

/*
 * ========== 闪烁定时器回调 ==========
 *
 * 内核定时器在软中断上下文执行，不能睡眠。
 * 这就是为什么不能在里面用 mutex_lock（会睡眠），
 * 但 gpio_set_value 是原子操作，可以在这里调用。
 *
 * mod_timer() 重新设置下一次触发时间 = 当前 jiffies + 周期
 */
static void blink_timer_callback(struct timer_list *t)
{
    if (blink_period_ms <= 0)
        return;

    led_set(!led_state);  /* 翻转 */

    mod_timer(&blink_timer,
              jiffies + msecs_to_jiffies(blink_period_ms));
}

/*
 * ========== write(): 用户空间控制亮灭 ==========
 *
 * 对比 DHT11 的 read()，这里是反向操作：
 *   DHT11: 内核→用户 (copy_to_user)
 *   LED:   用户→内核 (get_user)
 *
 * get_user(val, buf) 是 copy_from_user 的简化版，只拷贝一个字节
 */
static ssize_t led_fops_write(struct file *filp, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    char val;

    if (count < 1)
        return -EINVAL;

    if (get_user(val, buf))
        return -EFAULT;

    mutex_lock(&led_lock);

    /* 手动写入时停止闪烁 */
    blink_period_ms = 0;
    del_timer_sync(&blink_timer);

    if (val == '1')
        led_set(1);
    else if (val == '0')
        led_set(0);
    else {
        mutex_unlock(&led_lock);
        return -EINVAL;
    }

    mutex_unlock(&led_lock);
    return count;
}

/*
 * ========== read(): 返回当前 LED 状态 ==========
 *
 * 和 DHT11 的 read 对比:
 *   DHT11: 读传感器硬件 → 返回 5 字节原始数据
 *   LED:   读软件状态变量 → 返回 "1\n" 或 "0\n"
 *
 * *ppos 检查: 防止 cat /dev/lubancat_led 无限循环读取
 *   第一次 read: *ppos=0，返回数据，*ppos 前进
 *   第二次 read: *ppos>0，返回 0 (EOF)，cat 退出
 */
static ssize_t led_fops_read(struct file *filp, char __user *buf,
                              size_t count, loff_t *ppos)
{
    char status[3];
    int len;

    if (*ppos > 0)
        return 0;

    len = snprintf(status, sizeof(status), "%d\n", led_state);

    if (count < len)
        return -EINVAL;

    if (copy_to_user(buf, status, len))
        return -EFAULT;

    *ppos += len;
    return len;
}

/*
 * ========== ioctl(): 高级控制接口 ==========
 *
 * ioctl 是字符设备的"万能"接口，用来传递 read/write 不方便表达的命令。
 * DHT11 只用了 read，功能单一所以够用。
 * LED 控制有多种模式 (亮/灭/翻转/闪烁)，用 ioctl 更合适。
 *
 * 面试考点:
 *   - cmd 怎么编码 (_IO/_IOW/_IOR/_IOWR)
 *   - 用户空间怎么调用 (ioctl(fd, LED_IOC_BLINK, &period))
 *   - 和 write 的区别 (ioctl 可以传结构化命令+参数)
 */
static long led_fops_ioctl(struct file *filp, unsigned int cmd,
                            unsigned long arg)
{
    int period;

    mutex_lock(&led_lock);

    switch (cmd) {
    case LED_IOC_ON:
        blink_period_ms = 0;
        del_timer_sync(&blink_timer);
        led_set(1);
        break;

    case LED_IOC_OFF:
        blink_period_ms = 0;
        del_timer_sync(&blink_timer);
        led_set(0);
        break;

    case LED_IOC_TOGGLE:
        blink_period_ms = 0;
        del_timer_sync(&blink_timer);
        led_set(!led_state);
        break;

    case LED_IOC_BLINK:
        if (copy_from_user(&period, (int __user *)arg, sizeof(int))) {
            mutex_unlock(&led_lock);
            return -EFAULT;
        }
        if (period < 0) {
            mutex_unlock(&led_lock);
            return -EINVAL;
        }
        blink_period_ms = period;
        if (period > 0) {
            mod_timer(&blink_timer,
                      jiffies + msecs_to_jiffies(period));
        } else {
            del_timer_sync(&blink_timer);
        }
        break;

    default:
        mutex_unlock(&led_lock);
        return -ENOTTY;  /* 不支持的命令 */
    }

    mutex_unlock(&led_lock);
    return 0;
}

/*
 * ========== file_operations ==========
 *
 * 对比 DHT11 只有 .read，LED 驱动有三个接口:
 *   .write           → 简单控制 (echo "1" > /dev/lubancat_led)
 *   .read            → 读状态   (cat /dev/lubancat_led)
 *   .unlocked_ioctl  → 高级控制 (闪烁等)
 */
static const struct file_operations led_fops = {
    .owner          = THIS_MODULE,
    .write          = led_fops_write,
    .read           = led_fops_read,
    .unlocked_ioctl = led_fops_ioctl,
};

static struct miscdevice led_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = LED_DEV_NAME,
    .fops  = &led_fops,
};

/*
 * ========== 模块加载 ==========
 *
 * 和 DHT11 对比，多了一个 timer_setup 初始化定时器
 * gpio_direction_output 第二个参数:
 *   DHT11: gpio_direction_output(102, 1) → 初始高电平 (空闲状态)
 *   LED:   gpio_direction_output(109, 1) → 初始高电平 = 灭灯 (active-low)
 */
static int __init led_init(void)
{
    int ret;

    ret = gpio_request(LED_GPIO, LED_DEV_NAME);
    if (ret) {
        pr_err("led: gpio_request(%d) failed: %d\n", LED_GPIO, ret);
        pr_err("led: 请先执行: echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/unbind\n");
        return ret;
    }

    gpio_direction_output(LED_GPIO, 1);  /* 初始灭灯 */
    led_state = 0;

    timer_setup(&blink_timer, blink_timer_callback, 0);

    ret = misc_register(&led_misc);
    if (ret) {
        pr_err("led: misc_register failed: %d\n", ret);
        gpio_free(LED_GPIO);
        return ret;
    }

    pr_info("led: driver loaded (GPIO %d -> /dev/%s)\n",
            LED_GPIO, LED_DEV_NAME);
    return 0;
}

/*
 * ========== 模块卸载 ==========
 *
 * 注意顺序: 先停定时器 → 灭灯 → 反注册 → 释放GPIO
 * 如果先释放GPIO再停定时器，定时器回调里 gpio_set_value 就会操作已释放的引脚
 */
static void __exit led_exit(void)
{
    blink_period_ms = 0;
    del_timer_sync(&blink_timer);
    led_set(0);
    misc_deregister(&led_misc);
    gpio_free(LED_GPIO);
    pr_info("led: driver unloaded\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("LED character device driver for LubanCat-4 (GPIO3_B5)");
MODULE_VERSION("1.0");
```

## 第三步：编写 Makefile

创建 `Makefile`（注意缩进必须用 **Tab**，不能用空格）:

```makefile
obj-m += led_drv.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## 第四步：编译

```bash
cd ~/chezai/driver/led
make
```

成功的话会生成 `led_drv.ko`。

## 第五步：加载驱动

```bash
# 1. 先解绑默认的 gpio-leds 驱动（释放 GPIO 109）
echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/unbind

# 2. 加载我们的驱动
sudo insmod led_drv.ko

# 3. 验证设备节点出现
ls -l /dev/lubancat_led

# 4. 查看内核日志
dmesg | tail -5
```

## 第六步：测试（命令行方式）

```bash
# 亮灯
echo 1 | sudo tee /dev/lubancat_led

# 灭灯
echo 0 | sudo tee /dev/lubancat_led

# 读状态
cat /dev/lubancat_led
```

## 第七步：编写测试程序

创建 `test_led.c`：

```c
/*
 * test_led.c — LED 驱动测试程序
 *
 * 用法:
 *   ./test_led on          亮灯
 *   ./test_led off         灭灯
 *   ./test_led toggle      翻转
 *   ./test_led blink 500   闪烁 (500ms 周期)
 *   ./test_led blink 0     停止闪烁
 *   ./test_led status      读取当前状态
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* 和驱动中定义保持一致 */
#define LED_IOC_MAGIC   'L'
#define LED_IOC_ON      _IO(LED_IOC_MAGIC, 0)
#define LED_IOC_OFF     _IO(LED_IOC_MAGIC, 1)
#define LED_IOC_TOGGLE  _IO(LED_IOC_MAGIC, 2)
#define LED_IOC_BLINK   _IOW(LED_IOC_MAGIC, 3, int)

#define DEV_PATH "/dev/lubancat_led"

int main(int argc, char *argv[])
{
    int fd, ret;

    if (argc < 2) {
        printf("用法:\n");
        printf("  %s on          亮灯\n", argv[0]);
        printf("  %s off         灭灯\n", argv[0]);
        printf("  %s toggle      翻转\n", argv[0]);
        printf("  %s blink <ms>  闪烁 (周期毫秒, 0=停止)\n", argv[0]);
        printf("  %s status      读取状态\n", argv[0]);
        return 1;
    }

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open " DEV_PATH);
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        ret = write(fd, "1", 1);
        if (ret < 0) perror("write");
        else printf("LED ON\n");

    } else if (strcmp(argv[1], "off") == 0) {
        ret = write(fd, "0", 1);
        if (ret < 0) perror("write");
        else printf("LED OFF\n");

    } else if (strcmp(argv[1], "toggle") == 0) {
        ret = ioctl(fd, LED_IOC_TOGGLE);
        if (ret < 0) perror("ioctl TOGGLE");
        else printf("LED TOGGLED\n");

    } else if (strcmp(argv[1], "blink") == 0) {
        int period = 500;
        if (argc >= 3)
            period = atoi(argv[2]);
        ret = ioctl(fd, LED_IOC_BLINK, &period);
        if (ret < 0) perror("ioctl BLINK");
        else if (period > 0) printf("LED BLINK: %d ms\n", period);
        else printf("LED BLINK stopped\n");

    } else if (strcmp(argv[1], "status") == 0) {
        char buf[4] = {0};
        ret = read(fd, buf, sizeof(buf) - 1);
        if (ret < 0) perror("read");
        else printf("LED status: %s", buf);

    } else {
        printf("未知命令: %s\n", argv[1]);
    }

    close(fd);
    return 0;
}
```

编译并测试:

```bash
gcc -o test_led test_led.c
sudo ./test_led on
sudo ./test_led off
sudo ./test_led toggle
sudo ./test_led blink 300    # 300ms 闪烁
sleep 5                       # 看 5 秒闪烁效果
sudo ./test_led blink 0      # 停止闪烁
sudo ./test_led status
```

## 第八步：卸载驱动

```bash
sudo rmmod led_drv
dmesg | tail -3
```

卸载后 `/dev/lubancat_led` 会自动消失。

如果想恢复默认心跳灯：
```bash
echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/bind
```

---

## 和 DHT11 驱动的对比（面试加分点）

| 维度 | DHT11 驱动 | LED 驱动 |
|------|-----------|---------|
| GPIO 方向 | 动态切换 (output→input→output) | 固定 output |
| 数据流 | 内核→用户 (read, copy_to_user) | 双向 (read+write) |
| 时序要求 | 严格 us 级，必须关中断 | 无时序要求 |
| 接口 | 只用 read | read + write + ioctl |
| 并发控制 | mutex (防多进程同时读) | mutex + timer (闪烁) |
| 内核定时器 | 未使用 | 使用 timer_list 实现闪烁 |
| 电平逻辑 | 需配置上拉 (ioremap 写寄存器) | active-low 取反即可 |

## 常见问题

**Q: gpio_request 返回 -EBUSY？**
A: GPIO 109 被 gpio-leds 占用了。先执行:
```bash
echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/unbind
```

**Q: 加载后 LED 不亮不灭？**
A: 确认 active-low 逻辑。如果你的板子是高电平亮，把 `led_set()` 里的 `!led_state` 改成 `led_state`。

**Q: 闪烁不工作？**
A: 检查 `dmesg`，确认定时器有没有报错。也可以先用 `write` 测试亮灭确认 GPIO 控制正常。
