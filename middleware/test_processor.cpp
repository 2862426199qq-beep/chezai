#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "radar_processor.h"

#define FRAME_SIZE  RadarConfig::FRAME_SIZE   /* 264 */
#define BUFFER_SIZE (FRAME_SIZE * 100)

static volatile int running = 1;
static void sig_handler(int) { running = 0; }

static int find_header(const uint8_t *buf, int len)
{
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i] == 0xAA && buf[i+1] == 0x30 &&
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

    /* 鲁班猫端模拟扫描角度：120°/s，每新帧 +6°（50ms × 120°/s） */
    double sim_angle = 0.0;

    std::memset(rx_buf, 0, sizeof(rx_buf));
    signal(SIGINT, sig_handler);

    int fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) { perror("打不开雷达设备"); return -1; }

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  FMCW 雷达信号处理中间件 V2.0 (角度模拟版)      ║\n");
    printf("║  载波: 60MHz  带宽: 60MHz  分辨率: 2.5m          ║\n");
    printf("║  扫描速度: 120°/s  帧大小: 264B                  ║\n");
    printf("║  按 Ctrl+C 退出                                 ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    RadarProcessor processor;

    while (running) {
        int space = BUFFER_SIZE - data_len;
        if (space <= 0) { data_len = 0; continue; }

        int bytes = read(fd, rx_buf + data_len, space);
        if (bytes <= 0) break;
        data_len += bytes;

        while (data_len >= FRAME_SIZE && running) {
            int pos = find_header(rx_buf, data_len);
            if (pos < 0) { data_len = 0; break; }
            if (pos > 0) {
                std::memmove(rx_buf, rx_buf + pos, data_len - pos);
                data_len -= pos;
            }
            if (data_len < FRAME_SIZE) break;

            IQPoint  *raw_iq    = reinterpret_cast<IQPoint*>(&rx_buf[4]);
            uint16_t  frame_seq = *reinterpret_cast<uint16_t*>(&rx_buf[260]);

            auto targets = processor.process(raw_iq, frame_seq, sim_angle);

            if (processor.is_new_frame()) {
                new_frame_count++;

                /* 每新帧推进角度 */
                sim_angle += 6.0;  /* 120°/s × 0.05s = 6° */
                if (sim_angle >= 360.0) sim_angle -= 360.0;

                if (new_frame_count % 20 == 0) {
                    printf("━━━ 帧 #%d  seq=%u  角度=%.1f° ━━━━━━━━━━━━━━━\n",
                           new_frame_count, frame_seq, sim_angle);

                    if (targets.empty()) {
                        printf("  (无目标)\n");
                    } else {
                        printf("  %-6s %-4s %8s %10s %8s %8s\n",
                               "类型", "Bin", "距离", "速度", "角度", "幅度");
                        for (const auto& t : targets) {
                            printf("  %-6s %-4d %6.1fm %+8.2fm/s %6.1f° %8.0f\n",
                                   t.label().c_str(), t.bin,
                                   t.range_m, t.velocity_mps,
                                   t.angle_deg, t.amplitude);
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