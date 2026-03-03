#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gpiod.h>
#include <time.h>
#include <sched.h>

#define GPIO_CHIP "gpiochip3"
#define GPIO_OFFSET 6

static inline long micros(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000;
}

static int wait_for(struct gpiod_line *line, int val, int timeout_us) {
    long deadline = micros() + timeout_us;
    while (gpiod_line_get_value(line) != val) {
        if (micros() > deadline) return -1;
    }
    return 0;
}

int main(void) {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int data[5];
    int i;

    /* 提升为实时优先级，减少被调度打断 */
    struct sched_param sp;
    sp.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0) {
        perror("sched_setscheduler (run as root!)");
    }

    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) { perror("gpiod_chip_open"); return 1; }

    line = gpiod_chip_get_line(chip, GPIO_OFFSET);
    if (!line) { perror("gpiod_chip_get_line"); return 1; }

    printf("DHT11 test v3 (RT priority) on %s line %d\n\n", GPIO_CHIP, GPIO_OFFSET);

    int success = 0;
    for (int attempt = 0; attempt < 20; attempt++) {
        memset(data, 0, sizeof(data));

        /* 1. Start: 输出低 20ms */
        gpiod_line_request_output(line, "dht11", 1);
        gpiod_line_set_value(line, 0);
        usleep(20000);
        gpiod_line_set_value(line, 1);
        usleep(40);
        gpiod_line_release(line);

        /* 2. 切回输入 */
        gpiod_line_request_input(line, "dht11");

        /* 等待 DHT11 响应 */
        if (wait_for(line, 0, 5000)) goto retry;
        if (wait_for(line, 1, 5000)) goto retry;
        if (wait_for(line, 0, 5000)) goto retry;

        /* 3. 读 40 bit — 加大超时到 2000us */
        int ok = 1;
        for (i = 0; i < 40; i++) {
            if (wait_for(line, 1, 2000)) { ok = 0; break; }
            long t = micros();
            if (wait_for(line, 0, 2000)) { ok = 0; break; }
            long dur = micros() - t;
            data[i/8] <<= 1;
            if (dur > 40) data[i/8] |= 1;
        }

        if (!ok) {
            printf("Attempt %d: bit %d timeout\n", attempt+1, i);
            goto retry;
        }

        /* 4. 校验 */
        int cksum = (data[0]+data[1]+data[2]+data[3]) & 0xFF;
        if (cksum == data[4]) {
            printf("===================================\n");
            printf("[OK] Attempt %d:\n", attempt+1);
            printf("  Humidity : %d.%d %%\n", data[0], data[1]);
            printf("  Temp     : %d.%d C\n", data[2], data[3]);
            printf("  Raw      : %02X %02X %02X %02X %02X\n",
                   data[0], data[1], data[2], data[3], data[4]);
            printf("===================================\n");
            success++;
            if (success >= 3) break;  /* 连续成功3次就够了 */
        } else {
            printf("Attempt %d: checksum err (%d.%d / %d.%d) ck=%d vs %d\n",
                   attempt+1, data[0], data[1], data[2], data[3], cksum, data[4]);
        }

retry:
        gpiod_line_release(line);
        sleep(2);
    }

    gpiod_chip_close(chip);
    return success > 0 ? 0 : 1;
}
