#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gpiod.h>
#include <time.h>

/* GPIO3_A6 = chip 3, offset 6 */
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

    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) { perror("gpiod_chip_open"); return 1; }

    line = gpiod_chip_get_line(chip, GPIO_OFFSET);
    if (!line) { perror("gpiod_chip_get_line"); return 1; }

    printf("DHT11 test via libgpiod (%s line %d)\n\n", GPIO_CHIP, GPIO_OFFSET);

    for (int attempt = 0; attempt < 10; attempt++) {
        memset(data, 0, sizeof(data));

        /* 1. Start: 输出低 20ms */
        gpiod_line_request_output(line, "dht11", 1);
        gpiod_line_set_value(line, 0);
        usleep(20000);
        gpiod_line_set_value(line, 1);
        usleep(30);
        gpiod_line_release(line);

        /* 2. 切换输入 */
        gpiod_line_request_input(line, "dht11");

        /* 等待 DHT11 拉低 */
        if (wait_for(line, 0, 2000)) { printf("Attempt %d: no response (wait low)\n", attempt+1); goto retry; }
        /* 等待拉低结束 80us */
        if (wait_for(line, 1, 2000)) { printf("Attempt %d: no response (wait high)\n", attempt+1); goto retry; }
        /* 等待拉高结束 80us */
        if (wait_for(line, 0, 2000)) { printf("Attempt %d: no response (wait data)\n", attempt+1); goto retry; }

        /* 3. 读 40 bit */
        for (i = 0; i < 40; i++) {
            if (wait_for(line, 1, 1000)) { printf("Attempt %d: bit %d low timeout\n", attempt+1, i); goto retry; }
            long t = micros();
            if (wait_for(line, 0, 1000)) { printf("Attempt %d: bit %d high timeout\n", attempt+1, i); goto retry; }
            long dur = micros() - t;
            data[i/8] <<= 1;
            if (dur > 40) data[i/8] |= 1;
        }

        /* 4. 校验 */
        int cksum = (data[0]+data[1]+data[2]+data[3]) & 0xFF;
        if (cksum == data[4]) {
            printf("[OK] Attempt %d: Humidity=%d.%d%%  Temp=%d.%d C\n",
                   attempt+1, data[0], data[1], data[2], data[3]);
            gpiod_line_release(line);
            gpiod_chip_close(chip);
            return 0;
        } else {
            printf("[CHECKSUM ERR] %d.%d / %d.%d / ck=%d vs %d\n",
                   data[0], data[1], data[2], data[3], cksum, data[4]);
        }

retry:
        gpiod_line_release(line);
        sleep(2);
    }

    printf("Failed after 10 attempts\n");
    gpiod_chip_close(chip);
    return 1;
}
