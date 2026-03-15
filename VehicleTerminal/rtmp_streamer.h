#ifndef RTMP_STREAMER_H
#define RTMP_STREAMER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

// MPP 编码器
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/rk_venc_cfg.h>
#include <rockchip/rk_venc_cmd.h>

// ffmpeg RTMP 推流
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

class RtmpStreamer : public QThread
{
    Q_OBJECT

public:
    explicit RtmpStreamer(int width, int height, int fps = 25,
                          const QString &rtmpUrl = QString(),
                          QObject *parent = nullptr);
    ~RtmpStreamer();

    void stop();
    bool isStreaming() const { return m_streaming; }

public slots:
    // CameraThread 每帧都调用，传入 NV12 原始数据指针和大小
    void pushNv12Frame(void *nv12Data, int dataSize);

signals:
    void streamStarted();
    void streamError(const QString &msg);

protected:
    void run() override;

private:
    bool initMppEncoder();
    void closeMppEncoder();
    bool initRtmpOutput();
    void closeRtmpOutput();
    bool encodeAndSend(void *nv12Data, int dataSize);

    // 视频参数
    int m_width;
    int m_height;
    int m_fps;
    QString m_rtmpUrl;

    // MPP 编码器
    MppCtx m_mppCtx;
    MppApi *m_mppApi;
    MppEncCfg m_encCfg;
    MppBufferGroup m_bufGroup;
    MppBuffer m_frameBuffer;
    MppFrame m_mppFrame;
    int m_frameSize;       // NV12 帧大小 = w * h * 3 / 2
    int m_headerSize;      // SPS/PPS 头大小
    uint8_t *m_headerData; // SPS/PPS 数据

    // ffmpeg RTMP 输出
    AVFormatContext *m_fmtCtx;
    AVStream *m_videoStream;
    int64_t m_pts;

    // 线程控制
    volatile bool m_running;
    volatile bool m_streaming;
    QMutex m_mutex;

    // 帧缓冲（线程间传递）
    uint8_t *m_pendingFrame;
    int m_pendingSize;
    bool m_hasNewFrame;
    QMutex m_frameMutex;
    QWaitCondition m_frameCond;
};

#endif
