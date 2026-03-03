#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFrame>
#include <QTime>
#include <QDate>

#include "settingwindow.h"
#include "clock.h"
#include "dht11.h"
#include "Map/baidumap.h"
#include "Music/musicplayer.h"
#include "Weather/weather.h"
#include "Monitor/monitor.h"
#include "speechrecognition.h"
#include "Radar/radar_widget.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void SendCommandToMusic(int);
    void SendCommandToMap(int);
    void SendCommandToMonitor(int);

private slots:
    void on_update_humidity_temp(QString humidity, QString temp);
    void on_timer_updateTime();
    void on_handleRecord();
    void getSpeechResult(QNetworkReply *reply);
    void onBtnMusic();
    void onBtnWeather();
    void onBtnMonitor();
    void onBtnMap();
    void onBtnSetting();
    void onBtnRadarFull();

private:
    void setupUI();
    void setupConnections();
    QPushButton* makeNavBtn(const QString &text, const QString &color);

    RadarWidget   *radarWidget;
    QLabel        *cameraLabel;
    Clock         *clockWidget;

    QLabel *lblTime;
    QLabel *lblDate;
    QLabel *lblTemp;
    QLabel *lblHumi;
    QLabel *lblCpu;
    QLabel *lblNpu;
    QLabel *lblStatus;

    QPushButton *btnMusic;
    QPushButton *btnWeather;
    QPushButton *btnMap;
    QPushButton *btnMonitor;
    QPushButton *btnSetting;
    QPushButton *btnRadarFull;

    SettingWindow   settingWindow;
    BaiduMap       *baiduMap;
    MusicPlayer    *musicPlayer;
    Weather        *weather;
    Monitor        *monitor;

    Dht11              *dht11;
    SpeechRecognition  *AsrThread;
    QTimer             *timer;
    QNetworkAccessManager *networkManage;
    QNetworkRequest       *request;
};

#endif
