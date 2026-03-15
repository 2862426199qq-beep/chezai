#include "mainwindow.h"
#include <QApplication>
#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QProcessEnvironment>

#include <QDateTime>
static const char* DARK_STYLE = R"(
    QMainWindow {
        background-color: #1a1a2e;
    }
    QLabel {
        color: #e0e0e0;
        font-family: "WenQuanYi Zen Hei", "Noto Sans CJK SC", sans-serif;
    }
    QPushButton#navBtn {
        background-color: #16213e;
        color: #e0e0e0;
        border: 1px solid #0f3460;
        border-radius: 8px;
        padding: 8px;
        font-size: 13px;
        font-family: "WenQuanYi Zen Hei", sans-serif;
    }
    QPushButton#navBtn:hover {
        background-color: #0f3460;
        border: 1px solid #00d4ff;
    }
    QPushButton#navBtn:pressed {
        background-color: #533483;
    }
    QFrame#separator {
        background-color: #0f3460;
    }
)";

static QString resolveBaiduAsrToken()
{
    const QString envToken = qEnvironmentVariable("BAIDU_ASR_TOKEN").trimmed();
    if (!envToken.isEmpty()) {
        return envToken;
    }
    return "24.464f01bbb7583296b1e578ddf66073f0.2592000.1731685220.282335-115895512";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setStyleSheet(DARK_STYLE);
    resize(1024, 600);
    setWindowTitle("AI Smart Cockpit - RK3588S");

    baiduMap    = new BaiduMap(this);
    monitor     = Monitor::getInstance();
    musicPlayer = new MusicPlayer(this);
    weather     = new Weather(this);

    dht11 = new Dht11;
    timer = new QTimer(this);
    timer->setInterval(500);

    request       = new QNetworkRequest;
    networkManage = new QNetworkAccessManager(this);
    request->setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("application/json"));

    // Legacy ASR thread is kept for backward compatibility but not started.
    // VoiceAssistant owns the new Whisper + Qwen pipeline and should be the only recorder path.
    AsrThread = new SpeechRecognition();

    cameraThread = new CameraThread(this);

    m_voiceAssistant = new VoiceAssistant(this);
    m_voiceAssistant->preInitAudio();   // 启动时异步预热音频，首次语音不再等待
    m_voiceAssistant->preInitLlm();     // 启动时后台预加载 LLM 模型(~10s)，首次语音不再等待

    m_mjpegServer = new MjpegServer(8080, this);

    // RTMP 推流：传入 RTMP 服务器地址（为空则不推流，仅初始化编码器）
    // 使用前需修改为实际 RTMP 地址，如 "rtmp://你的PC-IP/live/camera"
    QString rtmpUrl = qEnvironmentVariable("RTMP_URL").trimmed();
    if (!rtmpUrl.isEmpty()) {
        m_rtmpStreamer = new RtmpStreamer(WIDTH, HEIGHT, 15, rtmpUrl, this);
    }

    setupUI();
    setupConnections();

    timer->start();
    dht11->start();
    cameraThread->start();
}

MainWindow::~MainWindow()
{
    if (m_rtmpStreamer) {
        m_rtmpStreamer->stop();
        m_rtmpStreamer->wait();
    }
    cameraThread->stop();
    cameraThread->wait();
    delete request;
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    /* ---- Left: Radar PPI ---- */
    QVBoxLayout *leftCol = new QVBoxLayout;
    {
        QLabel *radarTitle = new QLabel("◉ FMCW Radar Scan");
        radarTitle->setStyleSheet("color: #00d4ff; font-size: 14px; font-weight: bold;");

        radarWidget = new RadarWidget(this);
        radarWidget->setMinimumSize(360, 360);

        btnRadarFull = new QPushButton("⤢ Fullscreen Radar");
        btnRadarFull->setObjectName("navBtn");
        btnRadarFull->setFixedHeight(30);

        leftCol->addWidget(radarTitle);
        leftCol->addWidget(radarWidget, 1);
        leftCol->addWidget(btnRadarFull);
    }

    /* ---- Middle: Camera + Nav ---- */
    QVBoxLayout *midCol = new QVBoxLayout;
    {
        QLabel *camTitle = new QLabel("◉ Video / YOLO AI");
        camTitle->setStyleSheet("color: #00d4ff; font-size: 14px; font-weight: bold;");

        cameraLabel = new QLabel("Camera Not Connected\n\nConnect camera for\nreal-time YOLOv5 detection");
        cameraLabel->setAlignment(Qt::AlignCenter);
        cameraLabel->setStyleSheet(
            "background-color: #0d1117;"
            "border: 2px dashed #0f3460;"
            "border-radius: 8px;"
            "color: #666;"
            "font-size: 14px;"
        );
        cameraLabel->setMinimumSize(300, 280);

        btnCameraToggle = new QPushButton("■ 关闭摄像头");
        btnCameraToggle->setObjectName("navBtn");
        btnCameraToggle->setFixedHeight(32);
        btnCameraToggle->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #ff4444; font-size: 13px; }"
        );

        btnCapture = new QPushButton("📸 抓拍");
        btnCapture->setObjectName("navBtn");
        btnCapture->setFixedHeight(32);
        btnCapture->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #2196f3; font-size: 13px; }"
        );
        QHBoxLayout *navRow = new QHBoxLayout;
        navRow->setSpacing(6);

        btnMusic   = makeNavBtn("Music",   "#e91e63");
        btnWeather = makeNavBtn("Weather", "#ff9800");
        btnMap     = makeNavBtn("Map",     "#4caf50");
        btnMonitor = makeNavBtn("Monitor", "#2196f3");
        btnSetting = makeNavBtn("Setting", "#9c27b0");
        btnAiVoice = makeNavBtn("AI Voice", "#00d4ff");

        navRow->addWidget(btnMusic);
        navRow->addWidget(btnWeather);
        navRow->addWidget(btnMap);
        navRow->addWidget(btnMonitor);
        navRow->addWidget(btnSetting);
        navRow->addWidget(btnAiVoice);

        midCol->addWidget(camTitle);
        midCol->addWidget(cameraLabel, 1);
        QHBoxLayout *camBtnRow = new QHBoxLayout;
        camBtnRow->addWidget(btnCameraToggle);
        camBtnRow->addWidget(btnCapture);
        midCol->addLayout(camBtnRow);   
        midCol->addSpacing(6);
        midCol->addLayout(navRow);
    }

    /* ---- Right: Status ---- */
    QVBoxLayout *rightCol = new QVBoxLayout;
    {
        lblTime = new QLabel("--:--:--");
        lblTime->setStyleSheet("color: #ffffff; font-size: 36px; font-weight: bold;");
        lblTime->setAlignment(Qt::AlignCenter);

        lblDate = new QLabel("----.--.--");
        lblDate->setStyleSheet("color: #00d4ff; font-size: 16px;");
        lblDate->setAlignment(Qt::AlignCenter);

        QFrame *sep1 = new QFrame;
        sep1->setObjectName("separator");
        sep1->setFrameShape(QFrame::HLine);
        sep1->setFixedHeight(2);

        QLabel *envTitle = new QLabel("◉ Environment");
        envTitle->setStyleSheet("color: #00d4ff; font-size: 14px; font-weight: bold;");

        QHBoxLayout *tempRow = new QHBoxLayout;
        QLabel *tempIcon = new QLabel("Temp:");
        tempIcon->setStyleSheet("color: #999; font-size: 14px;");
        lblTemp = new QLabel("--.-");
        lblTemp->setStyleSheet("color: #ff4444; font-size: 20px; font-weight: bold;");
        QLabel *tempUnit = new QLabel("C");
        tempUnit->setStyleSheet("color: #999; font-size: 12px;");
        tempRow->addWidget(tempIcon);
        tempRow->addWidget(lblTemp);
        tempRow->addWidget(tempUnit);
        tempRow->addStretch();

        QHBoxLayout *humiRow = new QHBoxLayout;
        QLabel *humiIcon = new QLabel("Humi:");
        humiIcon->setStyleSheet("color: #999; font-size: 14px;");
        lblHumi = new QLabel("--.-");
        lblHumi->setStyleSheet("color: #00ff88; font-size: 20px; font-weight: bold;");
        QLabel *humiUnit = new QLabel("%");
        humiUnit->setStyleSheet("color: #999; font-size: 12px;");
        humiRow->addWidget(humiIcon);
        humiRow->addWidget(lblHumi);
        humiRow->addWidget(humiUnit);
        humiRow->addStretch();

        QFrame *sep2 = new QFrame;
        sep2->setObjectName("separator");
        sep2->setFrameShape(QFrame::HLine);
        sep2->setFixedHeight(2);

        QLabel *sysTitle = new QLabel("◉ System");
        sysTitle->setStyleSheet("color: #00d4ff; font-size: 14px; font-weight: bold;");

        lblCpu = new QLabel("CPU: ---%");
        lblCpu->setStyleSheet("color: #44aaff; font-size: 14px;");
        lblNpu = new QLabel("NPU: Standby");
        lblNpu->setStyleSheet("color: #44aaff; font-size: 14px;");
        lblStatus = new QLabel("Radar: Online");
        lblStatus->setStyleSheet("color: #00ff88; font-size: 13px;");

        QFrame *sep3 = new QFrame;
        sep3->setObjectName("separator");
        sep3->setFrameShape(QFrame::HLine);
        sep3->setFixedHeight(2);

        QLabel *clockTitle = new QLabel("◉ Clock");
        clockTitle->setStyleSheet("color: #00d4ff; font-size: 14px; font-weight: bold;");

        clockWidget = new Clock(this);
        clockWidget->setFixedSize(150, 150);

        rightCol->addWidget(lblTime);
        rightCol->addWidget(lblDate);
        rightCol->addWidget(sep1);
        rightCol->addWidget(envTitle);
        rightCol->addLayout(tempRow);
        rightCol->addLayout(humiRow);
        rightCol->addWidget(sep2);
        rightCol->addWidget(sysTitle);
        rightCol->addWidget(lblCpu);
        rightCol->addWidget(lblNpu);
        rightCol->addWidget(lblStatus);
        rightCol->addWidget(sep3);
        rightCol->addWidget(clockTitle);
        rightCol->addWidget(clockWidget, 0, Qt::AlignCenter);
        rightCol->addStretch();
    }

    mainLayout->addLayout(leftCol, 4);

    QFrame *vSep1 = new QFrame;
    vSep1->setObjectName("separator");
    vSep1->setFrameShape(QFrame::VLine);
    vSep1->setFixedWidth(2);
    mainLayout->addWidget(vSep1);

    mainLayout->addLayout(midCol, 4);

    QFrame *vSep2 = new QFrame;
    vSep2->setObjectName("separator");
    vSep2->setFrameShape(QFrame::VLine);
    vSep2->setFixedWidth(2);
    mainLayout->addWidget(vSep2);

    mainLayout->addLayout(rightCol, 2);
}

QPushButton* MainWindow::makeNavBtn(const QString &text, const QString &color)
{
    QPushButton *btn = new QPushButton(text);
    btn->setObjectName("navBtn");
    btn->setFixedHeight(45);
    btn->setStyleSheet(
        QString("QPushButton#navBtn { border-bottom: 3px solid %1; }").arg(color)
    );
    return btn;
}

void MainWindow::setupConnections()
{
    connect(timer, &QTimer::timeout, this, &MainWindow::on_timer_updateTime);
    connect(dht11, SIGNAL(updateDht11Data(QString,QString)),
            this,  SLOT(on_update_humidity_temp(QString,QString)));
        // NOTE: legacy Baidu-ASR callback chain is intentionally disconnected to avoid
        // concurrent recorder/network paths interfering with VoiceAssistant.

    connect(this, SIGNAL(SendCommandToMap(int)),     baiduMap,     SLOT(on_handleCommand(int)));
    connect(this, SIGNAL(SendCommandToMonitor(int)), monitor,      SLOT(on_handleCommand(int)));
    connect(this, SIGNAL(SendCommandToMusic(int)),   musicPlayer,  SLOT(on_handleCommand(int)));

    connect(btnMusic,    &QPushButton::clicked, this, &MainWindow::onBtnMusic);
    connect(btnWeather,  &QPushButton::clicked, this, &MainWindow::onBtnWeather);
    connect(btnMap,      &QPushButton::clicked, this, &MainWindow::onBtnMap);
    connect(btnMonitor,  &QPushButton::clicked, this, &MainWindow::onBtnMonitor);
    connect(btnSetting,  &QPushButton::clicked, this, &MainWindow::onBtnSetting);
    connect(btnAiVoice,  &QPushButton::clicked, this, &MainWindow::onBtnAiVoice);

    // VoiceAssistant 信号连接
    connect(m_voiceAssistant, &VoiceAssistant::statusChanged,
            this, &MainWindow::onVoiceStatus);
    connect(m_voiceAssistant, &VoiceAssistant::sttResult,
            this, &MainWindow::onVoiceStt);
    connect(m_voiceAssistant, &VoiceAssistant::actionResult,
            this, &MainWindow::onVoiceAction);
    connect(m_voiceAssistant, &VoiceAssistant::errorOccurred,
            this, &MainWindow::onVoiceError);
    connect(m_voiceAssistant, &VoiceAssistant::finished,
            this, &MainWindow::onVoiceFinished);

    connect(btnRadarFull,&QPushButton::clicked, this, &MainWindow::onBtnRadarFull);
    connect(btnCameraToggle, &QPushButton::clicked, this, &MainWindow::onBtnCameraToggle);
    connect(btnCapture, &QPushButton::clicked, this, &MainWindow::onBtnCapture);
    connect(cameraThread, &CameraThread::frameReady,
            this, [this](QImage img) {
         lastFrame = img;
        cameraLabel->setPixmap(
            QPixmap::fromImage(img).scaled(
                cameraLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation)
        );
        // MJPEG 推流：每帧同步推给所有 TCP 客户端
        if (m_mjpegServer)
            m_mjpegServer->onNewFrame(img);
    });

    // NV12 原始数据 → RTMP 推流（MPP 硬件 H.264 编码）
    if (m_rtmpStreamer) {
        connect(cameraThread, &CameraThread::nv12FrameReady,
                m_rtmpStreamer, &RtmpStreamer::pushNv12Frame);
        m_rtmpStreamer->start();
    }
}
void MainWindow::onBtnCapture()
{
    if (lastFrame.isNull()) return;   // 没有帧就不保存

    QString dir = "/home/cat/chezai/VehicleTerminal/picture";

    // 4K 图片太大（24.8MB），PNG 编码会 OOM
    // 超过 1080p 时先缩到 1920×1080 再保存，降低内存压力
    QImage saveImg = lastFrame;
    if (saveImg.width() > 1920 || saveImg.height() > 1080) {
        saveImg = saveImg.scaled(1920, 1080, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QString filename = QString("%1/cap_%2.jpg")
        .arg(dir)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    saveImg.save(filename, "JPEG", 90);  // JPEG 质量 90，内存占用远小于 PNG
    qDebug() << "Captured:" << filename << saveImg.size();
}
void MainWindow::on_timer_updateTime()
{
    lblTime->setText(QTime::currentTime().toString("HH:mm:ss"));
    lblDate->setText(QDate::currentDate().toString("yyyy-MM-dd"));

    static qint64 prevIdle = 0, prevTotal = 0;
    QFile f("/proc/stat");
    if (f.open(QIODevice::ReadOnly)) {
        QString line = f.readLine();
        f.close();
        QStringList parts = line.simplified().split(' ');
        if (parts.size() >= 8) {
            qint64 user   = parts[1].toLongLong();
            qint64 nice   = parts[2].toLongLong();
            qint64 sys    = parts[3].toLongLong();
            qint64 idle   = parts[4].toLongLong();
            qint64 total  = user + nice + sys + idle;
            qint64 dTotal = total - prevTotal;
            qint64 dIdle  = idle - prevIdle;
            if (dTotal > 0 && prevTotal > 0) {
                int usage = (int)(100.0 * (1.0 - (double)dIdle / dTotal));
                lblCpu->setText(QString("CPU: %1%").arg(usage));
            }
            prevIdle = idle;
            prevTotal = total;
        }
    }
}

void MainWindow::on_update_humidity_temp(QString humidity, QString temp)
{
    lblTemp->setText(temp);
    lblHumi->setText(humidity);
}

void MainWindow::onBtnMusic()   { musicPlayer->show(); }
void MainWindow::onBtnWeather() { weather->show(); }
void MainWindow::onBtnMap()     { baiduMap->show(); }
void MainWindow::onBtnSetting() { settingWindow.ScanGif(); settingWindow.show(); }

void MainWindow::onBtnMonitor()
{
    monitor->show();
    monitor->myStart();
}

void MainWindow::onBtnCameraToggle()
{
    if (cameraRunning) {
        cameraThread->stop();
        cameraThread->wait();
        cameraRunning = false;
        btnCameraToggle->setText("▶ 开启摄像头");
        btnCameraToggle->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #4caf50; font-size: 13px; }"
        );
        cameraLabel->clear();
        cameraLabel->setText("摄像头已关闭\n\n点击下方按钮重新开启");
    } else {
        cameraThread->start();
        cameraRunning = true;
        btnCameraToggle->setText("■ 关闭摄像头");
        btnCameraToggle->setStyleSheet(
            "QPushButton#navBtn { border-bottom: 3px solid #ff4444; font-size: 13px; }"
        );
    }
}

void MainWindow::onBtnRadarFull()
{
    QWidget *container = new QWidget(nullptr);
    container->setWindowTitle("FMCW Radar 360 Scan");
    container->resize(800, 800);
    container->setAttribute(Qt::WA_DeleteOnClose);
    container->setStyleSheet("background-color: #1a1a2e;");

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(5, 5, 5, 5);

    RadarWidget *fullRadar = new RadarWidget(container);
    layout->addWidget(fullRadar, 1);

    QPushButton *btnBack = new QPushButton("X  Close / Back");
    btnBack->setStyleSheet(
        "background-color: #ff4444; color: white; font-size: 16px;"
        "font-weight: bold; border-radius: 8px; padding: 10px;"
    );
    btnBack->setFixedHeight(50);
    QObject::connect(btnBack, &QPushButton::clicked, container, &QWidget::close);
    layout->addWidget(btnBack);

    container->show();
}

void MainWindow::onBtnAiVoice()
{
    if (m_voiceAssistant->isRunning()) {
        // 再次点击 → 提前结束录音
        m_voiceAssistant->stopRecording();
        return;
    }

    lblStatus->setText("AI Voice: Starting...");
    lblStatus->setStyleSheet("color: #00d4ff; font-size: 13px;");
    btnAiVoice->setStyleSheet(
        "QPushButton#navBtn { border-bottom: 3px solid #ff4444; }");
    btnAiVoice->setText("■ Stop");

    m_voiceAssistant->start(5);  // 录音 5 秒
}

void MainWindow::onVoiceStatus(const QString &status)
{
    if (status.startsWith("LLM JSON:")) {
        m_lastLlmJson = status;
    }
    lblStatus->setText(QString("AI Voice: %1").arg(status));
    lblStatus->setStyleSheet("color: #00d4ff; font-size: 13px;");
}

void MainWindow::onVoiceStt(const QString &text)
{
    m_lastSttText = text;
    lblStatus->setText(QString("STT: %1").arg(text));
    lblStatus->setStyleSheet("color: #00ff88; font-size: 13px;");
}

void MainWindow::onVoiceAction(const QString &action, const QString &param)
{
    dispatchVoiceAction(action, param);
    if (action == "unknown" || action == "unknow") {
        QString debugText = QString("AI: unknown\nSTT:%1\n%2")
            .arg(m_lastSttText)
            .arg(m_lastLlmJson);
        lblStatus->setText(debugText);
        lblStatus->setStyleSheet("color: #ffbb33; font-size: 12px;");
    } else {
        lblStatus->setText(QString("AI: %1 %2").arg(action, param));
        lblStatus->setStyleSheet("color: #00ff88; font-size: 13px;");
    }
}

void MainWindow::onVoiceError(const QString &msg)
{
    lblStatus->setText(QString("AI Voice: %1").arg(msg));
    lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
}

void MainWindow::onVoiceFinished()
{
    btnAiVoice->setStyleSheet(
        "QPushButton#navBtn { border-bottom: 3px solid #00d4ff; }");
    btnAiVoice->setText("AI Voice");
}

void MainWindow::dispatchVoiceAction(const QString &action, const QString &param)
{
    if (action == "open_music" || action == "play_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PLAY);
    } else if (action == "close_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_CLOSE);
    } else if (action == "next_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_NEXT);
    } else if (action == "prev_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PRE);
    } else if (action == "pause_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PAUSE);
    } else if (action == "open_weather") {
        weather->show();
    } else if (action == "open_map") {
        emit SendCommandToMap(MAP_COMMAND_SHOW);
    } else if (action == "open_camera") {
        if (!cameraRunning) onBtnCameraToggle();
    } else if (action == "close_camera") {
        if (cameraRunning) onBtnCameraToggle();
    } else if (action == "open_radar") {
        onBtnRadarFull();
    } else if (action == "open_monitor" || action == "open_dht11") {
        onBtnMonitor();
    } else if (action == "open_settings") {
        onBtnSetting();
    }
    // unknown → 不执行任何操作
}

void MainWindow::on_handleRecord()
{
    QFile file("./record.wav");
    if (!file.open(QIODevice::ReadOnly)) {
        if (m_aiVoicePending) {
            m_aiVoicePending = false;
            lblStatus->setText("AI Voice: Record Failed");
            lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        }
        return;
    }
    request->setUrl(QUrl::fromUserInput("http://vop.baidu.com/server_api"));
    QByteArray fileData = file.readAll();
    file.close();
    if (m_aiVoicePending && fileData.size() < 12000) {
        m_aiVoicePending = false;
        lblStatus->setText("AI Voice: Audio Too Short/Invalid");
        lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        return;
    }

    QByteArray base64Encoded = fileData.toBase64();
    QJsonObject obj;
    obj.insert("format",  QJsonValue("wav"));
    obj.insert("rate",    QJsonValue(16000));
    obj.insert("channel", QJsonValue(1));
    obj.insert("cuid",    QJsonValue("L5a9DNZMyQD4MyipDR3ck7jhmdvtagjZ"));
    obj.insert("token",   QJsonValue(resolveBaiduAsrToken()));
    obj.insert("speech",  QJsonValue(QString(base64Encoded)));
    obj.insert("len",     QJsonValue(fileData.length()));
    if (m_aiVoicePending) {
        lblStatus->setText("AI Voice: Uploading...");
        lblStatus->setStyleSheet("color: #00d4ff; font-size: 13px;");
    }
    QNetworkReply *asrReply = networkManage->post(*request, QJsonDocument(obj).toJson());
    QTimer::singleShot(10000, this, [this, asrReply]() {
        if (!m_aiVoicePending || !asrReply) {
            return;
        }
        if (asrReply->isRunning()) {
            asrReply->abort();
        }
    });
}

void MainWindow::getSpeechResult(QNetworkReply *reply)
{
    if (m_aiVoicePending && reply->error() != QNetworkReply::NoError) {
        m_aiVoicePending = false;
        lblStatus->setText("AI Voice: ASR Network Error");
        lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        return;
    }

    QByteArray content = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if (!doc.isObject()) {
        if (m_aiVoicePending) {
            m_aiVoicePending = false;
            lblStatus->setText("AI Voice: ASR Parse Failed");
            lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        }
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray results = obj.value("result").toArray();
    if (results.isEmpty()) {
        if (m_aiVoicePending) {
            m_aiVoicePending = false;
            QString errMsg = obj.value("err_msg").toString();
            int errNo = obj.value("err_no").toInt(-1);
            if (errNo == 3302) {
                errMsg = "Token Invalid(3302), set BAIDU_ASR_TOKEN";
            }
            if (errMsg.isEmpty()) {
                errMsg = "ASR Empty Result";
            }
            lblStatus->setText(QString("AI Voice: %1").arg(errMsg));
            lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        }
        return;
    }
    QString asr = results.at(0).toString();

    if (m_aiVoicePending) {
        m_aiVoicePending = false;
        lblStatus->setText(QString("ASR: %1").arg(asr));
        lblStatus->setStyleSheet("color: #00ff88; font-size: 13px;");
    }

    if      (asr.contains("播放音乐")) { emit SendCommandToMusic(MUSIC_COMMAND_SHOW); emit SendCommandToMusic(MUSIC_COMMAND_PLAY); }
    else if (asr.contains("下一首"))   { emit SendCommandToMusic(MUSIC_COMMAND_SHOW); emit SendCommandToMusic(MUSIC_COMMAND_NEXT); }
    else if (asr.contains("上一首"))   { emit SendCommandToMusic(MUSIC_COMMAND_SHOW); emit SendCommandToMusic(MUSIC_COMMAND_PRE); }
    else if (asr.contains("暂停音乐")) { emit SendCommandToMusic(MUSIC_COMMAND_SHOW); emit SendCommandToMusic(MUSIC_COMMAND_PAUSE); }
    else if (asr.contains("退出音乐")) { emit SendCommandToMusic(MUSIC_COMMAND_CLOSE); this->show(); }
    else if (asr.contains("打开地图")) { emit SendCommandToMap(MAP_COMMAND_SHOW); }
    else if (asr.contains("地图放大")) { emit SendCommandToMap(MAP_COMMAND_SHOW); emit SendCommandToMap(MAP_COMMAND_AMPLIFY); }
    else if (asr.contains("地图缩小")) { emit SendCommandToMap(MAP_COMMAND_SHOW); emit SendCommandToMap(MAP_COMMAND_SHRINK); }
    else if (asr.contains("开始倒车")) { emit SendCommandToMonitor(MONITOR_COMMAND_SHOW); }
    else if (asr.contains("查看天气")) { weather->show(); }
}
