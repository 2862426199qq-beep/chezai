#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#define GPIO_NUM 102

static int gpio_export(int gpio) {
    char buf[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return -1;
    int len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    usleep(100000);
    return 0;
}

static int gpio_unexport(int gpio) {
    char buf[64];
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) return -1;
    int len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

static int gpio_set_dir(int gpio, const char *dir) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    write(fd, dir, strlen(dir));
    close(fd);
    return 0;
}

static int gpio_set_value(int gpio, int val) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    char c = val ? '1' : '0';
    write(fd, &c, 1);
    close(fd);
    return 0;
}

static int gpio_get_value(int gpio) {
    char path[128], c;
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    read(fd, &c, 1);
    close(fd);
    return (c == '1') ? 1 : 0;
}

static long micros(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int main(void) {
    int data[5] = {0};
    int i, j;

    gpio_export(GPIO_NUM);

    printf("DHT11 test on GPIO %d (Pin 29)\n", GPIO_NUM);
    printf("Reading... (may need a few tries)\n\n");

    for (int attempt = 0; attempt < 10; attempt++) {
        memset(data, 0, sizeof(data));

        /* 1. 发送 Start 信号: 拉低 20ms */
        gpio_set_dir(GPIO_NUM, "out");
        gpio_set_value(GPIO_NUM, 0);
        usleep(20000);  /* 20ms */
        gpio_set_value(GPIO_NUM, 1);
        usleep(30);

        /* 2. 切换为输入，等待 DHT11 响应 */
        gpio_set_dir(GPIO_NUM, "in");

        /* 等待 DHT11 拉低 (响应信号) */
        long timeout = micros() + 1000;
        while (gpio_get_value(GPIO_NUM) == 1) {
            if (micros() > timeout) goto retry;
        }

        /* 等待拉低结束 (80us) */
        timeout = micros() + 1000;
        while (gpio_get_value(GPIO_NUM) == 0) {
            if (micros() > timeout) goto retry;
        }

        /* 等待拉高结束 (80us) */
        timeout = micros() + 1000;
        while (gpio_get_value(GPIO_NUM) == 1) {
            if (micros() > timeout) goto retry;
        }

        /* 3. 读取 40 bit 数据 */
        for (i = 0; i < 40; i++) {
            /* 等待低电平结束 (50us) */
            timeout = micros() + 1000;
            while (gpio_get_value(GPIO_NUM) == 0) {
                if (micros() > timeout) goto retry;
            }

            /* 测量高电平持续时间 */
            long t_start = micros();
            timeout = t_start + 1000;
            while (gpio_get_value(GPIO_NUM) == 1) {
                if (micros() > timeout) goto retry;
            }
            long duration = micros() - t_start;

            /* >40us 为 bit 1, <40us 为 bit 0 */
            data[i / 8] <<= 1;
            if (duration > 40)
                data[i / 8] |= 1;
        }

        /* 4. 校验 */
        int checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (checksum == data[4]) {
            printf("[OK] Attempt %d: Humidity=%d.%d%%  Temp=%d.%d C\n",
                   attempt + 1, data[0], data[1], data[2], data[3]);
            gpio_unexport(GPIO_NUM);
            return 0;
        } else {
            printf("[CHECKSUM ERR] Attempt %d: %d.%d / %d.%d / check=%d vs %d\n",
                   attempt + 1, data[0], data[1], data[2], data[3], checksum, data[4]);
        }

retry:
        printf("Attempt %d: retrying...\n", attempt + 1);
        /* DHT11 至少等 2 秒再读 */
        sleep(2);
    }

    printf("Failed after 10 attempts\n");
    gpio_unexport(GPIO_NUM);
    return 1;
}
