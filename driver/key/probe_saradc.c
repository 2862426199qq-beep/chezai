/*
 * probe_saradc.c — SARADC 寄存器探测模块
 *
 * 目的: 确定 RK3588 SARADC v2 的数据寄存器偏移量
 * 用法: insmod probe_saradc.ko → 看 dmesg 输出 → rmmod probe_saradc
 *
 * 背景知识:
 *   RK3588 SARADC v2 的数据寄存器在不同 BSP 版本中偏移量不同:
 *     - 方案A: DATA(ch) = 0x2C + ch*4  (部分 Rockchip BSP)
 *     - 方案B: DATA(ch) = 0x80 + ch*4  (另一些 BSP 版本)
 *   我们用这个小模块来确定你板子上用的是哪个
 */
 #include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#define SARADC_BASE     0xFEC10000
#define SARADC_SIZE     0x10000

/*
 * SARADC v2 转换控制寄存器 (CONV_CON)
 *
 * RK3588 寄存器写入机制 (和你 DHT11 里操作上拉寄存器一样):
 *   低 16 位 = 数据
 *   高 16 位 = 写掩码 (对应位置1才允许写低16位)
 *
 * BIT(1) = 选择通道1 (Recovery按键)
 * BIT(4) = 启动转换
 * BIT(5) = 单次模式
 */
#define CONV_CON        0x000
#define CH1_START_SINGLE (1 | BIT(4) | BIT(5))  /* = 0x31，通道1是数值1，不是BIT(1) */  /* = 0x32 */

/* 两种可能的数据寄存器偏移 */
#define DATA_OFFSET_A   0x02C   /* 方案A: 0x2C + 1*4 = 0x30 (ch1) */
#define DATA_OFFSET_B   0x080   /* 方案B: 0x80 + 1*4 = 0x84 (ch1) */

static int __init probe_init(void)
{
    void __iomem *base;
    u32 val_a, val_b;
    u32 ctrl;

    base = ioremap(SARADC_BASE, SARADC_SIZE);
    if (!base) {
        pr_err("probe: ioremap failed\n");
        return -ENOMEM;
    }

    /* 配置时序参数 (和内核 SARADC 驱动一致) */
    writel_relaxed(0x000c000c, base + 0x004);  /* T_PD_SOC */
    writel_relaxed(0x000c000c, base + 0x00C);  /* T_DAS_SOC */

    /* 触发通道1单次转换 */
    ctrl = CH1_START_SINGLE;
    writel_relaxed(ctrl | (ctrl << 16), base + CONV_CON);

    /* 等待转换完成 (~10us 足够，保守等 100us) */
    udelay(100);

    /* 从两个候选偏移读取数据 */
    val_a = readl_relaxed(base + DATA_OFFSET_A + 1 * 4);  /* 0x30 */
    val_b = readl_relaxed(base + DATA_OFFSET_B + 1 * 4);  /* 0x84 */

    pr_info("========== SARADC 寄存器探测结果 ==========\n");
    pr_info("方案A (0x%03X): ch1 = %u\n", DATA_OFFSET_A + 4, val_a & 0xFFF);
    pr_info("方案B (0x%03X): ch1 = %u\n", DATA_OFFSET_B + 4, val_b & 0xFFF);
    pr_info("期望值: ~4090 (未按键) 或 ~0 (按下Recovery键)\n");
    pr_info("匹配 IIO 读到的值那个就是正确的偏移\n");
    pr_info("============================================\n");

    iounmap(base);
    return -EAGAIN;  /* 故意返回错误，让模块不驻留 */
}

static void __exit probe_exit(void) {}

module_init(probe_init);
module_exit(probe_exit);
MODULE_LICENSE("GPL");