#include "mainwindow.h"
#include <QApplication>
#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QProcessEnvironment>

#include "llm_engine.h"
#include "tts_engine.h"

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

    AsrThread = new SpeechRecognition();
    AsrThread->startSpeechRecognition();

    setupUI();
    setupConnections();

    timer->start();
    dht11->start();
}

MainWindow::~MainWindow()
{
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
    connect(AsrThread, &SpeechRecognition::RecordFinished,
            this, &MainWindow::on_handleRecord);
    connect(networkManage, &QNetworkAccessManager::finished,
            this, &MainWindow::getSpeechResult);

    connect(this, SIGNAL(SendCommandToMap(int)),     baiduMap,     SLOT(on_handleCommand(int)));
    connect(this, SIGNAL(SendCommandToMonitor(int)), monitor,      SLOT(on_handleCommand(int)));
    connect(this, SIGNAL(SendCommandToMusic(int)),   musicPlayer,  SLOT(on_handleCommand(int)));

    connect(btnMusic,    &QPushButton::clicked, this, &MainWindow::onBtnMusic);
    connect(btnWeather,  &QPushButton::clicked, this, &MainWindow::onBtnWeather);
    connect(btnMap,      &QPushButton::clicked, this, &MainWindow::onBtnMap);
    connect(btnMonitor,  &QPushButton::clicked, this, &MainWindow::onBtnMonitor);
    connect(btnSetting,  &QPushButton::clicked, this, &MainWindow::onBtnSetting);
    connect(btnAiVoice,  &QPushButton::clicked, this, &MainWindow::onBtnAiVoice);
    connect(btnRadarFull,&QPushButton::clicked, this, &MainWindow::onBtnRadarFull);
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
    if (m_aiVoicePending) {
        return;
    }

    m_aiVoicePending = true;
    lblStatus->setText("AI Voice: Running...");
    lblStatus->setStyleSheet("color: #00d4ff; font-size: 13px;");

    AsrThread->startRecord();
    QTimer::singleShot(2500, this, [this]() {
        AsrThread->stopRecord();
        lblStatus->setText("AI Voice: Recognizing...");
        lblStatus->setStyleSheet("color: #00d4ff; font-size: 13px;");
    });

    QTimer::singleShot(15000, this, [this]() {
        if (m_aiVoicePending) {
            m_aiVoicePending = false;
            lblStatus->setText("AI Voice: Timeout");
            lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
        }
    });
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
        lblStatus->setText(QString("AI Voice: %1").arg(asr));

        QtConcurrent::run([this, asr]() {
            LlmEngine llm;
            TtsEngine tts;
            const std::string llmModel = "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf";

            if (!llm.init(llmModel, 4)) {
                QMetaObject::invokeMethod(this, [this]() {
                    lblStatus->setText("AI Voice: LLM Init Failed");
                    lblStatus->setStyleSheet("color: #ff4444; font-size: 13px;");
                }, Qt::QueuedConnection);
                return;
            }

            const std::string userText = asr.toStdString();
            const std::string replyText = llm.chat(userText);
            const std::string intent = llm.parseIntent(replyText);

            std::string speakText = replyText;
            const std::string tagStart = "[INTENT:";
            std::size_t tagPos = speakText.find(tagStart);
            if (tagPos != std::string::npos) {
                speakText = speakText.substr(0, tagPos);
            }
            tts.speak(speakText);

            QMetaObject::invokeMethod(this, [this, intent]() {
                if (intent == "OPEN_MUSIC") {
                    emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
                    emit SendCommandToMusic(MUSIC_COMMAND_PLAY);
                } else if (intent == "OPEN_MAP") {
                    emit SendCommandToMap(MAP_COMMAND_SHOW);
                } else if (intent == "OPEN_MONITOR") {
                    emit SendCommandToMonitor(MONITOR_COMMAND_SHOW);
                } else if (intent == "OPEN_WEATHER") {
                    weather->show();
                }

                lblStatus->setText(QString("AI Voice: %1")
                                   .arg(QString::fromStdString(intent)));
                lblStatus->setStyleSheet("color: #00ff88; font-size: 13px;");
            }, Qt::QueuedConnection);
        });
        return;
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
