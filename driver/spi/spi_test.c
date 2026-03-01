// 文件名：spi_test.c
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

int main() {
    int fd;
    int ret = 0;
    
    uint8_t mode = SPI_MODE_0; // 极性相位配置，需和 STM32 保持一致
    
    // 我们要接收 4 个字节的雷达数据测试
 // ... 前面保持不变 ...
    uint8_t tx[264] = {0}; 
    uint8_t rx[264] = {0}; // 扩大接收缓冲区

    // 稍微降低一点时钟频率，排除杜邦线太长导致的信号干扰
    uint32_t speed = 1000000; // 500kHz

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 264, // 一次性抽 32 个字节出来
        .speed_hz = speed,
    };

    fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        perror("打不开 SPI 设备");
        return -1;
    }

    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    printf("开始向 STM32 请求数据...\n");
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        perror("SPI 传输失败");
    } else {
        printf("收到数据:\n");
        // 专业的 Hexdump 打印格式，方便找规律
        for(int i = 0; i < 264; i++) {
            printf("%02X ", rx[i]);
            if ((i + 1) % 8 == 0) printf("\n"); // 每 8 个字节换一行
        }
    }

    close(fd);
    return 0;
}