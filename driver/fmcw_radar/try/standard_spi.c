#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>


//私有设备结构体
struct fmcw_radar_dev{
    struct spi_device *spi;
    struct miscdevice misc;

    struct mutex read_lock;//保护用户态 read() 不被并发调用搞乱

    //配置环形缓冲区
    uint8_t *ring_buf;//缓冲区本体
    int head;//写指针
    int tail;//读指针
    atomic_t count;//当前有多少字节 int 改为 atomic_t，修复裸读竞争
    spinlock_t ring_lock;//保护 head/tail/count 的自旋锁

    //等待队列：read() 没数据时睡这里
    wait_queue_head_t wq;

    //定时采集
    struct hrtimer poll_timer;//高精度定时器：每 1ms
    struct work_struct spi_work;//工作项：在内核线程里读 SPI
    struct workqueue_struct *wq_spi;//专属工作队列，不和系统共享

    //read() 专用的内核临时缓冲区（避免每次 kmalloc）kmalloc是内核空间的动态内存分配函数，类似于用户空间的 malloc。每次调用 kmalloc 都会有一定的开销，尤其是在频繁调用 read() 的情况下。预分配一个 read_buf 可以避免每次读操作都进行动态内存分配，提高性能。
    uint8_t *read_buf;
    
};

static ssize_t fmcw_radar_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos){
    //现在啥也不读 就打开设备
    
    return 0;
}
//inode 是文件系统的概念 代表一个文件 通过 container_of 从 miscdevice 结构体中获取到 fmcw_radar_dev 结构体
static int fmcw_radar_open(struct inode *inode, struct file *file){
    struct fmcw_radar_dev *dev;
    //misc_open会不秘书device结构体的指针放到file->private_data中
    dev = container_of(file->private_data, struct fmcw_radar_dev, misc);
    file->private_data = dev;//以后read poll 都从这里取dev

    return 0;
}

static const struct file_operations fmcw_radar_fops = {
    .owner = THIS_MODULE,
    .open = fmcw_radar_open,
    .read = fmcw_radar_read,
};

//匹配设备树节点
static const struct of_device_id fmcw_radar_dt_ids[] = {
    { .compatible = "fmcw,radar" },
    {},
};
MODULE_DEVICE_TABLE(of, fmcw_radar_dt_ids);
//什么意思：这个宏会将设备树匹配表的信息导出到内核模块的符号表中，
// 使得内核能够正确地识别和匹配设备树中的节点与驱动程序。
// 这是设备树驱动程序中常见的一个步骤，确保驱动程序能够被正确加载和绑定到相应的硬件设备上。

//spi 是内核的总线类型 这个宏会将驱动程序注册为一个spi驱动程序
//dev是spi设备结构体 代表一个spi设备，比如我们的雷达模块
static int fmcw_radar_probe(struct spi_device *spi){
    int ret;
    struct fmcw_radar_dev *dev;//私有设备结构体指针
    dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);//分配设备结构体
    if(!dev){
        return -ENOMEM;
    }
    dev->spi  = spi;//保存 spi 设备指针
    spi_set_drvdata(spi, dev);//把设备结构体指针挂到 spi 设备的 driver_data 上，方便以后取回

    dev->misc.minor = MISC_DYNAMIC_MINOR;//动态分配次设备号
    dev->misc.name = "fmcw_radar";//设备名字 /dev/fmcw_radar
    dev->misc.fops = &fmcw_radar_fops;//文件操作函数表
    ret = misc_register(&dev->misc);//注册 misc 设备，成功后会在 /dev/ 下创建设备节点 /dev/fmcw_radar
    if (ret) {
        dev_err(&spi->dev, "failed to register misc device\n");
        return ret;
    }
    //内核日志
    dev_info(&spi->dev, "FMCW radar step1 probe OK, /dev/%s created\n",
             dev->misc.name);
 
    return 0;
}
static int fmcw_radar_remove(struct spi_device *spi){
    struct fmcw_radar_dev *dev = spi_get_drvdata(spi);//从 spi 设备的 driver_data 上取回设备结构体指针
    misc_deregister(&dev->misc);//反注册 misc 设备，删除 /dev/fmcw_radar 设备节点
    dev_info(&spi->dev, "FMCW radar removed, /dev/%s deleted\n",
             dev->misc.name);
    return 0;
}

static struct spi_driver fmcw_radar_driver = {
    .driver = {
        .name = "fmcw_radar",
        .owner = THIS_MODULE,
        .of_match_table = fmcw_radar_dt_ids,
    },
    .probe = fmcw_radar_probe,
    .remove = fmcw_radar_remove,
};
module_spi_driver(fmcw_radar_driver);//注册 spi 驱动

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("FMCW Radar SPI Driver - Step1 skeleton");
MODULE_VERSION("0.1");
