#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/*
 * 用滑动窗口方式持续积累数据并搜索帧头
 * 和 test_processor 的解帧逻辑一模一样
 */
int main()
{
    int fd;
    uint8_t buf[4096];
    int data_len = 0;
    int read_count = 0;
    int header_found = 0;

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开设备");
        return -1;
    }

    printf("滑动窗口搜索帧头（最多读 20 次）...\n\n");

    while (read_count < 20 && !header_found) {
        int space = 4096 - data_len;
        if (space <= 0) {
            /* 缓冲区满了还没找到，清空重来 */
            printf("  缓冲区满，清空重来\n");
            data_len = 0;
            continue;
        }

        int bytes = read(fd, buf + data_len, space);
        if (bytes <= 0) break;
        data_len += bytes;
        read_count++;

        printf("第 %2d 次读取: +%d 字节, 累计 %d 字节\n",
               read_count, bytes, data_len);

        /* 在累积的数据里搜索帧头 */
        int i;
        for (i = 0; i <= data_len - 4; i++) {
            if (buf[i]   == 0xAA && buf[i+1] == 0x30 &&
                buf[i+2] == 0xFE && buf[i+3] == 0xFF) {
                printf("\n  ★★★ 在累计偏移 %d 处发现帧头 AA 30 FE FF ！★★★\n", i);

                /* 从帧头开始打印一整帧 */
                if (i + 264 <= data_len) {
                    printf("  完整帧前 16 字节: ");
                    int j;
                    for (j = i; j < i + 16; j++)
                        printf("%02X ", buf[j]);
                    printf("\n");

                    printf("  完整帧后 8 字节:  ");
                    for (j = i + 256; j < i + 264; j++)
                        printf("%02X ", buf[j]);
                    printf("\n");

                    /* 检查帧尾 */
                    if (buf[i+260] == 0xFE && buf[i+261] == 0xFE &&
                        buf[i+262] == 0xFE && buf[i+263] == 0x55) {
                        printf("  ✅ 帧尾 FE FE FE 55 正确！\n");
                    } else {
                        printf("  ❌ 帧尾不对！\n");
                    }

                    /* 看看 264 字节之后有没有下一个帧头 */
                    if (i + 264 + 4 <= data_len) {
                        if (buf[i+264] == 0xAA && buf[i+265] == 0x30 &&
                            buf[i+266] == 0xFE && buf[i+267] == 0xFF) {
                            printf("  ✅ 下一帧帧头也在！帧间距 = 264 字节！\n");
                        } else {
                            printf("  下一帧偏移处: %02X %02X %02X %02X\n",
                                   buf[i+264], buf[i+265], buf[i+266], buf[i+267]);
                        }
                    }
                } else {
                    printf("  数据不够一整帧，继续读...\n");
                }

                header_found = 1;
                break;
            }
        }

        if (!header_found) {
            /* 保留最后 3 字节（帧头可能跨读取边界） */
            if (data_len > 3) {
                memmove(buf, buf + data_len - 3, 3);
                data_len = 3;
            }
        }
    }

    if (!header_found) {
        printf("\n读了 20 次都没找到帧头！\n");
        printf("请确认 STM32 帧头是 AA 30 FE FF\n");
    }

    close(fd);
    return 0;
}