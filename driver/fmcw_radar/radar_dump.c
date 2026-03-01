#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    uint8_t buf[528];  /* 刚好两帧 */
    int bytes;
    int i;

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开设备");
        return -1;
    }

    /* 读两帧的数据量 */
    int total = 0;
    while (total < 528) {
        bytes = read(fd, buf + total, 528 - total);
        if (bytes <= 0) break;
        total += bytes;
    }

    printf("共 %d 字节，完整 hex dump:\n\n", total);

    for (i = 0; i < total; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0)
            printf("  | 偏移 %3d-%3d\n", i - 15, i);
    }
    printf("\n");

    /* 搜索所有可能的部分帧头 */
    printf("\n搜索 AA 出现的位置:\n");
    for (i = 0; i < total; i++) {
        if (buf[i] == 0xAA)
            printf("  偏移 %d: %02X %02X %02X %02X\n",
                   i, buf[i],
                   i+1 < total ? buf[i+1] : 0,
                   i+2 < total ? buf[i+2] : 0,
                   i+3 < total ? buf[i+3] : 0);
    }

    printf("\n搜索 FE FE 出现的位置:\n");
    for (i = 0; i < total - 1; i++) {
        if (buf[i] == 0xFE && buf[i+1] == 0xFE)
            printf("  偏移 %d: %02X %02X %02X %02X\n",
                   i, buf[i], buf[i+1],
                   i+2 < total ? buf[i+2] : 0,
                   i+3 < total ? buf[i+3] : 0);
    }

    close(fd);
    return 0;
}