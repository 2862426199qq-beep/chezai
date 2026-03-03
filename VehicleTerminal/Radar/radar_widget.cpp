#include "radar_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QtMath>

RadarWidget::RadarWidget(QWidget *parent)
    : QWidget(parent)
    , m_scanAngle(0)
    , m_maxRange(80.0)
    , m_numRings(4)
{
    /* 黑色背景 */
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(10, 15, 10));
    setPalette(pal);

    /* 启动数据读取线程 */
    m_reader = new RadarReader(this);
    connect(m_reader, &RadarReader::newFrame, this, &RadarWidget::onNewFrame);
    m_reader->start();

    /* 30fps 刷新 */
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &RadarWidget::onRefresh);
    m_timer->start(33);
}

RadarWidget::~RadarWidget()
{
    m_reader->stop();
    m_reader->wait();
}

void RadarWidget::onNewFrame(double angle)
{
    m_scanAngle = angle;
}

void RadarWidget::onRefresh()
{
    m_targets = m_reader->getTargets();
    m_scanAngle = m_reader->getScanAngle();
    update();
}

QPointF RadarWidget::polarToScreen(double range_m, double angle_deg,
                                    QPointF center, double scale) const
{
    /*
     * 雷达坐标：0° = 正前方（屏幕上方），顺时针增加
     * 转成数学角：screen_angle = 90° - radar_angle
     */
    double rad = qDegreesToRadians(angle_deg - 90.0);
    double r = range_m * scale;
    return QPointF(center.x() + r * cos(rad),
                   center.y() + r * sin(rad));
}

void RadarWidget::drawBackground(QPainter &p, QPointF center, double scale)
{
    /* 距离同心圈 */
    QPen ringPen(QColor(0, 80, 0), 1, Qt::DashLine);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);

    for (int i = 1; i <= m_numRings; i++) {
        double range = (m_maxRange / m_numRings) * i;
        double r = range * scale;
        p.drawEllipse(center, r, r);

        /* 距离标注 */
        p.setPen(QColor(0, 120, 0));
        p.setFont(QFont("WenQuanYi Zen Hei", 9));
        p.drawText(QPointF(center.x() + 4, center.y() - r + 14),
                   QString("%1m").arg((int)range));
        p.setPen(ringPen);
    }

    /* 十字方位线 */
    QPen crossPen(QColor(0, 60, 0), 1);
    p.setPen(crossPen);
    double maxR = m_maxRange * scale;

    /* 0° / 90° / 180° / 270° */
    p.drawLine(QPointF(center.x(), center.y() - maxR),
               QPointF(center.x(), center.y() + maxR));
    p.drawLine(QPointF(center.x() - maxR, center.y()),
               QPointF(center.x() + maxR, center.y()));

    /* 45° 对角线 */
    QPen diagPen(QColor(0, 40, 0), 1, Qt::DotLine);
    p.setPen(diagPen);
    for (int deg = 45; deg < 360; deg += 90) {
        QPointF edge = polarToScreen(m_maxRange, deg, center, scale);
        p.drawLine(center, edge);
    }

    /* 方位标注 */
    p.setPen(QColor(0, 160, 0));
    p.setFont(QFont("WenQuanYi Zen Hei", 11, QFont::Bold));
    p.drawText(QPointF(center.x() - 6, center.y() - maxR - 8), "前");
    p.drawText(QPointF(center.x() - 6, center.y() + maxR + 18), "后");
    p.drawText(QPointF(center.x() + maxR + 8, center.y() + 5), "右");
    p.drawText(QPointF(center.x() - maxR - 24, center.y() + 5), "左");

    /* 中心点（车身） */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 200, 0));
    p.drawEllipse(center, 4, 4);
}

void RadarWidget::drawScanLine(QPainter &p, QPointF center, double scale)
{
    double maxR = m_maxRange * scale;

    /* 扫描扇形拖尾（30°渐变） */
    for (int i = 30; i >= 0; i--) {
        double angle = m_scanAngle - i;
        double rad = qDegreesToRadians(angle - 90.0);
        int alpha = (int)(80.0 * (1.0 - (double)i / 30.0));

        QPointF end(center.x() + maxR * cos(rad),
                    center.y() + maxR * sin(rad));

        QPen pen(QColor(0, 255, 0, alpha), 1);
        p.setPen(pen);
        p.drawLine(center, end);
    }

    /* 当前扫描线（最亮） */
    double rad = qDegreesToRadians(m_scanAngle - 90.0);
    QPointF end(center.x() + maxR * cos(rad),
                center.y() + maxR * sin(rad));
    QPen scanPen(QColor(0, 255, 0, 200), 2);
    p.setPen(scanPen);
    p.drawLine(center, end);
}

void RadarWidget::drawTargets(QPainter &p, QPointF center, double scale)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const auto& pt : m_targets) {
        /* 淡出效果：越旧越透明 */
        qint64 age = now - pt.timestamp_ms;
        if (age > 5000) continue;

        double fade = 1.0 - (double)age / 5000.0;
        int alpha = (int)(255 * fade);

        /* 颜色根据目标类型 */
        QColor color;
        if (pt.label == "行人")      color = QColor(255, 255, 0, alpha);   /* 黄色 */
        else if (pt.label == "汽车") color = QColor(255, 60, 60, alpha);   /* 红色 */
        else if (pt.label == "路障") color = QColor(60, 150, 255, alpha);  /* 蓝色 */
        else                         color = QColor(200, 200, 200, alpha); /* 灰色 */

        QPointF pos = polarToScreen(pt.range_m, pt.angle_deg, center, scale);

        /* 目标点 */
        double dotSize = 4.0 + 6.0 * fade;
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pos, dotSize, dotSize);

        /* 光晕效果（最近 1 秒的点） */
        if (age < 1000) {
            QColor glow = color;
            glow.setAlpha(alpha / 3);
            p.setBrush(glow);
            p.drawEllipse(pos, dotSize * 2, dotSize * 2);
        }

        /* 标注文字（只对最新的点显示） */
        if (age < 200) {
            p.setPen(color);
            p.setFont(QFont("WenQuanYi Zen Hei", 9));
            QString info = QString("%1 %2m %3m/s")
                .arg(pt.label)
                .arg(pt.range_m, 0, 'f', 1)
                .arg(pt.velocity_mps, 0, 'f', 1);
            p.drawText(pos + QPointF(12, -8), info);
        }
    }
}

void RadarWidget::drawInfoPanel(QPainter &p)
{
    p.setPen(QColor(0, 180, 0));
    p.setFont(QFont("WenQuanYi Zen Hei", 10));

    int y = 20;
    p.drawText(10, y, QString("扫描角度: %1°").arg(m_scanAngle, 0, 'f', 1));
    y += 20;

    /* 统计最近 1 秒内的活跃目标 */
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int pedestrian = 0, car = 0, obstacle = 0;
    for (const auto& pt : m_targets) {
        if (now - pt.timestamp_ms > 1000) continue;
        if (pt.label == "行人") pedestrian++;
        else if (pt.label == "汽车") car++;
        else if (pt.label == "路障") obstacle++;
    }

    p.setPen(QColor(255, 255, 0));
    p.drawText(10, y, QString("● 行人: %1").arg(pedestrian));
    y += 20;
    p.setPen(QColor(255, 60, 60));
    p.drawText(10, y, QString("● 汽车: %1").arg(car));
    y += 20;
    p.setPen(QColor(60, 150, 255));
    p.drawText(10, y, QString("● 路障: %1").arg(obstacle));
}

void RadarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    /* 计算中心和缩放 */
    int side = qMin(width(), height());
    double margin = 50.0;
    double scale = (side / 2.0 - margin) / m_maxRange;
    QPointF center(width() / 2.0, height() / 2.0);

    /* 绘制各层 */
    drawBackground(p, center, scale);
    drawScanLine(p, center, scale);
    drawTargets(p, center, scale);
    drawInfoPanel(p);
}
