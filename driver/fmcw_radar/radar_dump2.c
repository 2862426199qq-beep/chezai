#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    uint8_t buf[536];  /* 两帧 268×2 */
    int total = 0, bytes;

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) { perror("打不开"); return -1; }

    while (total < 536) {
        bytes = read(fd, buf + total, 536 - total);
        if (bytes <= 0) break;
        total += bytes;
    }

    printf("共 %d 字节\n\n", total);

    /* 搜索帧头 */
    int i;
    for (i = 0; i <= total - 268; i++) {
        if (buf[i]==0xAA && buf[i+1]==0x30 && buf[i+2]==0xFE && buf[i+3]==0xFF) {
            uint16_t seq   = *(uint16_t*)&buf[i+260];
            uint16_t angle = *(uint16_t*)&buf[i+262];
            uint8_t t0 = buf[i+264], t1 = buf[i+265];
            /* 另外检查原来的264偏移 */
            uint8_t old_t0 = buf[i+260], old_t1 = buf[i+261];

            printf("帧头在偏移 %d:\n", i);
            printf("  前4字节: %02X %02X %02X %02X\n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
            printf("  偏移260-265: %02X %02X %02X %02X %02X %02X\n",
                   buf[i+260], buf[i+261], buf[i+262], buf[i+263], buf[i+264], buf[i+265]);
            printf("  帧序号: %u\n", seq);
            printf("  扫描角度: %u (= %.1f°)\n", angle, angle/10.0);
            printf("  帧尾(266-267): %02X %02X\n", buf[i+266], buf[i+267]);

            if (buf[i+266]==0xFE && buf[i+267]==0x55)
                printf("  ✅ 帧尾 FE 55 正确！帧大小 268\n");
            else if (t0==0xFE && t1==0x55)
                printf("  ⚠ 帧尾在偏移264！STM32还在用264字节旧帧！\n");
            else
                printf("  ❌ 帧尾位置异常\n");
            printf("\n");
        }
    }

    close(fd);
    return 0;
}
