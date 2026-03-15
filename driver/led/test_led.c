#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* 和驱动中定义保持一致 */
#define LED_IOC_MAGIC   'L'
#define LED_IOC_ON      _IO(LED_IOC_MAGIC, 0)
#define LED_IOC_OFF     _IO(LED_IOC_MAGIC, 1)
#define LED_IOC_TOGGLE  _IO(LED_IOC_MAGIC, 2)
#define LED_IOC_BLINK   _IOW(LED_IOC_MAGIC, 3, int)

#define DEV_PATH "/dev/lubancat_led"

int main(int argc, char *argv[])
{
    int fd, ret;

    if (argc < 2) {
        printf("用法:\n");
        printf("  %s on          亮灯\n", argv[0]);
        printf("  %s off         灭灯\n", argv[0]);
        printf("  %s toggle      翻转\n", argv[0]);
        printf("  %s blink <ms>  闪烁 (周期毫秒, 0=停止)\n", argv[0]);
        printf("  %s status      读取状态\n", argv[0]);
        return 1;
    }

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open " DEV_PATH);
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        ret = write(fd, "1", 1);
        if (ret < 0) perror("write");
        else printf("LED ON\n");

    } else if (strcmp(argv[1], "off") == 0) {
        ret = write(fd, "0", 1);
        if (ret < 0) perror("write");
        else printf("LED OFF\n");

    } else if (strcmp(argv[1], "toggle") == 0) {
        ret = ioctl(fd, LED_IOC_TOGGLE);
        if (ret < 0) perror("ioctl TOGGLE");
        else printf("LED TOGGLED\n");

    } else if (strcmp(argv[1], "blink") == 0) {
        int period = 500;
        if (argc >= 3)
            period = atoi(argv[2]);
        ret = ioctl(fd, LED_IOC_BLINK, &period);
        if (ret < 0) perror("ioctl BLINK");
        else if (period > 0) printf("LED BLINK: %d ms\n", period);
        else printf("LED BLINK stopped\n");

    } else if (strcmp(argv[1], "status") == 0) {
        char buf[4] = {0};
        ret = read(fd, buf, sizeof(buf) - 1);
        if (ret < 0) perror("read");
        else printf("LED status: %s", buf);

    } else {
        printf("未知命令: %s\n", argv[1]);
    }

    close(fd);
    return 0;
}