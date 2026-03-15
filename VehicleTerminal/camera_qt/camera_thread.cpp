#include "camera_thread.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

CameraThread::CameraThread(QObject *parent)
    : QThread(parent), fd(-1), running(false)
#ifdef USE_RGA
      , rgbBuffer(nullptr)
#endif
{
}

void CameraThread::stop()
{
    running = false;  // 设置标志，run()循环会退出
}

bool CameraThread::initDevice()
{
    // 1. 打开设备
    fd = open(DEVICE, O_RDWR);
    if (fd < 0) { qDebug() << "open failed"; return false; }

    // 2. 设置格式（和之前C程序一样）
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width       = WIDTH;
    fmt.fmt.pix_mp.height      = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.num_planes  = 1;
    //调用驱动层imx415_set_fmt() (imx415.c 第2111行) 回调设置格式
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        qDebug() << "S_FMT failed"; return false;
    }

    // 3. 申请buffer
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = BUF_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        qDebug() << "REQBUFS failed"; return false;
    }

    // 4. mmap 映射
    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = i;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qDebug() << "QUERYBUF failed"; return false;
        }
        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start  = mmap(NULL, buf.m.planes[0].length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 fd, buf.m.planes[0].m.mem_offset);
        if (buffers[i].start == MAP_FAILED) {
            qDebug() << "mmap failed"; return false;
        }
    }

    // 5. 所有buffer入队
    for (int i = 0; i < BUF_COUNT; i++) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.index    = i;
        buf.m.planes = planes;
        buf.length   = 1;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "QBUF failed"; return false;
        }
    }

    // 6. 开始采集
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        qDebug() << "STREAMON failed"; return false;
    }

    qDebug() << "Camera init success";
    return true;
}

void CameraThread::closeDevice()
{
    // 停止采集
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    // 解除mmap映射
    for (int i = 0; i < BUF_COUNT; i++)
        munmap(buffers[i].start, buffers[i].length);

    close(fd);
    fd = -1;

#ifdef USE_RGA
    // 释放 RGA 输出 buffer
    delete[] rgbBuffer;
    rgbBuffer = nullptr;
#endif
}

#ifdef USE_RGA
// ===== RGA 硬件加速版本 =====
QImage CameraThread::nv12ToQImage(void *data, int width, int height)
{
    if (!rgbBuffer) {
        rgbBuffer = new uint8_t[width * height * 3];
    }

    rga_buffer_t src = wrapbuffer_virtualaddr(data, width, height,
                                               RK_FORMAT_YCbCr_420_SP);
    rga_buffer_t dst = wrapbuffer_virtualaddr(rgbBuffer, width, height,
                                               RK_FORMAT_RGB_888);

    IM_STATUS status = imcvtcolor(src, dst,
                                   RK_FORMAT_YCbCr_420_SP,
                                   RK_FORMAT_RGB_888);
    if (status != IM_STATUS_SUCCESS) {
        qDebug() << "RGA imcvtcolor failed:" << imStrError(status);
        return QImage();
    }

    return QImage(rgbBuffer, width, height, width * 3,
                  QImage::Format_RGB888).copy();
}
#else
// ===== 纯 CPU 版本（scanLine + 浮点运算）=====
QImage CameraThread::nv12ToQImage(void *data, int width, int height)
{
    QImage img(width, height, QImage::Format_RGB888);
    uint8_t *nv12     = (uint8_t *)data;
    uint8_t *y_plane  = nv12;
    uint8_t *uv_plane = nv12 + width * height;

    for (int row = 0; row < height; row++) {
        uint8_t *line = img.scanLine(row);
        for (int col = 0; col < width; col++) {
            int y = y_plane[row * width + col];
            int uv_index = (row / 2) * width + (col & ~1);
            int u = uv_plane[uv_index]     - 128;
            int v = uv_plane[uv_index + 1] - 128;

            line[col * 3]     = qBound(0, (int)(y + 1.402f * v), 255);
            line[col * 3 + 1] = qBound(0, (int)(y - 0.344f * u - 0.714f * v), 255);
            line[col * 3 + 2] = qBound(0, (int)(y + 1.772f * u), 255);
        }
    }
    return img;
}
#endif
void CameraThread::run()
{
    // 初始化失败直接退出
    if (!initDevice()) return;

    running = true;

    // 循环采帧
    while (running) {
        struct v4l2_plane planes[1];
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory   = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length   = 1;

        // 取一帧（阻塞等待硬件填满）
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            qDebug() << "DQBUF failed";
            break;
        }

        // NV12 转 QImage，发送给UI线程
        // 注意：这里在子线程里做转换，不阻塞UI
        QImage img = nv12ToQImage(buffers[buf.index].start, WIDTH, HEIGHT);
        emit frameReady(img);  // 通过信号传给主线程显示

        // 处理完，把buffer还回去继续采集
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "QBUF failed";
            break;
        }
        // DQBUF 本身是阻塞的，硬件帧率自然限速，无需 msleep
    }

    closeDevice();
}