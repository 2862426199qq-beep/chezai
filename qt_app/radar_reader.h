#ifndef RADAR_READER_H
#define RADAR_READER_H

#include <QThread>
#include <QMutex>
#include <QVector>
#include "radar_processor.h"

/* 供 UI 使用的目标点结构 */
struct TargetPoint {
    double range_m;
    double angle_deg;
    double velocity_mps;
    double amplitude;
    QString label;
    qint64 timestamp_ms;   /* 检测时间，用于淡出效果 */
};

/*
 * 后台线程：读取 /dev/fmcw_radar → 解帧 → 处理 → 发信号给 UI
 */
class RadarReader : public QThread
{
    Q_OBJECT

public:
    explicit RadarReader(QObject *parent = nullptr);
    ~RadarReader();

    void stop();

    /* UI 调用：获取当前所有目标点（线程安全） */
    QVector<TargetPoint> getTargets();
    double getScanAngle();

signals:
    void newFrame(double scanAngleDeg);

protected:
    void run() override;

private:
    volatile bool m_running;
    QMutex m_mutex;
    QVector<TargetPoint> m_targets;
    double m_scanAngle;

    /* 目标历史（保留最近 5 秒的点用于拖尾效果） */
    static constexpr int MAX_HISTORY_MS = 5000;
    void addTargets(const std::vector<RadarTarget>& targets, qint64 now);
    void pruneOld(qint64 now);
};

#endif
