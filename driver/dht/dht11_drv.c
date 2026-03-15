/*
 * dht11_drv.c — DHT11 温湿度传感器 Linux 内核驱动
 *
 * 硬件连接: DHT11 DATA 脚 → RK3588S GPIO3_A6 (gpiochip3 offset 6)
 * 设备节点: /dev/dht11 (misc device, 自动创建)
 * 接口: read() 返回 5 字节 [湿度整数, 湿度小数, 温度整数, 温度小数, 校验和]
 *
 * DHT11 单线协议时序:
 *   主机起始: 拉低 ≥18ms → 拉高 20~40us → 释放为输入
 *   DHT11应答: 拉低 80us → 拉高 80us
 *   数据传输: 40 bit (每 bit 先 50us 低电平，再高电平: 26~28us=0, 70us=1)
 *   数据格式: [湿度整数][湿度小数][温度整数][温度小数][校验和]
 */

#include <linux/module.h>       /* 所有内核模块必须 — MODULE_LICENSE 等宏 */
#include <linux/fs.h>           /* file_operations 结构体 */
#include <linux/miscdevice.h>   /* misc_register — 简化字符设备注册 */
#include <linux/gpio.h>         /* gpio_request/free/direction/get_value */
#include <linux/delay.h>        /* msleep, udelay, ndelay */
#include <linux/io.h>           /* ioremap/iounmap/writel/readl — 直接操作寄存器 */
#include <linux/uaccess.h>      /* copy_to_user — 内核→用户空间拷贝 */
#include <linux/irqflags.h>     /* local_irq_save/restore — 关/开中断 */
#include <linux/timekeeping.h>  /* ktime_get_ns — 纳秒级时间戳 */
#include <linux/mutex.h>        /* mutex — 防止多进程同时读传感器 */

/*
 * ========== GPIO 定义 ==========
 *
 * RK3588S GPIO 编号计算:
 *   GPIO3_A6 → bank=3, group=A(0), pin=6
 *   编号 = bank * 32 + group * 8 + pin = 3*32 + 0*8 + 6 = 102
 *
 * 验证: gpiochip3 base=96, offset=6, 96+6=102 ✓
 */
#define DHT11_GPIO      102
#define DHT11_DEV_NAME  "dht11"

/* 超时阈值 (微秒) — DHT11 任何一个电平变化不应超过 200us */
#define TIMEOUT_US      200

/* 数据采集缓冲: 5 字节 = 40 bit */
#define DHT11_DATA_LEN  5

/*
 * RK3588S GPIO3_A6 上拉寄存器地址:
 *   VCCIO3_5_IOC 基址 = 0xFD5F8000
 *   GPIO3A_P 寄存器偏移 = 0x0090
 *   A6 对应 bit[13:12]: 00=浮空, 01=下拉, 10=上拉
 *   RK3588 写使能机制: 高 16 位是写掩码 (对应 bit 置 1 才允许写低 16 位)
 *   所以要写 A6 的 bit[13:12], 掩码在 bit[29:28] = 11
 */
#define RK3588_GPIO3A_P_REG   0xFD5F8090
#define GPIO3_A6_PULL_MASK    (0x3 << 12)          /* bit[13:12] */
#define GPIO3_A6_PULL_UP      (0x2 << 12)          /* 10 = 上拉 */
#define GPIO3_A6_WRITE_EN     (0x3 << (12 + 16))   /* bit[29:28] 写使能 */

/* 互斥锁: DHT11 是单线设备, 同一时刻只能有一个读操作 */
static DEFINE_MUTEX(dht11_lock);

/*
 * ========== 辅助函数: 等待 GPIO 变为指定电平 ==========
 *
 * 为什么不用 usleep？因为 DHT11 的时序是 us 级别的:
 *   - 一个 bit 的高电平持续 26~70us
 *   - usleep 最小精度约 50-100us, 完全不够用
 *   - 所以必须用「忙等」(busy wait) + ktime 纳秒计时
 *
 * 返回: 0=成功, -1=超时
 */
static int wait_for_level(int level, int timeout_us)
{
    u64 deadline = ktime_get_ns() + (u64)timeout_us * 1000;

    while (gpio_get_value(DHT11_GPIO) != level) {
        if (ktime_get_ns() > deadline)
            return -1;
    }
    return 0;
}

/*
 * ========== 核心: 读取 DHT11 传感器 ==========
 *
 * 这个函数完成整个单线协议的通信, 返回 5 字节原始数据。
 * 对照你的 v5 程序, 逻辑完全一样, 只是 API 不同。
 *
 * 关键设计决策:
 *   - 采集 40 bit 数据时 关闭本地 CPU 中断 (local_irq_save)
 *   - 原因: 如果中断打断了 us 级时序测量, 26us 和 70us 就分不清了
 *   - 这就是为什么用户空间 v5 要用 SCHED_FIFO prio=99 的原因 —— 同样的思路
 *   - 内核中关中断比 SCHED_FIFO 更彻底, 连定时器中断都挡掉了
 *
 * 返回: 0=成功, 负数=失败
 */
static int dht11_read_sensor(unsigned char data[DHT11_DATA_LEN])
{
    unsigned long irq_flags;
    int i;
    u64 t_start;

    memset(data, 0, DHT11_DATA_LEN);

    /*
     * --- 第1步: 主机起始信号 ---
     * 拉低 GPIO ≥18ms, 告诉 DHT11 "我要读数据了"
     * 然后拉高 20-40us, 释放总线等 DHT11 响应
     *
     * 对应 v5 的:
     *   gpiod_line_set_value(line, 0); usleep(20000);
     *   gpiod_line_set_value(line, 1); usleep(40);
     */
    gpio_direction_output(DHT11_GPIO, 1);
    msleep(5);                          /* 先稳定高电平 */
    gpio_set_value(DHT11_GPIO, 0);
    msleep(20);                         /* 拉低 20ms (规格要求 ≥18ms) */
    gpio_set_value(DHT11_GPIO, 1);
    udelay(30);                         /* 拉高 ~30us */

    /*
     * --- 第2步: 切换为输入, 等待 DHT11 应答 ---
     * DHT11 看到起始信号后, 会:
     *   1) 拉低 80us (应答低电平)
     *   2) 拉高 80us (应答高电平)
     *   3) 然后开始发 40 bit 数据
     *
     * 对应 v5 的:
     *   gpiod_line_release(line);
     *   gpiod_line_request_input(line, "dht11");
     *   wait_for(line, 0, 5000); wait_for(line, 1, 5000); wait_for(line, 0, 5000);
     */
    gpio_direction_input(DHT11_GPIO);

    if (wait_for_level(0, TIMEOUT_US)) {    /* 等低电平 (应答开始) */
        pr_err("dht11: no response (low)\n");
        return -ETIMEDOUT;
    }
    if (wait_for_level(1, TIMEOUT_US)) {    /* 等高电平 (应答中段) */
        pr_err("dht11: no response (high)\n");
        return -ETIMEDOUT;
    }
    if (wait_for_level(0, TIMEOUT_US)) {    /* 等低电平 (第一个 bit 开始) */
        pr_err("dht11: no data start\n");
        return -ETIMEDOUT;
    }

    /*
     * --- 第3步: 采集 40 bit 数据 (关中断) ---
     *
     * 每个 bit 的时序:
     *   先 ~50us 低电平 (同步信号)
     *   再高电平: 26-28us = bit 0, 70us = bit 1
     *
     * 判断方法: 测量高电平持续时间
     *   < 40us → 0
     *   ≥ 40us → 1
     *
     * 为什么关中断？
     *   想象你正在测一个 26us 的高电平, 刚测了 10us 突然来个定时器中断
     *   处理完中断回来已经过了 100us, 你以为它持续了 100us, 判定成"1" → 数据错误
     *   关中断保证这段测量不被打断
     *
     * 对应 v5 的:
     *   for (i = 0; i < 40; i++) {
     *       wait_for(line, 1, 2000);      ← 等高电平
     *       long t = micros();            ← 记录时间
     *       wait_for(line, 0, 2000);      ← 等低电平
     *       if (micros() - t > 40) bit=1; ← 判断持续时间
     *   }
     */
    local_irq_save(irq_flags);         /* 🔒 关中断 — 进入临界区 */

    for (i = 0; i < 40; i++) {
        /* 等待高电平 (每个 bit 的数据部分) */
        if (wait_for_level(1, TIMEOUT_US))
            goto timeout;

        /* 记录高电平开始时间 */
        t_start = ktime_get_ns();

        /* 等待低电平 (高电平结束) */
        if (wait_for_level(0, TIMEOUT_US))
            goto timeout;

        /* 计算高电平持续时间, 判断 0/1 */
        data[i / 8] <<= 1;
        if ((ktime_get_ns() - t_start) > 40000)    /* 40us = 40000ns */
            data[i / 8] |= 1;
    }

    local_irq_restore(irq_flags);      /* 🔓 恢复中断 */

    /*
     * --- 第4步: 校验和 ---
     * data[4] 应该等于 data[0]+data[1]+data[2]+data[3] 的低 8 位
     * 和你 v5 里的 cksum 检查一模一样
     */
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        pr_warn("dht11: checksum error [%02x %02x %02x %02x %02x]\n",
                data[0], data[1], data[2], data[3], data[4]);
        return -EIO;
    }

    pr_debug("dht11: %d.%d%% %d.%dC\n",
             data[0], data[1], data[2], data[3]);
    return 0;

timeout:
    local_irq_restore(irq_flags);      /* 超时也要恢复中断！ */
    pr_err("dht11: bit %d timeout\n", i);
    return -ETIMEDOUT;
}

/*
 * ========== file_operations: read ==========
 *
 * 用户调用 read(fd, buf, 5) 时内核执行这个函数。
 *
 * 设计要点:
 *   - 用 mutex 保证同一时刻只有一个进程读传感器
 *   - DHT11 最快 1 秒采样一次, 这里重试 3 次, 每次间隔 1.5 秒
 *   - copy_to_user: 内核空间 data[] → 用户空间 buf[]
 *     (不能直接 memcpy, 因为内核和用户空间地址隔离)
 */
static ssize_t dht11_fops_read(struct file *filp, char __user *buf,
                                size_t count, loff_t *ppos)
{
    unsigned char data[DHT11_DATA_LEN];
    int ret;
    int retry;

    if (count < DHT11_DATA_LEN)
        return -EINVAL;

    /* 加锁: 防止两个线程同时操作 GPIO */
    if (mutex_lock_interruptible(&dht11_lock))
        return -ERESTARTSYS;

    /* 重试最多 3 次 (DHT11 偶尔会校验失败, 和你 v5 里 15 次重试一个道理) */
    for (retry = 0; retry < 3; retry++) {
        ret = dht11_read_sensor(data);
        if (ret == 0)
            break;
        msleep(1500);   /* DHT11 采样间隔至少 1s */
    }

    mutex_unlock(&dht11_lock);

    if (ret)
        return ret;

    /* 内核数据 → 用户空间 */
    if (copy_to_user(buf, data, DHT11_DATA_LEN))
        return -EFAULT;

    return DHT11_DATA_LEN;     /* 返回读到的字节数 = 5 */
}

/*
 * ========== file_operations 结构体 ==========
 *
 * 只需要实现 .read — 因为 Qt 端只调用 open + read
 * .owner = THIS_MODULE 防止模块被卸载时还有人在用
 */
static const struct file_operations dht11_fops = {
    .owner = THIS_MODULE,
    .read  = dht11_fops_read,
};

/*
 * ========== misc device ==========
 *
 * 为什么用 misc device 而不是手动 register_chrdev？
 *   - misc device 自动分配主设备号 (10)
 *   - 自动在 /dev/ 下创建设备节点 (不需要手动 mknod)
 *   - 代码量少很多, 适合这种简单的传感器
 *
 * .minor = MISC_DYNAMIC_MINOR 让内核自动分配次设备号
 * .name = "dht11" → 创建 /dev/dht11
 */
static struct miscdevice dht11_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DHT11_DEV_NAME,
    .fops  = &dht11_fops,
};

/*
 * ========== 模块加载: insmod dht11_drv.ko ==========
 *
 * 1. 申请 GPIO 102 的使用权
 * 2. 注册 misc 设备 → /dev/dht11 出现
 */
static int __init dht11_init(void)
{
    int ret;
    void __iomem *pull_reg;

    ret = gpio_request(DHT11_GPIO, DHT11_DEV_NAME);
    if (ret) {
        pr_err("dht11: gpio_request(%d) failed: %d\n", DHT11_GPIO, ret);
        return ret;
    }

    /*
     * 设置 GPIO3_A6 为上拉 (pull-up)
     * DHT11 单线协议要求总线空闲时为高电平
     * RK3588S 默认配置是 pull-down, 必须改为 pull-up
     *
     * 通过 ioremap 直接写 VCCIO3_5_IOC 的 GPIO3A_P 寄存器:
     *   bit[13:12] = 10 (上拉)
     *   bit[29:28] = 11 (写使能掩码, RK3588 特有机制)
     */
    pull_reg = ioremap(RK3588_GPIO3A_P_REG, 4);
    if (pull_reg) {
        writel(GPIO3_A6_WRITE_EN | GPIO3_A6_PULL_UP, pull_reg);
        iounmap(pull_reg);
        pr_info("dht11: GPIO3_A6 pull-up configured via IOC register\n");
    } else {
        pr_warn("dht11: failed to ioremap pull register, pull-up not set\n");
    }

    ret = misc_register(&dht11_misc);
    if (ret) {
        pr_err("dht11: misc_register failed: %d\n", ret);
        gpio_free(DHT11_GPIO);
        return ret;
    }

    pr_info("dht11: driver loaded (GPIO %d → /dev/%s)\n",
            DHT11_GPIO, DHT11_DEV_NAME);
    return 0;
}

/*
 * ========== 模块卸载: rmmod dht11_drv ==========
 *
 * 反注册 misc 设备, 释放 GPIO — 资源释放顺序要和申请相反
 */
static void __exit dht11_exit(void)
{
    misc_deregister(&dht11_misc);
    gpio_free(DHT11_GPIO);
    pr_info("dht11: driver unloaded\n");
}

module_init(dht11_init);
module_exit(dht11_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("DHT11 temperature & humidity sensor driver for RK3588S");
MODULE_VERSION("1.0");
