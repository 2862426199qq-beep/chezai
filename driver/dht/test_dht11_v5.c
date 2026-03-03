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

    struct sched_param sp;
    sp.sched_priority = 99;
    sched_setscheduler(0, SCHED_FIFO, &sp);

    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) { perror("gpiod_chip_open"); return 1; }
    line = gpiod_chip_get_line(chip, GPIO_OFFSET);
    if (!line) { perror("gpiod_chip_get_line"); return 1; }

    printf("DHT11 test v5 on %s line %d\n\n", GPIO_CHIP, GPIO_OFFSET);

    int success = 0;
    for (int attempt = 0; attempt < 15; attempt++) {
        memset(data, 0, sizeof(data));

        gpiod_line_request_output(line, "dht11", 1);
        gpiod_line_set_value(line, 0);
        usleep(20000);
        gpiod_line_set_value(line, 1);
        usleep(40);
        gpiod_line_release(line);

        gpiod_line_request_input(line, "dht11");

        if (wait_for(line, 0, 5000)) goto retry;
        if (wait_for(line, 1, 5000)) goto retry;
        if (wait_for(line, 0, 5000)) goto retry;

        int ok = 1;
        for (i = 0; i < 40; i++) {
            if (wait_for(line, 1, 2000)) { ok = 0; break; }

            long t = micros();

            if (i < 39) {
                if (wait_for(line, 0, 2000)) { ok = 0; break; }
                long dur = micros() - t;
                data[i/8] <<= 1;
                if (dur > 40) data[i/8] |= 1;
            } else {
                /* 最后一个 bit: 延时 35us 后直接采样
                   如果还是高电平 → bit1 (高持续>40us)
                   如果已经低电平 → bit0 (高持续<28us) */
                usleep(35);
                int val = gpiod_line_get_value(line);
                data[i/8] <<= 1;
                if (val == 1) data[i/8] |= 1;
            }
        }

        if (!ok) {
            printf("Attempt %d: bit %d timeout\n", attempt+1, i);
            goto retry;
        }

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
            if (success >= 3) break;
        } else {
            printf("Attempt %d: checksum err (%d.%d%% / %d.%dC) ck=%d vs %d raw=[%02X %02X %02X %02X %02X]\n",
                   attempt+1, data[0], data[1], data[2], data[3], cksum, data[4],
                   data[0], data[1], data[2], data[3], data[4]);
        }

retry:
        gpiod_line_release(line);
        sleep(2);
    }

    if (success > 0)
        printf("\nDHT11 is working!\n");
    else
        printf("\nFailed.\n");

    gpiod_chip_close(chip);
    return success > 0 ? 0 : 1;
}
