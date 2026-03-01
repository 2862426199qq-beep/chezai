#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    uint8_t buf[264];
    int bytes;
    int i;

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开设备");
        return -1;
    }

    printf("读取 3 次原始数据...\n\n");

    for (i = 0; i < 3; i++) {
        bytes = read(fd, buf, 264);
        printf("第 %d 次读取：%d 字节\n", i + 1, bytes);

        if (bytes > 0) {
            /* 打印前 16 字节（看帧头在不在） */
            int j;
            int show = bytes;
            if (show > 16)
                show = 16;

            printf("  前 %d 字节: ", show);
            for (j = 0; j < show; j++)
                printf("%02X ", buf[j]);
            printf("\n");

            /* 打印最后 8 字节（看帧尾在不在） */
            if (bytes >= 8) {
                printf("  后 8 字节:  ");
                for (j = bytes - 8; j < bytes; j++)
                    printf("%02X ", buf[j]);
                printf("\n");
            }

            /* 统计全零字节数 */
            int zero_count = 0;
            for (j = 0; j < bytes; j++) {
                if (buf[j] == 0x00)
                    zero_count++;
            }
            printf("  全零字���: %d / %d (%.1f%%)\n", 
                   zero_count, bytes, 100.0 * zero_count / bytes);

            /* 找找 AA 在哪里 */
            for (j = 0; j < bytes - 3; j++) {
                if (buf[j] == 0xAA && buf[j+1] == 0x30 &&
                    buf[j+2] == 0xFE && buf[j+3] == 0xFF) {
                    printf("  ★ 在偏移 %d 处发现帧头 AA 30 FE FF ！\n", j);
                }
            }
        }
        printf("\n");
    }

    close(fd);
    return 0;
}