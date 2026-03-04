#include <QApplication>
#include <QObject>
#include "BluetoothAudio.h"
#include "radar_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BluetoothAudio btAudio;
    btAudio.start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&btAudio]() {
        btAudio.stop();
    });

    RadarWidget w;
    w.setWindowTitle("FMCW 雷达 360° 扫描");
    w.resize(800, 800);
    w.show();

    return app.exec();
}
