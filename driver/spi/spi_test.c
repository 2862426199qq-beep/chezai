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
    uint32_t speed = 1000000; // 1MHz 稳妥起见
    uint8_t mode = SPI_MODE_0; // 极性相位配置，需和 STM32 保持一致
    
    // 我们要接收 4 个字节的雷达数据测试
    uint8_t tx[] = {0x00, 0x00, 0x00, 0x00}; 
    uint8_t rx[4] = {0, };

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 4,
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
        printf("收到数据: 0x%02X 0x%02X 0x%02X 0x%02X\n", rx[0], rx[1], rx[2], rx[3]);
    }

    close(fd);
    return 0;
}