#include <QApplication>
#include "radar_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    RadarWidget w;
    w.setWindowTitle("FMCW 雷达 360° 扫描");
    w.resize(800, 800);
    w.show();

    return app.exec();
}
