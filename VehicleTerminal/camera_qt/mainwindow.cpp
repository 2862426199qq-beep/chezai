#include "mainwindow.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 设置窗口大小
    setWindowTitle("Camera Preview");
    resize(960, 540);  // 显示1/2分辨率，板子性能够用

    // 创建显示Label，铺满窗口
    displayLabel = new QLabel(this);
    displayLabel->setAlignment(Qt::AlignCenter);
    displayLabel->setText("正在启动摄像头...");
    setCentralWidget(displayLabel);

    // 创建采集线程
    cameraThread = new CameraThread(this);

    // 信号槽连接：子线程发出frameReady → 主线程onFrameReady显示
    // Qt::QueuedConnection 保证跨线程安全
    connect(cameraThread, &CameraThread::frameReady,
            this, &MainWindow::onFrameReady,
            Qt::QueuedConnection);

    // 启动采集线程
    cameraThread->start();
}

MainWindow::~MainWindow()
{
    // 退出时停止线程
    cameraThread->stop();
    cameraThread->wait();  // 等线程真正结束
}

void MainWindow::onFrameReady(QImage img)
{
    // 缩放图像适应Label大小，保持宽高比
    // QPixmap pixmap = QPixmap::fromImage(img)
    //                      .scaled(displayLabel->size(),
    //                              Qt::KeepAspectRatio,
    //                              Qt::SmoothTransformation);
    // displayLabel->setPixmap(pixmap);
    // debug2 直接更新，不缓存
    displayLabel->setPixmap(
        QPixmap::fromImage(img).scaled(
            displayLabel->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation  // 改成Fast，减少CPU
        )
    );
}