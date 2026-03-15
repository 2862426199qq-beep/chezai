#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/poll.h>          /* [NEW] poll/select/epoll 支持 */

/* ============================================================
 * 配置参数（根据你的雷达实际情况调整）
 * ============================================================ */
#define FRAME_SIZE       264                          /* 一帧：4帧头 + 256数据 + 4帧尾 */
#define RING_BUF_FRAMES  256                          /* 环形缓冲区容量：256 帧 */
#define RING_BUF_SIZE    (FRAME_SIZE * RING_BUF_FRAMES)  /* = 67584 字节 ≈ 66KB */
#define POLL_INTERVAL_MS 1                            /* 采集周期：1ms = 1000帧/秒 */
#define READ_BUF_FRAMES  16                           /* [NEW] read() 预分配缓冲区：一次最多读 16 帧 */
#define READ_BUF_SIZE    (FRAME_SIZE * READ_BUF_FRAMES)  /* [NEW] = 4224 字节 */

/* ============================================================
 * 设备结构体
 * ============================================================ */
struct fmcw_radar_dev {
    struct spi_device    *spi;          /* SPI 硬件设备 */
    struct miscdevice     misc;         /* misc 设备，自动创建 /dev/fmcw_radar */
    struct mutex          read_lock;    /* 保护用户态 read() 不被并发调用搞乱 */

    /* 环形缓冲区 */
    uint8_t              *ring_buf;     /* 缓冲区本体 */
    int                   head;         /* 写指针 */
    int                   tail;         /* 读指针 */
    atomic_t              count;        /* [FIX] 当前有多少字节（原 int → atomic_t，修复裸读竞争） */
    spinlock_t            ring_lock;    /* 保护 head/tail/count 的自旋锁 */

    /* 等待队列：read() 没数据时睡这里 */
    wait_queue_head_t     wq;

    /* 定时采集 */
    struct hrtimer        poll_timer;   /* 高精度定时器：每 1ms 触发 */
    struct work_struct    spi_work;     /* 工作项：在内核线程里读 SPI */
    struct workqueue_struct *wq_spi;    /* [NEW] 专属工作队列，不和系统共享 */

    /* [NEW] read() 专用的内核临时缓冲区（避免每次 kmalloc） */
    /*
    kmalloc() 调用
        ↓
    内核内存分配器运行    ← 耗时
        ↓
    找到合适的内存块      ← 可能失败（返回NULL）
        ↓
    read()结束后 kfree()  ← 又耗时
        ↓
    内存碎片增加          ← 长期运行变慢*

    把缓冲区read_buf前放在结构体里，驱动加载时分配一次，
    之后每次 read() 直接复用，省去反复申请/释放内存的开销。
    */
    uint8_t              *read_buf;

    /* 生命周期 */
    atomic_t              alive;        /* 1=运行中，0=正在卸载 */
};

/* [FIX] 删除全局指针 static struct fmcw_radar_dev *fmcw_dev;
 * 改为通过 spi_set_drvdata / file->private_data 传递设备指针，
 * 支持多设备实例 */

/* ============================================================
 * 环形缓冲区：写入（内核工作队列调用）
 * ============================================================ */
static void ring_buf_write(struct fmcw_radar_dev *dev,
                           const uint8_t *data, int len)
{
    unsigned long flags;
    int space;
    int first_chunk;
    int second_chunk;
    int cur_count;

    spin_lock_irqsave(&dev->ring_lock, flags);

    cur_count = atomic_read(&dev->count);  /* [FIX] 原 dev->count → atomic_read */
    space = RING_BUF_SIZE - cur_count;

    /* 缓冲区满：丢弃最老的数据，给新数据腾位置 */
    if (len > space) {
        int overflow = len - space;
        dev->tail = (dev->tail + overflow) % RING_BUF_SIZE;
        atomic_sub(overflow, &dev->count);  /* [FIX] 原 dev->count -= overflow */
    }

    /* 环形写入：可能需要分两段（写到末尾后绕回开头） */
    first_chunk = RING_BUF_SIZE - dev->head;
    if (first_chunk >= len) {
        memcpy(dev->ring_buf + dev->head, data, len);
    } else {
        second_chunk = len - first_chunk;
        memcpy(dev->ring_buf + dev->head, data, first_chunk);
        memcpy(dev->ring_buf, data + first_chunk, second_chunk);
    }

    dev->head = (dev->head + len) % RING_BUF_SIZE;
    atomic_add(len, &dev->count);  /* [FIX] 原 dev->count += len */

    spin_unlock_irqrestore(&dev->ring_lock, flags);

    /* 唤醒正在 read() 里睡觉的进程 */
    wake_up_interruptible(&dev->wq);
}

/* ============================================================
 * 环形缓冲区：读出（用户态 read() 调用）
 * ============================================================ */
static int ring_buf_read(struct fmcw_radar_dev *dev,
                         uint8_t *data, int len)
{
    unsigned long flags;
    int available;
    int first_chunk;
    int second_chunk;

    spin_lock_irqsave(&dev->ring_lock, flags);//自旋锁+关中断

    available = atomic_read(&dev->count);  /* [FIX] 原 dev->count → atomic_read */
    if (len > available)
        len = available;//用户想读100字节 但只有50 那就50

    if (len == 0) {
        spin_unlock_irqrestore(&dev->ring_lock, flags);
        return 0;//没数据返回
    }

    /* 环形读出：同样可能分两段 */
    /**
    ┌──────────────────────────────────────────┐
    │   │▓▓▓▓▓▓▓▓▓▓▓▓▓▓│                       │  所以这个是ring_buf，
    └──────────────────────────────────────────┘是一个环形缓冲区，head和tail分别是写指针和读指针。上图中，
        ↑              ↑                        head在前面，tail在后面，表示缓冲区里有数据。用户想读len字节的数据，如果len超过了available（当前缓冲区里有多少字节），就只能读available字节。
        tail           head                     读数据时可能需要分两段：第一段从tail读到缓冲区末尾，第二段从缓冲区开头读剩余的数据。
    */
    first_chunk = RING_BUF_SIZE - dev->tail;//first_chunk是从tail到缓冲区末尾的字节数
    if (first_chunk >= len) {
        memcpy(data,                        //目标 内核临时缓冲区
             dev->ring_buf + dev->tail,     //源 读指针位置，ring_buf是什么？就是之前spi读到的原始数据存的地方。
             len
            );//直接读
    } else {
        /**
        ┌──────────────────────────────────────────┐
        │▓▓▓▓│                    │▓▓▓▓▓▓▓▓▓▓▓▓▓▓  
        └──────────────────────────────────────────┘ 数据绕回（需要两段读取）
              ↑                    ↑
            head                 tail*/

        second_chunk = len - first_chunk;
        memcpy(data, dev->ring_buf + dev->tail, first_chunk);
        memcpy(data + first_chunk, dev->ring_buf, second_chunk);
    }

    dev->tail = (dev->tail + len) % RING_BUF_SIZE;//% RING_BUF_SIZE表示读指针绕回
    atomic_sub(len, &dev->count);  /* [FIX] 原 dev->count -= len */

    spin_unlock_irqrestore(&dev->ring_lock, flags);
    return len;
}

/* ============================================================
 * SPI 工作函数：在内核线程中执行，可以安全调用 spi_sync
 * ============================================================ */
static void fmcw_spi_work_func(struct work_struct *work)
{
    struct fmcw_radar_dev *dev =
        container_of(work, struct fmcw_radar_dev, spi_work);
    uint8_t tx_buf[FRAME_SIZE];
    uint8_t rx_buf[FRAME_SIZE];
    struct spi_transfer t;
    struct spi_message m;
    int ret;

    /* 驱动正在卸载，不再读 SPI */
    if (!atomic_read(&dev->alive))
        return;

    memset(tx_buf, 0, sizeof(tx_buf));
    memset(&t, 0, sizeof(t));
    t.tx_buf = tx_buf;
    t.rx_buf = rx_buf;
    t.len    = FRAME_SIZE;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    ret = spi_sync(dev->spi, &m);
    if (ret < 0) {
        printk(KERN_ERR "[FMCW Radar] SPI 读取失败: %d\n", ret);
        return;
    }

    /* 数据塞进环形缓冲区 */
    ring_buf_write(dev, rx_buf, FRAME_SIZE);
}

/* ============================================================
 * 定时器回调：每 POLL_INTERVAL_MS 毫秒触发一次
 * 运行在硬中断上下文，不能睡眠，只负责 schedule_work
 * ============================================================ */
static enum hrtimer_restart fmcw_timer_callback(struct hrtimer *timer)
{
    /* [FIX] 原代码直接用全局 fmcw_dev 指针
     * 改为 container_of 从 timer 反推 dev，支持多设备 */
    struct fmcw_radar_dev *dev =
        container_of(timer, struct fmcw_radar_dev, poll_timer);

    if (!atomic_read(&dev->alive))
        return HRTIMER_NORESTART;

    /* [FIX] 原 schedule_work() 提交到系统公共 workqueue
     * 改为 queue_work() 提交到专属 workqueue，保证实时性 */
    queue_work(dev->wq_spi, &dev->spi_work);

    hrtimer_forward_now(timer, ms_to_ktime(POLL_INTERVAL_MS));
    return HRTIMER_RESTART;
}

/* ============================================================
 * [NEW] 用户态接口：open()
 * 把设备指针存到 file->private_data，后续 read/poll 都从这里取
 * ============================================================ */
static int fmcw_radar_open(struct inode *inode, struct file *file)
{
    //container_of是一个宏，用于从结构体成员的指针获取到包含该成员的结构体的指针。
    // 在这里，file->private_data 是 miscdevice 结构体的指针，通过 container_of 
    // 可以得到 fmcw_radar_dev 结构体的指针。
    struct fmcw_radar_dev *dev =
        container_of(file->private_data, struct fmcw_radar_dev, misc);
    file->private_data = dev;
    return 0;
}

/* ============================================================
 * 用户态接口：read()
 * 应用程序调�� read(fd, buf, size) 时进入这里
 * ============================================================ */
static ssize_t fmcw_radar_read(struct file *file, char __user *user_buf,
                                size_t count, loff_t *ppos)
{
    /* [FIX] 原代码直接用全局 fmcw_dev，改为从 file->private_data 取 */
    struct fmcw_radar_dev *dev = file->private_data;
    int ret;
    int bytes_read;

    if (count == 0)
        return 0;

    /* [FIX] 原代码限制为 RING_BUF_SIZE，改为限制到预分配 read_buf 大小 */
    if (count > READ_BUF_SIZE)
        count = READ_BUF_SIZE;

    /* 阻塞等待：没数据就睡觉，有数据或驱动卸载时醒来 */
    /* [FIX] 原 fmcw_dev->count > 0 裸读，改为 atomic_read 安全读取 */
    ret = wait_event_interruptible(dev->wq,
              atomic_read(&dev->count) > 0 || !atomic_read(&dev->alive));
    if (ret)
        return -ERESTARTSYS;

    /* 驱动正在卸载，返回 EOF */
    if (!atomic_read(&dev->alive))
        return 0;

    /*
     * 先拷贝到内核临时缓冲区，再 copy_to_user
     * 因为 ring_buf_read 持有 spinlock，而 copy_to_user 可能睡眠
     * 持有 spinlock 时睡眠 = 死锁
     *
     * [FIX] 原代码每次 kmalloc/kfree，改为使用预分配的 dev->read_buf
     */
    mutex_lock(&dev->read_lock);
    bytes_read = ring_buf_read(dev, dev->read_buf, count);
    mutex_unlock(&dev->read_lock);

    if (bytes_read > 0) {
        ret = copy_to_user(user_buf, dev->read_buf, bytes_read);
        if (ret)
            return -EFAULT;
    }

    return bytes_read;
}

/* ============================================================
 * [NEW] 用户态接口：poll()
 * 支持 select() / poll() / epoll()，Qt QSocketNotifier 需要
 * ============================================================ */
static __poll_t fmcw_radar_poll(struct file *file,
                                 struct poll_table_struct *wait)
{
    struct fmcw_radar_dev *dev = file->private_data;
    __poll_t mask = 0;

    /* 注册到等待队列，内核在 wq 被唤醒时会重新调用本函数 */
    poll_wait(file, &dev->wq, wait);

    /* 有数据可读 */
    if (atomic_read(&dev->count) > 0)
        mask |= POLLIN | POLLRDNORM;

    /* 设备已断开 */
    if (!atomic_read(&dev->alive))
        mask |= POLLHUP;

    return mask;
}

/* ============================================================
 * 文件操作表
 * ============================================================ */
static const struct file_operations fmcw_radar_fops = {
    .owner = THIS_MODULE,
    .open  = fmcw_radar_open,      /* [NEW] open 存设备指针 */
    .read  = fmcw_radar_read,
    .poll  = fmcw_radar_poll,      /* [NEW] poll/select/epoll 支持 */
                                    //poll是文件操作表里一个函数指针，用户态调用poll()时内核会调用这个函数
};

/* ============================================================
 * probe：设备匹配成功时调用（驱动的"出生"）
 * ============================================================ */
static int fmcw_radar_probe(struct spi_device *spi)
{
    struct fmcw_radar_dev *dev;  /* [FIX] 原全局指针，改为局部变量 */

    /* 分配设备结构体（devm_ 前缀：设备移除时自动释放） */
    dev = devm_kzalloc(&spi->dev,
                        sizeof(struct fmcw_radar_dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->spi = spi;

    /* [FIX] 把 dev 挂到 spi_device 的 driver_data 上，
     * remove 时通过 spi_get_drvdata 取回，消除全局指针 */
    spi_set_drvdata(spi, dev);

    /* 分配环形缓冲区 */
    dev->ring_buf = devm_kzalloc(&spi->dev, RING_BUF_SIZE, GFP_KERNEL);
    if (!dev->ring_buf)
        return -ENOMEM;

    /* [NEW] 分配 read() 专用缓冲区，避免每次 kmalloc */
    dev->read_buf = devm_kzalloc(&spi->dev, READ_BUF_SIZE, GFP_KERNEL);
    if (!dev->read_buf)
        return -ENOMEM;

    dev->head  = 0;
    dev->tail  = 0;
    atomic_set(&dev->count, 0);  /* [FIX] 原 dev->count = 0 */

    /* 初始化同步原语 */
    mutex_init(&dev->read_lock);
    spin_lock_init(&dev->ring_lock);
    init_waitqueue_head(&dev->wq);
    INIT_WORK(&dev->spi_work, fmcw_spi_work_func);
    atomic_set(&dev->alive, 1);

    /* [NEW] 创建专属工作队列
     * WQ_HIGHPRI: 高优先级，SPI 采集不和普通任务抢
     * WQ_UNBOUND: 不绑定 CPU，调度器自由分配
     * max_active=1: 同时只跑 1 个 work（SPI 不能并发传输） */
    dev->wq_spi = alloc_workqueue("fmcw_spi", WQ_HIGHPRI | WQ_UNBOUND, 1);
    if (!dev->wq_spi) {
        printk(KERN_ERR "[FMCW Radar] 创建工作队列失败\n");
        return -ENOMEM;
    }

    /* 注册 misc 设备 → 创建 /dev/fmcw_radar */
    dev->misc.minor = MISC_DYNAMIC_MINOR;
    dev->misc.name  = "fmcw_radar";
    dev->misc.fops  = &fmcw_radar_fops;

    if (misc_register(&dev->misc) < 0) {
        printk(KERN_ERR "[FMCW Radar] Misc device 注册失败\n");
        destroy_workqueue(dev->wq_spi);  /* [NEW] 清理工作队列 */
        return -EFAULT;
    }

    /* 配置 SPI 模式 */
    spi->mode = SPI_MODE_1;
    spi_setup(spi);

    /* 启动定时器，开始周期性采集 */
    hrtimer_init(&dev->poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    dev->poll_timer.function = fmcw_timer_callback;
    hrtimer_start(&dev->poll_timer,
                  ms_to_ktime(POLL_INTERVAL_MS),
                  HRTIMER_MODE_REL);

    printk(KERN_INFO "[FMCW Radar] V3.2 探测成功！"  /* [FIX] 版本号 V3.1 → V3.2 */
           "环形缓冲区 %d 字节，采集间隔 %dms\n",
           RING_BUF_SIZE, POLL_INTERVAL_MS);
    return 0;
}

/* ============================================================
 * remove：驱动卸载时调用（驱动的"安全关机"）
 *
 * 关停顺序（严格不能乱）：
 *   ① alive=0     宣布死亡，所有异步任务看到后自动停止
 *   ② wake_up     踢醒睡着的 read()，防止僵尸进程
 *   ③ hrtimer     停定时器，不再产生新的 schedule_work
 *   ④ work_sync   等当前 SPI 读取结束
 *   ⑤ workqueue   销毁专属工作队列
 *   ⑥ deregister  最后注销设备节点
 * ============================================================ */
static int fmcw_radar_remove(struct spi_device *spi)
{
    /* [FIX] 原代码直接用全局 fmcw_dev，改为从 spi 取回设备指针 */
    struct fmcw_radar_dev *dev = spi_get_drvdata(spi);

    atomic_set(&dev->alive, 0);
    wake_up_interruptible(&dev->wq);
    hrtimer_cancel(&dev->poll_timer);
    cancel_work_sync(&dev->spi_work);
    destroy_workqueue(dev->wq_spi);     /* [NEW] ⑤ 销毁专属工作队列 */
    misc_deregister(&dev->misc);

    printk(KERN_INFO "[FMCW Radar] 驱动已安全卸载！\n");
    return 0;
}

/* ============================================================
 * 设备树匹配表
 * ============================================================ */
static const struct of_device_id fmcw_radar_dt_ids[] = {
    { .compatible = "fmcw,radar" },   /* 专属 compatible，避免与 spidev 冲突 */
    {},
};
MODULE_DEVICE_TABLE(of, fmcw_radar_dt_ids);

static struct spi_driver fmcw_radar_driver = {
    .driver = {
        .name = "fmcw_radar",
        .owner = THIS_MODULE,
        .of_match_table = fmcw_radar_dt_ids,
    },
    .probe  = fmcw_radar_probe,
    .remove = fmcw_radar_remove,
};
module_spi_driver(fmcw_radar_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xie Xinyu");
MODULE_DESCRIPTION("24-26GHz FMCW Radar SPI Kernel Driver V3.2");  /* [FIX] 版本号更新 */