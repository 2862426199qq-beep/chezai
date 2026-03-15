#ifndef CAMERA_THREAD_H
#define CAMERA_THREAD_H

#include <QThread>
#include <QImage>
#include <linux/videodev2.h>

// ========== RGA 开关 ==========
// 注释掉下面这行则回退到纯 CPU 整数定点 NV12→RGB 转换
// #define USE_RGA
// ==============================

#ifdef USE_RGA
#include "im2d.hpp"
#include "rga.h"
#endif

#define DEVICE      "/dev/video11"
#define SENSOR_DEV  "/dev/v4l-subdev2"   // IMX415 sensor subdev
#define WIDTH       1280
#define HEIGHT      720
// #define WIDTH       3840
// #define HEIGHT      2160
#define BUF_COUNT   4

// 存放 mmap 映射后的 buffer
struct CamBuffer {
    void   *start;
    size_t  length;
};

class CameraThread : public QThread
{
    Q_OBJECT

public:
    explicit CameraThread(QObject *parent = nullptr);
    void stop();  // 外部调用停止采集

signals:
    // 每采到一帧就发这个信号，把图像传给UI线程显示
    void frameReady(QImage img);
    // 原始 NV12 数据信号（用于 RTMP 推流，避免 RGB→NV12 回转）
    void nv12FrameReady(void *data, int size);

protected:
    void run() override;  // 线程主函数

private:
    bool initDevice();    // 初始化V4L2设备
    void closeDevice();   // 关闭设备
    void tuneFor4K();     // 4K 专属 ISP 调优参数
    QImage nv12ToQImage(void *data, int width, int height); // NV12转QImage

    int fd;                        // 设备文件描述符
    CamBuffer buffers[BUF_COUNT];  // mmap buffer数组
    volatile bool running;         // 控制线程运行/停止的标志
#ifdef USE_RGA
    uint8_t *rgbBuffer;            // RGA 输出 RGB buffer（复用，不每帧分配）
#endif
};

#endif