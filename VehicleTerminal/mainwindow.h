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
#include "camera_thread.h"
#include "voice_assistant.h"
#include "mjpeg_server.h"
#include "rtmp_streamer.h"


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
    void onBtnAiVoice();   // AI 语音交互（Whisper + Qwen2.5 离线语音链路）
    void onVoiceStatus(const QString &status);
    void onVoiceStt(const QString &text);
    void onVoiceAction(const QString &action, const QString &param);
    void onVoiceError(const QString &msg);
    void onVoiceFinished();
    void dispatchVoiceAction(const QString &action, const QString &param);
    void onBtnCameraToggle();  // 摄像头开关
    void onBtnCapture();  // 抓拍功能：采集当前帧，保存为 PNG 图片到本地
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
    QPushButton *btnAiVoice;
    QPushButton *btnCameraToggle;
    QPushButton *btnCapture;  // 抓拍按钮
    QImage lastFrame; 

    SettingWindow   settingWindow;
    BaiduMap       *baiduMap;
    MusicPlayer    *musicPlayer;
    Weather        *weather;
    Monitor        *monitor;

    CameraThread       *cameraThread;
    Dht11              *dht11;
    SpeechRecognition  *AsrThread;
    QTimer             *timer;
    QNetworkAccessManager *networkManage;
    QNetworkRequest       *request;
    VoiceAssistant     *m_voiceAssistant = nullptr;
    MjpegServer        *m_mjpegServer = nullptr;
    RtmpStreamer        *m_rtmpStreamer = nullptr;
    bool m_aiVoicePending = false;
    bool cameraRunning = true;  // 摄像头默认开启
    QString m_lastSttText;
    QString m_lastLlmJson;
};

#endif
