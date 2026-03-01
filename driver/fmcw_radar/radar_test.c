/*
 * FMCW Radar 测试程序 V3.1
 *
 * 配合内核驱动 V3.1 使用
 * 驱动端有环形缓冲区 + 阻塞 read()，所以应用端不需要 usleep 轮询
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define FRAME_SIZE  264
#define BUFFER_SIZE (264 * 100)

#pragma pack(push, 1)
struct radar_iq_point {
    int16_t i;
    int16_t q;
};
#pragma pack(pop)

static volatile int running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

/* 在缓冲区中查找帧头 AA 30 FE FF，返回偏移量，找不到返回 -1 */
static int find_header(const uint8_t *buf, int len)
{
    int i;
    for (i = 0; i <= len - 4; i++) {
        if (buf[i]   == 0xAA && buf[i+1] == 0x30 &&
            buf[i+2] == 0xFE && buf[i+3] == 0xFF)
            return i;
    }
    return -1;
}

int main()
{
    int fd;
    uint8_t rx_buf[BUFFER_SIZE];
    int data_len = 0;
    int frame_count = 0;

    memset(rx_buf, 0, sizeof(rx_buf));
    signal(SIGINT, sig_handler);

    fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开雷达设备");
        return -1;
    }

    printf("成功连接雷达设备 V3.1！按 Ctrl+C 退出\n\n");

    while (running) {

        /* 一次尽量多读，内核的 read() 自带阻塞等待 */
        int space = BUFFER_SIZE - data_len;
        if (space <= 0) {
            printf("[警告] 缓冲区满，清空重来\n");
            data_len = 0;
            continue;
        }

        int bytes = read(fd, rx_buf + data_len, space);

        if (bytes < 0) {
            perror("read() 失败");
            break;
        }
        if (bytes == 0) {
            /* EOF：驱动正在卸载 */
            printf("设备已断开\n");
            break;
        }

        data_len += bytes;

        /* 滑动窗口解帧 */
        while (data_len >= FRAME_SIZE) {

            int pos = find_header(rx_buf, data_len);

            if (pos < 0) {
                data_len = 0;
                break;
            }

            if (pos > 0) {
                memmove(rx_buf, rx_buf + pos, data_len - pos);
                data_len -= pos;
            }

            if (data_len < FRAME_SIZE)
                break;

            frame_count++;
            struct radar_iq_point *points =
                (struct radar_iq_point *)(&rx_buf[4]);

            /* 每 200 帧打印一次，不拖慢主循环 */
            if (frame_count % 200 == 0) {
                printf("帧 #%d | 点[0]: I=%6d Q=%6d | 点[63]: I=%6d Q=%6d\n",
                       frame_count,
                       points[0].i, points[0].q,
                       points[63].i, points[63].q);
            }

            /* 挖走这一帧 */
            memmove(rx_buf, rx_buf + FRAME_SIZE, data_len - FRAME_SIZE);
            data_len -= FRAME_SIZE;
        }

        /*
         * 不需要 usleep！
         * 内核驱动的 read() 自带 wait_event_interruptible 阻塞
         * 没数据时 read() 自动睡觉，有数据时立刻返回
         */
    }

    close(fd);
    printf("\n共解析 %d 帧\n", frame_count);

    return 0;
}

/**  =================================滑动窗口详解=================================
先搞懂"水池"长什么样
想象 rx_buf 是一条水槽，数据从右边灌进来，从左边消费掉：

Code
rx_buf（水池）：
┌────────────────────────────────────────────────────────────┐
│  已有的数据（data_len 个字节）            │   空闲空间     │
└────────────────────────────────────────────────────────────┘
↑ rx_buf[0]                              ↑ rx_buf[data_len]  ↑ rx_buf[2047]
每次 read() 就是往水池右边灌水，data_len 就是当前水位。

滑动窗口的核心代码（只有 20 行）
sliding_window_core.c
v1
while (data_len >= FRAME_SIZE) {       // 条件：水池里至少要有 264 字节，才可能包含一帧完整数据

    // 检查水池最前面 4 个字节是不是帧头
    if (rx_buf[0] == 0xAA && rx_buf[1] == 0x30 &&
        rx_buf[2] == 0xFE && rx_buf[3] == 0xFF) {

用图一步一步走一遍
假设 SPI 传回来的数据里，前面混了 2 个垃圾字节：

初始状态：水池里有 266 字节
Code
data_len = 266

rx_buf:
[0x00][0xFF][0xAA][0x30][0xFE][0xFF][I0_lo][I0_hi][Q0_lo]...[帧尾]
  ↑
  rx_buf[0] = 0x00 ≠ 0xAA  →  不是帧头！
第 1 轮：扔掉 1 字节
Code
走 else 分支：memmove(rx_buf, rx_buf + 1, 265)

所有数据往前挪 1 格：
[0xFF][0xAA][0x30][0xFE][0xFF][I0_lo][I0_hi][Q0_lo]...[帧尾]
  ↑
  rx_buf[0] = 0xFF ≠ 0xAA  →  还不是帧头！

data_len = 265
第 2 轮：再扔掉 1 字节
Code
走 else 分支：memmove(rx_buf, rx_buf + 1, 264)

所有数据再往前挪 1 格：
[0xAA][0x30][0xFE][0xFF][I0_lo][I0_hi][Q0_lo]...[帧尾]
  ↑
  rx_buf[0] = 0xAA ✅
  rx_buf[1] = 0x30 ✅
  rx_buf[2] = 0xFE ✅
  rx_buf[3] = 0xFF ✅  →  帧头命中！！！

data_len = 264
第 3 轮：解析整帧，然后挖走
Code
走 if 分支：

1. 解析 rx_buf[4] ~ rx_buf[259] 里的 64 个 IQ 点
2. memmove(rx_buf, rx_buf + 264, 0)  ← 后面没有剩余数据了
3. data_len = 264 - 264 = 0

水池空了！
第 4 轮：循环条件检查
Code
while (data_len >= FRAME_SIZE)
       ↓
while (0 >= 264)  →  false  →  退出内层 while，回到外层继续 read()*/