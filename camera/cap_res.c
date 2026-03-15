// 分辨率对比测试：./cap_res <width> <height> <outfile.yuv>
// 用法举例：./cap_res 640 480 /tmp/cap_640.yuv
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DEVICE    "/dev/video11"
#define BUF_COUNT 4

struct buffer { void *start; size_t length; };

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "用法: %s <width> <height> <outfile.yuv>\n", argv[0]);
        return 1;
    }
    int W = atoi(argv[1]);
    int H = atoi(argv[2]);
    const char *outfile = argv[3];

    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    // 等 rkaiq 稳定：丢弃前 20 帧
    int warmup = 20;

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width       = W;
    fmt.fmt.pix_mp.height      = H;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.num_planes  = 1;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("S_FMT"); return 1; }
    // 实际分辨率（驱动可能对齐步长）
    W = fmt.fmt.pix_mp.width;
    H = fmt.fmt.pix_mp.height;
    printf("[%dx%d] 格式设置成功\n", W, H);

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = BUF_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("REQBUFS"); return 1; }

    struct buffer buffers[BUF_COUNT];
    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) { perror("QUERYBUF"); return 1; }
        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start  = mmap(NULL, buf.m.planes[0].length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 fd, buf.m.planes[0].m.mem_offset);
        if (buffers[i].start == MAP_FAILED) { perror("mmap"); return 1; }
    }

    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) { perror("QBUF"); return 1; }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) { perror("STREAMON"); return 1; }

    // 丢弃前 warmup 帧，等 AE/AWB 收敛
    for (int n = 0; n < warmup + 1; n++) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) { perror("DQBUF"); return 1; }

        if (n == warmup) {
            // 最后一帧保存
            FILE *fp = fopen(outfile, "wb");
            if (!fp) { perror("fopen"); return 1; }
            fwrite(buffers[buf.index].start, buf.m.planes[0].bytesused, 1, fp);
            fclose(fp);
            printf("[%dx%d] 已保存 %d 字节到 %s\n", W, H, buf.m.planes[0].bytesused, outfile);
        }

        // 归还 buffer
        struct v4l2_plane planes2[1];
        struct v4l2_buffer qbuf;
        memset(&qbuf, 0, sizeof(qbuf));
        memset(planes2, 0, sizeof(planes2));
        qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        qbuf.memory = V4L2_MEMORY_MMAP;
        qbuf.index  = buf.index;
        qbuf.m.planes = planes2;
        qbuf.length   = 1;
        ioctl(fd, VIDIOC_QBUF, &qbuf);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < BUF_COUNT; i++)
        munmap(buffers[i].start, buffers[i].length);
    close(fd);
    return 0;
}
