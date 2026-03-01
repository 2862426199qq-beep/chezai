#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "radar_processor.h"

/* 新帧结构：264 字节 = 4帧头 + 256 IQ + 2序号 + 2帧尾 */
#define FRAME_SIZE  264
#define BUFFER_SIZE (264 * 100)

static volatile int running = 1;
static void sig_handler(int) { running = 0; }

static int find_header(const uint8_t *buf, int len)
{
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i]   == 0xAA && buf[i+1] == 0x30 &&
            buf[i+2] == 0xFE && buf[i+3] == 0xFF)
            return i;
    }
    return -1;
}

int main()
{
    uint8_t rx_buf[BUFFER_SIZE];
    int data_len = 0;
    int new_frame_count = 0;

    std::memset(rx_buf, 0, sizeof(rx_buf));
    signal(SIGINT, sig_handler);

    int fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        perror("打不开雷达设备");
        return -1;
    }

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  FMCW 雷达信号处理中间件 V1.1 (帧序号版)        ║\n");
    printf("║  载波: 25GHz  带宽: 60MHz  分辨率: 2.5m          ║\n");
    printf("║  按 Ctrl+C 退出                                 ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    RadarProcessor processor;

    while (running) {
        int space = BUFFER_SIZE - data_len;
        if (space <= 0) { data_len = 0; continue; }

        int bytes = read(fd, rx_buf + data_len, space);
        if (bytes < 0) { perror("read"); break; }
        if (bytes == 0) { printf("设备断开\n"); break; }
        data_len += bytes;

        while (data_len >= FRAME_SIZE && running) {
            int pos = find_header(rx_buf, data_len);
            if (pos < 0) { data_len = 0; break; }
            if (pos > 0) {
                std::memmove(rx_buf, rx_buf + pos, data_len - pos);
                data_len -= pos;
            }
            if (data_len < FRAME_SIZE) break;

            /* 提取 IQ 数据（偏移 4） */
            IQPoint *raw_iq = reinterpret_cast<IQPoint*>(&rx_buf[4]);

            /* 提取帧序号（偏移 4 + 256 = 260） */
            uint16_t frame_seq = *reinterpret_cast<uint16_t*>(&rx_buf[260]);

            /* 处理 */
            std::vector<RadarTarget> targets = processor.process(raw_iq, frame_seq);

            if (processor.is_new_frame()) {
                new_frame_count++;

                /* 每 20 个新帧打印一次 ≈ 1秒 */
                if (new_frame_count % 20 == 0) {
                    printf("━━━ 新帧 #%d (seq=%u) ━━━━━━━━━━━━━━━━━━━━━━\n",
                           new_frame_count, frame_seq);

                    if (targets.empty()) {
                        printf("  无目标\n");
                    } else {
                        printf("  %-8s %-6s %-10s %-12s %-10s\n",
                               "类型", "Bin", "距离(m)", "速度(m/s)", "幅度");
                        printf("  %-8s %-6s %-10s %-12s %-10s\n",
                               "────", "───", "──────", "────────", "────");
                        for (const auto& t : targets) {
                            printf("  %-8s %-6d %7.1f m  %+7.2f m/s  %.0f\n",
                                   t.label().c_str(),
                                   t.bin,
                                   t.range_m,
                                   t.velocity_mps,
                                   t.amplitude);
                        }
                    }
                    printf("\n");
                }
            }

            std::memmove(rx_buf, rx_buf + FRAME_SIZE, data_len - FRAME_SIZE);
            data_len -= FRAME_SIZE;
        }
    }

    close(fd);
    printf("\n共处理 %d 个新帧\n", new_frame_count);
    return 0;
}