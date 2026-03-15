#include "camera_thread.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <QElapsedTimer>

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

    // 4K 模式下写入专属调优参数
    if (WIDTH >= 3840) {
        tuneFor4K();
    }

    return true;
}

// ========== 4K 专属 ISP 调优参数 ==========
// 通过 V4L2 subdev 接口直接写入 IMX415 sensor 寄存器
// 这些参数在调试阶段确定好后固化在这里
//
// 调优策略：4K 无 binning，噪点明显 → 增大 vblank 让 3A 有更多曝光余量
//           → 3A 不需要拉高 gain → 噪点降低
//
// 可调参数一览（/dev/v4l-subdev2）：
//   exposure        4~2242    曝光行数（3A 自动控制，一般不手动设）
//   analogue_gain   0~240     模拟增益（3A 自动控制，值越大噪点越多）
//   vertical_blanking 58~30575  帧间隔行数（不受 3A 控制，可安全手动设）
//   horizontal_flip   0/1
//   vertical_flip     0/1
void CameraThread::tuneFor4K()
{
    int sfd = open(SENSOR_DEV, O_RDWR);
    if (sfd < 0) {
        qDebug() << "tuneFor4K: open sensor subdev failed";
        return;
    }

    // --- 增大 vertical blanking ---
    // 默认 vblank=58（最小值），4K@30fps 时每帧曝光时间上限 ≈ 2242 行
    // 增大 vblank → 每帧总行数增加 → 降低实际帧率 → 但 3A 有更多曝光余量
    // vblank=500 时帧率从 30fps 降到约 25fps，换取更长曝光、更低 gain、更少噪点
    //
    // ★ 调试时修改这个值，找到 帧率 vs 噪点 的平衡点 ★
    struct v4l2_control ctrl;
    ctrl.id    = V4L2_CID_VBLANK;
    ctrl.value = 500;  // 调试参数：可在 58~30575 范围内调整
    if (ioctl(sfd, VIDIOC_S_CTRL, &ctrl) < 0) {
        qDebug() << "tuneFor4K: set vblank failed";
    } else {
        qDebug() << "tuneFor4K: vblank =" << ctrl.value;
    }

    // --- 限制最大模拟增益（可选）---
    // 注意：rkaiq_3A_server 会持续覆写 gain，这里设的值会被立刻覆盖
    // 如果要真正限制 gain，需要修改 IQ 文件中的 AE 策略
    // 下面这行仅作示例，实际效果取决于 3A 服务是否覆盖
    // ctrl.id    = V4L2_CID_ANALOGUE_GAIN;
    // ctrl.value = 20;  // 限制 gain 上限（0~240，越小噪点越少但暗处更暗）
    // ioctl(sfd, VIDIOC_S_CTRL, &ctrl);

    close(sfd);
    qDebug() << "tuneFor4K: 4K tuning applied";
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
// ===== 纯 CPU 版本（整数定点）=====
static inline uint8_t clamp8(int x) { return x < 0 ? 0 : (x > 255 ? 255 : x); }

QImage CameraThread::nv12ToQImage(void *data, int width, int height)
{
    QImage img(width, height, QImage::Format_RGB888);
    const uint8_t *y_plane  = (const uint8_t *)data;
    const uint8_t *uv_plane = y_plane + width * height;

    for (int row = 0; row < height; row++) {
        uint8_t *line = img.scanLine(row);
        const uint8_t *y_row  = y_plane + row * width;
        const uint8_t *uv_row = uv_plane + (row >> 1) * width;
        for (int col = 0; col < width; col++) {
            int y = y_row[col];
            int uv_col = col & ~1;
            int u = uv_row[uv_col]     - 128;
            int v = uv_row[uv_col + 1] - 128;
            int idx = col * 3;
            line[idx]     = clamp8(y + (359 * v >> 8));
            line[idx + 1] = clamp8(y - (88 * u >> 8) - (183 * v >> 8));
            line[idx + 2] = clamp8(y + (454 * u >> 8));
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
    QElapsedTimer emitTimer;
    emitTimer.start();
    qint64 lastEmitMs = 0;
    const qint64 minEmitIntervalMs = 66; // ~15 FPS, reduce UI queue backlog latency

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

        // 限速发送：丢弃多余帧，避免主线程队列堆积造成 0.5s 延迟
        const qint64 nowMs = emitTimer.elapsed();
        if (nowMs - lastEmitMs >= minEmitIntervalMs) {
            // 发送原始 NV12 数据给 RTMP 推流（在 RGB 转换之前，零额外开销）
            emit nv12FrameReady(buffers[buf.index].start, WIDTH * HEIGHT * 3 / 2);

            // NV12 转 QImage，发送给UI线程
            QImage img = nv12ToQImage(buffers[buf.index].start, WIDTH, HEIGHT);
            emit frameReady(img);  // 通过信号传给主线程显示
            lastEmitMs = nowMs;
        }

        // 处理完，把buffer还回去继续采集
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "QBUF failed";
            break;
        }
        // DQBUF 本身是阻塞的，不需要额外 sleep
    }

    closeDevice();
}