#ifndef RADAR_WIDGET_H
#define RADAR_WIDGET_H

#include <QWidget>
#include <QTimer>
#include "radar_reader.h"

class RadarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RadarWidget(QWidget *parent = nullptr);
    ~RadarWidget();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onNewFrame(double angle);
    void onRefresh();

private:
    RadarReader *m_reader;
    QTimer      *m_timer;

    double m_scanAngle;
    QVector<TargetPoint> m_targets;

    /* 绘制参数 */
    double m_maxRange;       /* 最大显示距离 */
    int    m_numRings;       /* 距离圈数量 */

    /* 坐标转换：雷达极坐标 → 屏幕像素 */
    QPointF polarToScreen(double range_m, double angle_deg,
                          QPointF center, double scale) const;

    void drawBackground(QPainter &p, QPointF center, double scale);
    void drawScanLine(QPainter &p, QPointF center, double scale);
    void drawTargets(QPainter &p, QPointF center, double scale);
    void drawInfoPanel(QPainter &p);
};

#endif
