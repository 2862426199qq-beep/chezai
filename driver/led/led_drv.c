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
#include <linux/miscdevice.h>//misc device是linux内核提供的一种简化字符设备驱动开发的框架，适用于功能简单、设备数量较少的字符设备。使用misc device可以避免繁琐的字符设备注册和管理流程，内核会自动为每个misc device分配一个次设备号，并创建对应的设备节点。
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/jiffies.h>//jiffies是linux内核中的一个全局变量，表示系统启动以来经过的时钟滴答数。它是一个递增的计数器，单位是时钟滴答（通常是10ms或1ms，取决于系统配置）。通过jiffies可以实现定时器、延迟等功能。


/*
 * ========== GPIO 定义 ==========
 * GPIO3_B5 编号计算:
 *   bank=3, group=B(1), pin=5
 *   编号 = 3*32 + 1*8 + 5 = 109
 */
#define LED_GPIO 141
#define LED_DEV_NAME "lubancat_led"
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
#define LED_IOC_BLINK   _IOW(LED_IOC_MAGIC, 3, int) /* 闪烁，or心跳，参数=周期ms*/

static DEFINE_MUTEX(led_mutex); // 互斥锁，保护LED状态和闪烁定时器,DEFINE_MUTEX是一个宏，用于定义和初始化一个互斥锁（mutex）。在这个驱动中，led_mutex 用于保护对LED状态和闪烁定时器的访问，确保在多线程环境下对这些资源的访问是安全的。
static int led_state;
//闪烁定时器 内核定时器 新东西
static struct timer_list blink_timer;
static int blink_period_ms; // 闪烁周期，单位毫秒
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
    led_state = !!on;//把on转换成0或1，确保led_state只有0或1两种状态
    gpio_set_value(LED_GPIO, !led_state);//sys_led是active-low，写0亮灯，写1灭灯，所以这里取反
}

//闪烁定时器回调函数
//内核定时器在软中断上下文执行，不能睡眠
//所以不能使用mutex_lock等可能睡眠的函数
//gpio_set_value是一个原子操作，可以安全调用

static void blink_timer_callback(struct timer_list *t)
{
    if(blink_period_ms <= 0) return; // 如果周期不合法，直接返回

    led_set(!led_state); // 翻转LED状态
    // 重新设置定时器，下一次触发时间 = 当前时间 + 周期
    mod_timer(&blink_timer, jiffies + msecs_to_jiffies(blink_period_ms)); 
}

//用户函数
static ssize_t led_fops_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char val;
    if(count < 1) return -EINVAL; // 输入太短，返回错误
    if(get_user(val, buf) < 0) return -EFAULT; // 从用户空间获取数据失败，返回错误
    mutex_lock(&led_mutex); // 加锁，保护LED状态
    //手动写入的时候停止闪烁
    blink_period_ms = 0; // 停止闪烁
    if(val == '1') {
        led_set(1); // 亮灯
    } else if(val == '0') {
        led_set(0); // 灭灯
    } else {
        mutex_unlock(&led_mutex); // 解锁
        return -EINVAL; // 输入不合法，返回错误
    }
    mutex_unlock(&led_mutex); // 解锁
    return count; // 返回写入的字节数
}

static ssize_t led_fops_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char status[3];
    int len;
    if(*ppos > 0) return 0; // 已经读过一次了，返回EOF
    len = snprintf(status, sizeof(status), "%d\n", led_state); // 格式化状态字符串,返回写入status的字节数，应该是2（"0\n"或"1\n"）
    if(count < len) return -EINVAL; // 用户缓冲区太小，返回错误
    if(copy_to_user(buf, status, len)) return -EFAULT; // 复制到用户空间失败，返回错误
    *ppos += len; // 更新文件偏移量，下一次读会返回EOF
    return len; // 返回读取的字节数
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
static long led_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int period;
    mutex_lock(&led_mutex); // 加锁，保护LED状态和定时器
    switch(cmd) {
        case LED_IOC_ON:
            blink_period_ms = 0; // 停止闪烁
            led_set(1); // 亮灯
            break;
        case LED_IOC_OFF:
            blink_period_ms = 0; // 停止闪烁
            led_set(0); // 灭灯
            break;
        case LED_IOC_TOGGLE:
            blink_period_ms = 0; // 停止闪烁
            led_set(!led_state); // 翻转状态
            break;
        case LED_IOC_BLINK://内核态和用户态的内存隔离，所以使用copy到一个临时变量period里
            if(copy_from_user(&period, (int __user *)arg, sizeof(period))) {
                mutex_unlock(&led_mutex);
                return -EFAULT; // 从用户空间获取参数失败，返回错误
            }
            if(period <= 0) {
                blink_period_ms = 0; // 停止闪烁
                led_set(0); // 灭灯
            } else {
                blink_period_ms = period; // 设置闪烁周期
                mod_timer(&blink_timer, jiffies + msecs_to_jiffies(blink_period_ms)); // 启动定时器
            }
            break;
        default:
            mutex_unlock(&led_mutex);
            return -EINVAL; // 不支持的命令，返回错误
    }
    mutex_unlock(&led_mutex); // 解锁
    return 0; // 成功返回0
}

static const struct file_operations led_fops = {
    .owner          = THIS_MODULE,
    .write          = led_fops_write,
    .read           = led_fops_read,
    .unlocked_ioctl = led_fops_ioctl,
};

//杂项设备驱动的好处是
//不需要自己注册字符设备号，内核会自动分配次设备号
//不需要自己创建设备节点，内核会自动在/dev目录下创建对应的设备节点
//只需要定义一个miscdevice结构体，指定设备名称和文件操作函数表，就可以完成驱动的注册和使用
static struct miscdevice led_miscdev = {
    .minor = MISC_DYNAMIC_MINOR, // 由内核动态分配次设备号
    .name  = LED_DEV_NAME,       // 设备名称，创建的设备节点为 /dev/lubancat_led
    .fops  = &led_fops,         // 文件操作函数表
};

//模块加载
static int __init led_init(void)
{
    int ret;
    //申请GPIO
    ret = gpio_request(LED_GPIO, LED_DEV_NAME);
    if(ret) {
        pr_err("led: gpio_request(%d) failed: %d\n", LED_GPIO, ret);
        pr_err("led: 请执行 ： echo leds | sudo tee /sys/bus/platform/drivers/leds-gpio/unbind 来解绑默认驱动\n");
        return ret; 
    }

    gpio_direction_output(LED_GPIO, 1); // 设置为输出，初始状态灭灯
    led_state = 0; // 初始化状态变量
    timer_setup(&blink_timer, blink_timer_callback, 0); // 初始化闪烁定时器
    ret = misc_register(&led_miscdev); // 注册杂项设备
    if(ret) {
        pr_err("led: misc_register failed: %d\n", ret);
        gpio_free(LED_GPIO); // 释放GPIO资源
        return ret;
    }
    pr_info("led: driver loaded (GPIO %d -> /dev/%s)\n", LED_GPIO, LED_DEV_NAME);
    return 0;
}

//模块卸载---
static void __exit led_exit(void)
{
    //先删除定时器的原因是：如果定时器还在运行，可能会在回调函数里访问已经释放的资源（比如GPIO），
    //导致内核崩溃。所以我们要先停止定时器，确保回调函数不再被调用了，再去注销设备和释放GPIO。
    blink_period_ms = 0; // 停止闪烁
    del_timer_sync(&blink_timer); // 删除定时器，确保回调函数不再被调用
    led_set(0); // 灭灯

    //这两步才是镜像操作
    misc_deregister(&led_miscdev); // 注销设备
    gpio_free(LED_GPIO); // 释放GPIO资源
    pr_info("led: driver unloaded\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("LED character device driver for LubanCat-4 (GPIO3_B5)");
MODULE_VERSION("1.0");