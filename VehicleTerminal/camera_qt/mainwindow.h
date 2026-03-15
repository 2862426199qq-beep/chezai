#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "camera_thread.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 接收子线程发来的帧，显示到界面上
    void onFrameReady(QImage img);

private:
    QLabel        *displayLabel;   // 用来显示画面的Label
    CameraThread  *cameraThread;   // 采集线程
};

#endif