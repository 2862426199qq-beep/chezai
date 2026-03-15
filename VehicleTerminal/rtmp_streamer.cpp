#include "rtmp_streamer.h"
#include <QDebug>
#include <string.h>

RtmpStreamer::RtmpStreamer(int width, int height, int fps,
                           const QString &rtmpUrl, QObject *parent)
    : QThread(parent)
    , m_width(width)
    , m_height(height)
    , m_fps(fps)
    , m_rtmpUrl(rtmpUrl)
    , m_mppCtx(nullptr)
    , m_mppApi(nullptr)
    , m_encCfg(nullptr)
    , m_bufGroup(nullptr)
    , m_frameBuffer(nullptr)
    , m_mppFrame(nullptr)
    , m_frameSize(width * height * 3 / 2)
    , m_headerSize(0)
    , m_headerData(nullptr)
    , m_fmtCtx(nullptr)
    , m_videoStream(nullptr)
    , m_pts(0)
    , m_running(false)
    , m_streaming(false)
    , m_pendingFrame(nullptr)
    , m_pendingSize(0)
    , m_hasNewFrame(false)
{
    m_pendingFrame = new uint8_t[m_frameSize];
}

RtmpStreamer::~RtmpStreamer()
{
    stop();
    wait();
    delete[] m_pendingFrame;
    delete[] m_headerData;
}

void RtmpStreamer::stop()
{
    m_running = false;
    m_frameCond.wakeAll();
}

// ======================== MPP 硬件编码器 ========================

bool RtmpStreamer::initMppEncoder()
{
    MPP_RET ret;

    // 1. 创建 MPP 上下文 + API 句柄
    ret = mpp_create(&m_mppCtx, &m_mppApi);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: mpp_create failed" << ret;
        return false;
    }

    // 2. 初始化为 H.264 编码器
    ret = mpp_init(m_mppCtx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: mpp_init encoder failed" << ret;
        return false;
    }

    // 3. 配置编码参数
    ret = mpp_enc_cfg_init(&m_encCfg);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: mpp_enc_cfg_init failed" << ret;
        return false;
    }

    // 基础参数
    mpp_enc_cfg_set_s32(m_encCfg, "prep:width",      m_width);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:height",     m_height);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:hor_stride",  m_width);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:ver_stride",  m_height);
    mpp_enc_cfg_set_s32(m_encCfg, "prep:format",      MPP_FMT_YUV420SP); // NV12

    // 码率控制：CBR 2Mbps
    mpp_enc_cfg_set_s32(m_encCfg, "rc:mode",          1); // MPP_ENC_RC_MODE_CBR
    mpp_enc_cfg_set_s32(m_encCfg, "rc:bps_target",    2000000);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:bps_max",       2500000);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:bps_min",       1500000);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_flex",    0);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_num",     m_fps);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_in_denorm",  1);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_out_flex",   0);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_out_num",    m_fps);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:fps_out_denorm", 1);
    mpp_enc_cfg_set_s32(m_encCfg, "rc:gop",            m_fps * 2); // 2秒一个 GOP

    // H.264 编码参数
    mpp_enc_cfg_set_s32(m_encCfg, "codec:id",         MPP_VIDEO_CodingAVC);
    mpp_enc_cfg_set_s32(m_encCfg, "h264:profile",     100);  // High Profile
    mpp_enc_cfg_set_s32(m_encCfg, "h264:level",        41);  // Level 4.1
    mpp_enc_cfg_set_s32(m_encCfg, "h264:cabac_en",     1);
    mpp_enc_cfg_set_s32(m_encCfg, "h264:cabac_idc",    0);

    ret = m_mppApi->control(m_mppCtx, MPP_ENC_SET_CFG, m_encCfg);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: MPP_ENC_SET_CFG failed" << ret;
        return false;
    }

    // 4. 设置 SPS/PPS 每个 IDR 帧都带（RTMP 需要）
    MppEncHeaderMode headerMode = MPP_ENC_HEADER_MODE_EACH_IDR;
    m_mppApi->control(m_mppCtx, MPP_ENC_SET_HEADER_MODE, &headerMode);

    // 5. 获取 SPS/PPS 头数据
    MppPacket hdrPkt = nullptr;
    ret = m_mppApi->control(m_mppCtx, MPP_ENC_GET_HDR_SYNC, &hdrPkt);
    if (ret == MPP_OK && hdrPkt) {
        m_headerSize = mpp_packet_get_length(hdrPkt);
        m_headerData = new uint8_t[m_headerSize];
        memcpy(m_headerData, mpp_packet_get_pos(hdrPkt), m_headerSize);
        mpp_packet_deinit(&hdrPkt);
        qDebug() << "RtmpStreamer: SPS/PPS header size =" << m_headerSize;
    }

    // 6. 分配编码器输入 buffer
    ret = mpp_buffer_group_get_internal(&m_bufGroup, MPP_BUFFER_TYPE_NORMAL);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: buffer group create failed" << ret;
        return false;
    }

    ret = mpp_buffer_get(m_bufGroup, &m_frameBuffer, m_frameSize);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: mpp_buffer_get failed" << ret;
        return false;
    }

    // 7. 初始化 MppFrame
    mpp_frame_init(&m_mppFrame);
    mpp_frame_set_width(m_mppFrame, m_width);
    mpp_frame_set_height(m_mppFrame, m_height);
    mpp_frame_set_hor_stride(m_mppFrame, m_width);
    mpp_frame_set_ver_stride(m_mppFrame, m_height);
    mpp_frame_set_fmt(m_mppFrame, MPP_FMT_YUV420SP);
    mpp_frame_set_buffer(m_mppFrame, m_frameBuffer);

    qDebug() << "RtmpStreamer: MPP H.264 encoder initialized"
             << m_width << "x" << m_height << "@" << m_fps << "fps";
    return true;
}

void RtmpStreamer::closeMppEncoder()
{
    if (m_mppFrame) {
        mpp_frame_deinit(&m_mppFrame);
        m_mppFrame = nullptr;
    }
    if (m_frameBuffer) {
        mpp_buffer_put(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
    if (m_bufGroup) {
        mpp_buffer_group_put(m_bufGroup);
        m_bufGroup = nullptr;
    }
    if (m_encCfg) {
        mpp_enc_cfg_deinit(m_encCfg);
        m_encCfg = nullptr;
    }
    if (m_mppCtx) {
        mpp_destroy(m_mppCtx);
        m_mppCtx = nullptr;
        m_mppApi = nullptr;
    }
}

// ======================== RTMP 推流（ffmpeg libavformat）========================

bool RtmpStreamer::initRtmpOutput()
{
    if (m_rtmpUrl.isEmpty()) {
        qWarning() << "RtmpStreamer: RTMP URL is empty";
        return false;
    }

    // 1. 创建 FLV 输出上下文（RTMP 用 FLV 封装）
    int ret = avformat_alloc_output_context2(&m_fmtCtx, nullptr, "flv",
                                              m_rtmpUrl.toUtf8().constData());
    if (ret < 0 || !m_fmtCtx) {
        qWarning() << "RtmpStreamer: avformat_alloc_output_context2 failed";
        return false;
    }

    // 2. 添加 H.264 视频流
    m_videoStream = avformat_new_stream(m_fmtCtx, nullptr);
    if (!m_videoStream) {
        qWarning() << "RtmpStreamer: avformat_new_stream failed";
        return false;
    }

    AVCodecParameters *codecpar = m_videoStream->codecpar;
    codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codecpar->codec_id   = AV_CODEC_ID_H264;
    codecpar->width      = m_width;
    codecpar->height     = m_height;
    codecpar->format     = AV_PIX_FMT_NV12;
    m_videoStream->time_base = {1, 1000};  // 毫秒时基（FLV 标准）

    // 3. 设置 extradata（SPS/PPS）—— RTMP 播放器解码必需
    if (m_headerData && m_headerSize > 0) {
        codecpar->extradata = (uint8_t *)av_mallocz(m_headerSize + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(codecpar->extradata, m_headerData, m_headerSize);
        codecpar->extradata_size = m_headerSize;
    }

    // 4. 打开 RTMP 连接
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_fmtCtx->pb, m_rtmpUrl.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "RtmpStreamer: avio_open failed:" << errbuf;
            return false;
        }
    }

    // 5. 写 FLV 文件头
    ret = avformat_write_header(m_fmtCtx, nullptr);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "RtmpStreamer: avformat_write_header failed:" << errbuf;
        return false;
    }

    qDebug() << "RtmpStreamer: RTMP connected to" << m_rtmpUrl;
    return true;
}

void RtmpStreamer::closeRtmpOutput()
{
    if (m_fmtCtx) {
        if (m_fmtCtx->pb) {
            av_write_trailer(m_fmtCtx);
            avio_closep(&m_fmtCtx->pb);
        }
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
        m_videoStream = nullptr;
    }
}

// ======================== 编码 + 推流核心 ========================

bool RtmpStreamer::encodeAndSend(void *nv12Data, int dataSize)
{
    if (!m_mppCtx || !m_mppApi || !m_fmtCtx) return false;

    // 1. 将 NV12 数据拷入 MPP buffer
    void *bufPtr = mpp_buffer_get_ptr(m_frameBuffer);
    memcpy(bufPtr, nv12Data, dataSize);

    // 2. MPP 编码：送入一帧，取出 H.264 packet
    MPP_RET ret = m_mppApi->encode_put_frame(m_mppCtx, m_mppFrame);
    if (ret != MPP_OK) {
        qWarning() << "RtmpStreamer: encode_put_frame failed" << ret;
        return false;
    }

    MppPacket packet = nullptr;
    ret = m_mppApi->encode_get_packet(m_mppCtx, &packet);
    if (ret != MPP_OK || !packet) {
        return false;  // 编码器还未输出（正常，pipeline 延迟）
    }

    // 3. 获取编码后的 H.264 数据
    void *pktData = mpp_packet_get_pos(packet);
    size_t pktLen = mpp_packet_get_length(packet);

    if (pktLen > 0 && m_fmtCtx) {
        // 4. 封装成 AVPacket 推送到 RTMP
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = (uint8_t *)pktData;
        avpkt.size = pktLen;
        avpkt.stream_index = m_videoStream->index;

        // 时间戳：毫秒
        avpkt.pts = m_pts * 1000 / m_fps;
        avpkt.dts = avpkt.pts;
        avpkt.duration = 1000 / m_fps;
        m_pts++;

        // 关键帧标记
        RK_U32 isIntra = 0;
        MppMeta meta = mpp_packet_get_meta(packet);
        if (meta)
            mpp_meta_get_s32(meta, KEY_OUTPUT_INTRA, (RK_S32 *)&isIntra);
        if (isIntra)
            avpkt.flags |= AV_PKT_FLAG_KEY;

        int writeRet = av_interleaved_write_frame(m_fmtCtx, &avpkt);
        if (writeRet < 0) {
            char errbuf[128];
            av_strerror(writeRet, errbuf, sizeof(errbuf));
            qWarning() << "RtmpStreamer: write_frame failed:" << errbuf;
            mpp_packet_deinit(&packet);
            return false;
        }
    }

    mpp_packet_deinit(&packet);
    return true;
}

// ======================== 外部接口：接收 NV12 帧 ========================

void RtmpStreamer::pushNv12Frame(void *nv12Data, int dataSize)
{
    if (!m_running || !m_streaming) return;
    if (dataSize > m_frameSize) return;

    QMutexLocker lock(&m_frameMutex);
    memcpy(m_pendingFrame, nv12Data, dataSize);
    m_pendingSize = dataSize;
    m_hasNewFrame = true;
    m_frameCond.wakeOne();
}

// ======================== 线程主循环 ========================

void RtmpStreamer::run()
{
    m_running = true;
    m_streaming = false;

    // 初始化 MPP 编码器
    if (!initMppEncoder()) {
        emit streamError("MPP encoder init failed");
        closeMppEncoder();
        return;
    }

    // 初始化 RTMP 输出
    if (!initRtmpOutput()) {
        emit streamError("RTMP connection failed: " + m_rtmpUrl);
        closeRtmpOutput();
        closeMppEncoder();
        return;
    }

    m_streaming = true;
    emit streamStarted();
    qDebug() << "RtmpStreamer: streaming started";

    // 编码推流循环
    while (m_running) {
        QMutexLocker lock(&m_frameMutex);
        while (!m_hasNewFrame && m_running) {
            m_frameCond.wait(&m_frameMutex, 100);
        }
        if (!m_running) break;

        m_hasNewFrame = false;
        int size = m_pendingSize;

        // 复制数据出来再解锁，减少锁持有时间
        // 直接用 m_pendingFrame 编码（锁内完成拷贝到 MPP buffer）
        void *bufPtr = mpp_buffer_get_ptr(m_frameBuffer);
        memcpy(bufPtr, m_pendingFrame, size);
        lock.unlock();

        // 编码 + 推流（锁外执行，不阻塞采集线程）
        MPP_RET ret = m_mppApi->encode_put_frame(m_mppCtx, m_mppFrame);
        if (ret != MPP_OK) continue;

        MppPacket packet = nullptr;
        ret = m_mppApi->encode_get_packet(m_mppCtx, &packet);
        if (ret != MPP_OK || !packet) continue;

        void *pktData = mpp_packet_get_pos(packet);
        size_t pktLen = mpp_packet_get_length(packet);

        if (pktLen > 0) {
            AVPacket avpkt;
            av_init_packet(&avpkt);
            avpkt.data = (uint8_t *)pktData;
            avpkt.size = pktLen;
            avpkt.stream_index = m_videoStream->index;
            avpkt.pts = m_pts * 1000 / m_fps;
            avpkt.dts = avpkt.pts;
            avpkt.duration = 1000 / m_fps;
            m_pts++;

            RK_U32 isIntra = 0;
            MppMeta meta = mpp_packet_get_meta(packet);
            if (meta)
                mpp_meta_get_s32(meta, KEY_OUTPUT_INTRA, (RK_S32 *)&isIntra);
            if (isIntra)
                avpkt.flags |= AV_PKT_FLAG_KEY;

            if (av_interleaved_write_frame(m_fmtCtx, &avpkt) < 0) {
                qWarning() << "RtmpStreamer: RTMP write failed, stopping";
                break;
            }
        }

        mpp_packet_deinit(&packet);
    }

    m_streaming = false;
    closeRtmpOutput();
    closeMppEncoder();
    qDebug() << "RtmpStreamer: stopped";
}
