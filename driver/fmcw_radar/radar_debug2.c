#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    uint8_t buf[2048];  /* 一次读更多数据 */
    int total = 0;
    int bytes;
    int i;

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开设备");
        return -1;
    }

    printf("连续读取数据，搜索帧头和帧尾...\n\n");

    /* 连续读几次，积累足够多的数据 */
    while (total < 2048) {
        bytes = read(fd, buf + total, 2048 - total);
        if (bytes <= 0) break;
        total += bytes;
    }

    printf("共读取 %d 字节\n\n", total);

    /* 打印前 32 字节原始数据 */
    printf("前 32 字节:\n  ");
    for (i = 0; i < 32 && i < total; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n\n");

    /* 搜索帧头 AA 30 FE FF */
    printf("搜索帧头 AA 30 FE FF:\n");
    int header_count = 0;
    for (i = 0; i <= total - 4; i++) {
        if (buf[i] == 0xAA && buf[i+1] == 0x30 &&
            buf[i+2] == 0xFE && buf[i+3] == 0xFF) {
            printf("  ★ 偏移 %d (0x%03X) 处发现帧头\n", i, i);
            header_count++;

            /* 打印帧头后面 8 字节 */
            printf("    后续 8 字节: ");
            int j;
            for (j = i; j < i + 8 && j < total; j++)
                printf("%02X ", buf[j]);
            printf("\n");
        }
    }
    if (header_count == 0)
        printf("  未找到帧头！\n");

    /* 搜索帧尾 FE FE FE 55 */
    printf("\n搜索帧尾 FE FE FE 55:\n");
    int tail_count = 0;
    for (i = 0; i <= total - 4; i++) {
        if (buf[i] == 0xFE && buf[i+1] == 0xFE &&
            buf[i+2] == 0xFE && buf[i+3] == 0x55) {
            printf("  ★ 偏移 %d (0x%03X) 处发现帧尾\n", i, i);
            tail_count++;
        }
    }
    if (tail_count == 0)
        printf("  未找到帧尾！\n");

    printf("\n统计: 帧头 %d 个, 帧尾 %d 个\n", header_count, tail_count);

    /* 字节频率分析 */
    printf("\n高频字节 TOP5:\n");
    int freq[256] = {0};
    for (i = 0; i < total; i++)
        freq[buf[i]]++;
    for (int round = 0; round < 5; round++) {
        int max_val = 0, max_idx = 0;
        for (i = 0; i < 256; i++) {
            if (freq[i] > max_val) {
                max_val = freq[i];
                max_idx = i;
            }
        }
        printf("  0x%02X: %d 次 (%.1f%%)\n",
               max_idx, max_val, 100.0 * max_val / total);
        freq[max_idx] = 0;
    }

    close(fd);
    return 0;
}