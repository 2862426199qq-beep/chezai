#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/mod_devicetable.h>

// 当内核在设备树中发现匹配的雷达节点时，会自动调用这个 probe 函数
static int fmcw_radar_probe(struct spi_device *spi)
{
    printk(KERN_INFO "[FMCW Radar] 硬件探测成功！\n");
    printk(KERN_INFO "[FMCW Radar] 当前配置的 SPI 最大时钟频率: %d Hz\n", spi->max_speed_hz);
    
    // 我们后续会在这里注册字符设备节点 (/dev/fmcw_radar) 和申请硬件中断
    
    return 0;
}

// 当设备被拔出或驱动被卸载时调用
static int fmcw_radar_remove(struct spi_device *spi)
{
    printk(KERN_INFO "[FMCW Radar] 驱动已卸载！\n");
    return 0;
}

// 这是驱动的“身份证”，用于和设备树（Device Tree）里的节点进行相亲匹配
static const struct of_device_id fmcw_radar_dt_ids[] = {
    { .compatible = "rohm,dh2228fv" }, // 兼容当前野火插件默认的 spidev 名字
    { .compatible = "spidev" },
    { .compatible = "xinyu,fmcw-radar" }, // 为你未来专属的设备树节点预留
    {},
};
MODULE_DEVICE_TABLE(of, fmcw_radar_dt_ids);

// 定义 SPI 驱动结构体
static struct spi_driver fmcw_radar_driver = {
    .driver = {
        .name = "fmcw_radar",
        .owner = THIS_MODULE,
        .of_match_table = fmcw_radar_dt_ids,
    },
    .probe = fmcw_radar_probe,
    .remove = fmcw_radar_remove,
};

// 一键注册驱动宏
module_spi_driver(fmcw_radar_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xie Xinyu");
MODULE_DESCRIPTION("24-26GHz FMCW Radar SPI Kernel Driver");