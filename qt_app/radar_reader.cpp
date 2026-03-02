#include "radar_reader.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <QDateTime>

RadarReader::RadarReader(QObject *parent)
    : QThread(parent), m_running(false), m_scanAngle(0)
{
}

RadarReader::~RadarReader()
{
    stop();
    wait();
}

void RadarReader::stop()
{
    m_running = false;
}

QVector<TargetPoint> RadarReader::getTargets()
{
    QMutexLocker lock(&m_mutex);
    return m_targets;
}

double RadarReader::getScanAngle()
{
    QMutexLocker lock(&m_mutex);
    return m_scanAngle;
}

void RadarReader::addTargets(const std::vector<RadarTarget>& targets, qint64 now)
{
    for (const auto& t : targets) {
        TargetPoint pt;
        pt.range_m      = t.range_m;
        pt.angle_deg    = t.angle_deg;
        pt.velocity_mps = t.velocity_mps;
        pt.amplitude    = t.amplitude;
        pt.label        = QString::fromStdString(t.label());
        pt.timestamp_ms = now;
        m_targets.append(pt);
    }
}

void RadarReader::pruneOld(qint64 now)
{
    QVector<TargetPoint> fresh;
    fresh.reserve(m_targets.size());
    for (const auto& pt : m_targets) {
        if (now - pt.timestamp_ms < MAX_HISTORY_MS)
            fresh.append(pt);
    }
    m_targets = fresh;
}

void RadarReader::run()
{
    m_running = true;

    int fd = open("/dev/fmcw_radar", O_RDONLY);
    if (fd < 0) {
        qWarning("无法打开 /dev/fmcw_radar");
        return;
    }

    constexpr int FRAME_SIZE  = RadarConfig::FRAME_SIZE;  /* 264 */
    constexpr int BUFFER_SIZE = FRAME_SIZE * 100;

    uint8_t rx_buf[BUFFER_SIZE];
    int data_len = 0;
    RadarProcessor processor;
    double sim_angle = 0.0;

    while (m_running) {
        int space = BUFFER_SIZE - data_len;
        if (space <= 0) { data_len = 0; continue; }

        int bytes = ::read(fd, rx_buf + data_len, space);
        if (bytes <= 0) { usleep(1000); continue; }
        data_len += bytes;

        while (data_len >= FRAME_SIZE && m_running) {
            int pos = -1;
            for (int i = 0; i <= data_len - 4; i++) {
                if (rx_buf[i] == 0xAA && rx_buf[i+1] == 0x30 &&
                    rx_buf[i+2] == 0xFE && rx_buf[i+3] == 0xFF) {
                    pos = i; break;
                }
            }

            if (pos < 0) { data_len = 0; break; }
            if (pos > 0) {
                std::memmove(rx_buf, rx_buf + pos, data_len - pos);
                data_len -= pos;
            }
            if (data_len < FRAME_SIZE) break;

            IQPoint  *raw_iq    = reinterpret_cast<IQPoint*>(&rx_buf[4]);
            uint16_t  frame_seq = *reinterpret_cast<uint16_t*>(&rx_buf[260]);

            auto targets = processor.process(raw_iq, frame_seq, sim_angle);

            if (processor.is_new_frame()) {
                sim_angle += 6.0;
                if (sim_angle >= 360.0) sim_angle -= 360.0;

                qint64 now = QDateTime::currentMSecsSinceEpoch();

                m_mutex.lock();
                m_scanAngle = sim_angle;
                if (!targets.empty()) {
                    addTargets(targets, now);
                }
                pruneOld(now);
                m_mutex.unlock();

                emit newFrame(sim_angle);
            }

            std::memmove(rx_buf, rx_buf + FRAME_SIZE, data_len - FRAME_SIZE);
            data_len -= FRAME_SIZE;
        }
    }

    ::close(fd);
}