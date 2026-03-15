#include "voice_assistant.h"
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>

static QString normalizeIntentText(QString t)
{
    t = t.trimmed().toLower();
    // 去空白
    t.remove(QRegularExpression("\\s+"));
    // 去中英文常见标点，提升匹配鲁棒性
    t.remove(QRegularExpression("[,.!?;:，。！？；：、\"'“”‘’()（）\\[\\]【】<>《》-]"));
    return t;
}

static QString fallbackActionFromText(const QString &text)
{
    const QString t = normalizeIntentText(text);
    if (t.isEmpty()) return "unknown";

    if (t.contains("打开音乐") || t.contains("打開音樂") ||
        t.contains("播放音乐") || t.contains("播放音樂") ||
        t.contains("放音乐") || t.contains("放音樂") ||
        t.contains("波上音乐") || t.contains("波上音樂") ||
        t.contains("播上音乐") || t.contains("播上音樂") ||
        t.contains("openmusic") || t.contains("playmusic")) {
        return "open_music";
    }
    if (t.contains("下一首")) return "next_music";
    if (t.contains("上一首")) return "prev_music";
    if (t.contains("下一曲")) return "next_music";
    if (t.contains("上一曲")) return "prev_music";
    if (t.contains("暂停") || t.contains("暫停")) return "pause_music";
    if (t.contains("天气") || t.contains("天氣")) return "open_weather";
    if (t.contains("地图") || t.contains("地圖") || t.contains("导航") || t.contains("導航")) return "open_map";
    if (t.contains("打开摄像头") || t.contains("打開攝像頭")) return "open_camera";
    if (t.contains("关闭摄像头") || t.contains("關閉攝像頭")) return "close_camera";
    if (t.contains("雷达") || t.contains("雷達")) return "open_radar";
    if (t.contains("设置") || t.contains("設置")) return "open_settings";
    if (t.contains("监控") || t.contains("監控") || t.contains("温湿度") || t.contains("溫濕度")) return "open_monitor";

    return "unknown";
}

// 外部二进制路径 —— 按实际板端路径配置
static const QString WHISPER_DIR =
    "/home/cat/chezai/ai_voice/whisper/rknn_model_zoo/install/"
    "rk3588_linux_aarch64/rknn_whisper_demo";
static const QString LLM_DIR   = "/home/cat/chezai/ai_voice/demo_Linux_aarch64";
static const QString LLM_MODEL = "/home/cat/chezai/ai_voice/Qwen2.5-0.5B_W8A8_RK3588.rkllm";
static const QString AUDIO_INIT_HP  = "/home/cat/chezai/ai_voice/voice/audio_init_hp.sh";
static const QString AUDIO_INIT_SPK = "/home/cat/chezai/ai_voice/voice/audio_init_spk.sh";
static const QString AUDIO_TMP = "/tmp/voice_qt_input.wav";

VoiceAssistant::VoiceAssistant(QObject *parent)
    : QObject(parent)
    , m_whisperDir(WHISPER_DIR)
    , m_llmDir(LLM_DIR)
    , m_llmBin(LLM_DIR + "/llm_demo")
    , m_llmModel(LLM_MODEL)
    , m_audioTmp(AUDIO_TMP)
    , m_audioInitHp(AUDIO_INIT_HP)
    , m_audioInitSpk(AUDIO_INIT_SPK)
{
}

VoiceAssistant::~VoiceAssistant()
{
    stopRecording();
    shutdownLlm();
}

/* ---- public API ---- */

void VoiceAssistant::start(int recordSeconds)
{
    if (m_running) return;
    m_running = true;
    m_stopRequested = false;

    QtConcurrent::run([this, recordSeconds]() {
        runPipeline(recordSeconds);
    });
}

void VoiceAssistant::stopRecording()
{
    m_stopRequested = true;
    if (m_recordProc && m_recordProc->state() != QProcess::NotRunning) {
        m_recordProc->kill();       // kill timeout+parecord 进程组
        m_recordProc->waitForFinished(2000);
    }
}

void VoiceAssistant::preInitAudio()
{
    if (m_audioInited) return;
    QtConcurrent::run([this]() {
        qDebug() << "[VoiceAssistant] 后台预初始化音频...";
        if (!initAudio()) {
            qDebug() << "[VoiceAssistant] 音频预初始化告警:" << m_lastError;
        }
        m_audioInited = true;
        qDebug() << "[VoiceAssistant] 音频预初始化完成";
    });
}

void VoiceAssistant::preInitLlm()
{
    QtConcurrent::run([this]() {
        qDebug() << "[VoiceAssistant] 后台预加载 LLM 模型...";
        QMutexLocker lock(&m_llmMutex);
        if (ensureLlmRunning()) {
            qDebug() << "[VoiceAssistant] LLM 模型预加载完成";
        } else {
            qDebug() << "[VoiceAssistant] LLM 预加载失败:" << m_lastError;
        }
    });
}

/* ---- 内部流水线 ---- */

void VoiceAssistant::runPipeline(int recordSeconds)
{
    auto emitStatus = [this](const QString &s) {
        QMetaObject::invokeMethod(this, [this, s]() {
            emit statusChanged(s);
        }, Qt::QueuedConnection);
    };
    auto emitError = [this](const QString &s) {
        QMetaObject::invokeMethod(this, [this, s]() {
            emit errorOccurred(s);
            m_running = false;
            emit finished();
        }, Qt::QueuedConnection);
    };

    // 1. 音频初始化（仅首次，减少每次点击的等待）
    if (!m_audioInited) {
        emitStatus("初始化音频...");
        if (!initAudio()) {
            // 初始化失败不阻断，继续尝试录音（部分场景下脚本超时但默认路由可用）
            emitStatus(QString("音频初始化告警: %1").arg(m_lastError));
        }
        m_audioInited = true;
    }

    // 2. 录音
    emitStatus("录音中，请说话...");
    if (!recordAudio(recordSeconds)) {
        emitError(QString("录音失败: %1").arg(m_lastError));
        return;
    }
    if (m_stopRequested) {
        QMetaObject::invokeMethod(this, [this]() {
            m_running = false;
            emit finished();
        }, Qt::QueuedConnection);
        return;
    }

    // 3. 归一化
    emitStatus("音频归一化...");
    normalizeAudio();

    // 4. Whisper 识别
    emitStatus("Whisper 识别中...");
    QString text = runWhisper();
    if (text.isEmpty()) {
        if (m_lastError.isEmpty()) {
            emitError("未识别到语音");
        } else {
            emitError(QString("Whisper 失败: %1").arg(m_lastError));
        }
        return;
    }
    QMetaObject::invokeMethod(this, [this, text]() {
        emit sttResult(text);
    }, Qt::QueuedConnection);

    // 5. LLM 推理
    emitStatus("LLM 推理中...");
    QString jsonStr = runLlm(text);
    emitStatus(QString("LLM JSON: %1").arg(jsonStr));
    if (jsonStr.isEmpty()) {
        emitError(QString("LLM 失败: %1").arg(m_lastError));
        return;
    }

    // 6. 解析 JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QString action = "unknown";
    QString param;
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        action = obj.value("action").toString("unknown");
        param  = obj.value("param").toString();
    } else {
        // 容错: 处理非严格 JSON 文本，如 action:open_music,param:xxx
        QRegularExpression actionRe("action\\s*[:=]\\s*['\"]?([a-zA-Z_]+)");
        QRegularExpression paramRe("param\\s*[:=]\\s*['\"]?([^,}\\n\"']+)");
        auto a = actionRe.match(jsonStr);
        auto p = paramRe.match(jsonStr);
        if (a.hasMatch()) action = a.captured(1);
        if (p.hasMatch()) param = p.captured(1).trimmed();
    }

    QString normalized = action.trimmed().toLower();
    if (normalized == "unknow") normalized = "unknown";

    // LLM 可能输出不在列表内的近义 action，做别名映射
    static const QMap<QString, QString> aliasMap = {
        {"set_name", "open_settings"}, {"settings", "open_settings"},
        {"set_settings", "open_settings"}, {"setting", "open_settings"},
        {"play", "play_music"}, {"music", "open_music"},
        {"next", "next_music"}, {"prev", "prev_music"},
        {"previous_music", "prev_music"}, {"stop_music", "pause_music"},
        {"weather", "open_weather"}, {"map", "open_map"},
        {"navigation", "open_map"}, {"navigate", "open_map"},
        {"camera", "open_camera"}, {"radar", "open_radar"},
        {"monitor", "open_monitor"},
    };
    if (aliasMap.contains(normalized)) {
        normalized = aliasMap.value(normalized);
    }

    if (normalized.isEmpty() || normalized == "unknown" || normalized == "xxx") {
        normalized = fallbackActionFromText(text);
        if (param.isEmpty() && normalized == "open_music") {
            param = text;
        }
    }
    action = normalized;

    QMetaObject::invokeMethod(this, [this, action, param]() {
        emit actionResult(action, param);
        m_running = false;
        emit finished();
    }, Qt::QueuedConnection);
}

/* ---- 音频初始化 ---- */

bool VoiceAssistant::initAudio()
{
    // 检测耳机 Jack (numid=37)
    QProcess proc;
    proc.start("amixer", QStringList() << "-c" << "2" << "cget" << "numid=37");
    proc.waitForFinished(3000);
    QString out = QString::fromUtf8(proc.readAllStandardOutput());

    QString script;
    if (out.contains("values=on")) {
        script = m_audioInitHp;
    } else {
        script = m_audioInitSpk;
    }

    QProcess init;
    init.start("timeout", QStringList() << "8s" << "bash" << script);
    if (!init.waitForFinished(10000)) {
        init.kill();
        init.waitForFinished(2000);
        setLastError("audio_init 脚本超时");
        return false;
    }

    if (init.exitStatus() != QProcess::NormalExit) {
        setLastError("audio_init 脚本异常退出");
        return false;
    }

    // timeout 命令的超时退出码常见为 124/137/143
    if (init.exitCode() == 124 || init.exitCode() == 137 || init.exitCode() == 143) {
        setLastError(QString("audio_init 脚本超时(code=%1)").arg(init.exitCode()));
        return false;
    }

    // PulseAudio 录音增益恢复 100%（端口/Mux 路由修正后硬件 PGA 24dB 已足够，
    // 过高的软增益会放大风扇/环境噪声导致 Whisper 识别空白）
    QProcess boost;
    boost.start("pactl", QStringList()
        << "set-source-volume"
        << "alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"
        << "100%");
    boost.waitForFinished(3000);

    // 兜底: 直接用 amixer 强制打开麦克风开关和增益
    // 根据 Jack 状态设置对应的 mic switch
    auto amixerSet = [](const QString &numid, const QString &val) {
        QProcess p;
        p.start("amixer", QStringList() << "-c" << "2" << "cset" << numid << val);
        p.waitForFinished(2000);
    };
    if (out.contains("values=on")) {
        // 耳机模式
        amixerSet("numid=42", "on");   // Headset Mic Switch
        amixerSet("numid=45", "0");    // Differential Mux → Line 1 (耳机麦)
    } else {
        // 扬声器模式
        amixerSet("numid=41", "on");   // Main Mic Switch
        amixerSet("numid=45", "1");    // Differential Mux → Line 2 (板载麦)
    }
    amixerSet("numid=40", "on");   // Speaker Switch
    amixerSet("numid=27", "8");    // Left Capture Volume (PGA max)
    amixerSet("numid=28", "8");    // Right Capture Volume (PGA max)

    setLastError(QString());
    return true;
}

/* ---- 录音 ---- */

bool VoiceAssistant::recordAudio(int seconds)
{
    // 每次录音前动态检测 Jack，选择正确的麦克风输入
    // 插耳机(带麦) → [In] Headset + Differential Mux=Line1 + Headset Mic Switch + 300%
    // 无耳机      → [In] Mic     + Differential Mux=Line2 + Main Mic Switch    + 150%
    QProcess jackProc;
    jackProc.start("amixer", QStringList() << "-c" << "2" << "cget" << "numid=37");
    jackProc.waitForFinished(2000);
    bool headsetPlugged = QString::fromUtf8(jackProc.readAllStandardOutput()).contains("values=on");

    const QString sourcePort = headsetPlugged ? "[In] Headset" : "[In] Mic";
    // 耳机麦信号弱需要高增益; 板载麦 150% 平衡语音音量和风扇噪声
    const QString volume = headsetPlugged ? "300%" : "150%";
    qDebug() << "[VoiceAssistant] Jack=" << (headsetPlugged ? "插入" : "拔出")
             << ", 使用端口:" << sourcePort << ", 增益:" << volume;

    // 1) 源端口
    {
        QProcess p;
        p.start("pactl", QStringList()
            << "set-source-port"
            << "alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"
            << sourcePort);
        p.waitForFinished(2000);
    }
    // 2) 录音增益
    {
        QProcess p;
        p.start("pactl", QStringList()
            << "set-source-volume"
            << "alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"
            << volume);
        p.waitForFinished(2000);
    }
    // 3) amixer 强制设置路由
    auto amixerForce = [](const QString &numid, const QString &val) {
        QProcess p;
        p.start("amixer", QStringList() << "-c" << "2" << "cset" << numid << val);
        p.waitForFinished(1000);
    };
    if (headsetPlugged) {
        amixerForce("numid=45", "0");  // Differential Mux → Line 1 (耳机麦克风)
        amixerForce("numid=42", "on"); // Headset Mic Switch
    } else {
        amixerForce("numid=45", "1");  // Differential Mux → Line 2 (板载麦克风)
        amixerForce("numid=41", "on"); // Main Mic Switch
    }
    amixerForce("numid=27", "8");  // Left PGA max
    amixerForce("numid=28", "8");  // Right PGA max

    // 等待 ALSA 路由稳定
    QThread::msleep(100);

    // 删除旧临时文件
    QFile::remove(m_audioTmp);

    m_recordProc = new QProcess;
    QString parecordErr;
    // 用 timeout 包裹 parecord，录满指定秒数后自动结束
    // timeout 发 SIGTERM 后 parecord 会正常刷新 WAV header
    // 不手动 terminate()，避免出现只有 44 字节 header 的空文件
    m_recordProc->start("timeout",
        QStringList() << QString::number(seconds)
                      << "parecord" << "--file-format=wav" << m_audioTmp);

    if (!m_recordProc->waitForStarted(3000)) {
        setLastError(QString("parecord 启动失败: %1").arg(m_recordProc->errorString()));
        delete m_recordProc;
        m_recordProc = nullptr;
        return false;
    }

    // 等 timeout 自然结束，多给 2 秒余量
    if (!m_recordProc->waitForFinished((seconds + 2) * 1000)) {
        m_recordProc->kill();
        m_recordProc->waitForFinished(2000);
    }

    parecordErr = QString::fromUtf8(m_recordProc->readAllStandardError()).trimmed();
    delete m_recordProc;
    m_recordProc = nullptr;

    // 等 WAV header 刷新
    QThread::msleep(300);

    QFileInfo fi(m_audioTmp);
    if (!fi.exists()) {
        setLastError("录音文件未生成");
        return false;
    }
    if (fi.size() < 512) {
        if (!parecordErr.isEmpty()) {
            setLastError(QString("录音数据过小: %1 bytes, parecord=%2").arg(fi.size()).arg(parecordErr));
        } else {
            setLastError(QString("录音数据过小: %1 bytes").arg(fi.size()));
        }
        return false;
    }

    // 检测振幅
    QProcess sox;
    sox.start("sox", QStringList() << m_audioTmp << "-n" << "stat");
    sox.waitForFinished(5000);
    QString statOut = QString::fromUtf8(sox.readAllStandardError());
    QRegularExpression re("Maximum amplitude:\\s+([\\d.]+)");
    auto match = re.match(statOut);
    if (match.hasMatch()) {
        double amp = match.captured(1).toDouble();
        // 低振幅只告警，不直接判定失败，避免出现“总是录音失败”
        if (amp < 0.0005) {
            qDebug() << "[VoiceAssistant] 音频振幅较低, amp=" << amp;
        }
    }
    setLastError(QString());
    return true;
}

/* ---- 归一化 ---- */

bool VoiceAssistant::normalizeAudio()
{
    // 不做重采样，让 Whisper demo 内部 resample（质量更好）
    // 1) 高通滤波: 去除风扇低频噪声（<200Hz），提升语音信噪比
    // 2) norm -3: 音量归一化到 -3dB
    QString normTmp = m_audioTmp + ".norm.wav";
    QProcess sox;
    sox.start("sox", QStringList() << m_audioTmp << normTmp
        << "highpass" << "200" << "norm" << "-3");
    sox.waitForFinished(5000);
    if (QFileInfo::exists(normTmp)) {
        QFile::remove(m_audioTmp);
        QFile::rename(normTmp, m_audioTmp);
        return true;
    }
    return false;
}

/* ---- Whisper STT ---- */

QString VoiceAssistant::runWhisper()
{
    setLastError(QString());
    QProcess proc;
    proc.setWorkingDirectory(m_whisperDir);
    proc.start("taskset",
        QStringList()
            << "f0"
            << "timeout"
            << "20s"
            << (m_whisperDir + "/rknn_whisper_demo")
            << "./model/whisper_encoder_base_20s.rknn"
            << "./model/whisper_decoder_base_20s.rknn"
            << "zh"
            << m_audioTmp);

    if (!proc.waitForFinished(25000)) {
        proc.kill();
        proc.waitForFinished(2000);
        setLastError("Whisper 进程超时/卡住");
        return QString();
    }

    if (proc.exitCode() == 124 || proc.exitCode() == 137 || proc.exitCode() == 143) {
        setLastError(QString("Whisper 超时退出(code=%1)").arg(proc.exitCode()));
        return QString();
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput())
                   + QString::fromUtf8(proc.readAllStandardError());

    // 逐行提取 "Whisper output:"，避免把 RTF 行误当成识别文本
    const QStringList lines = output.split('\n');
    QString text;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (!line.contains("Whisper output:")) {
            continue;
        }

        QString candidate = line.section("Whisper output:", 1).trimmed();
        if (candidate.isEmpty() && i + 1 < lines.size()) {
            candidate = lines.at(i + 1).trimmed();
        }

        if (!candidate.isEmpty() &&
            !candidate.contains("Real Time Factor", Qt::CaseInsensitive) &&
            !candidate.contains("RTF", Qt::CaseInsensitive)) {
            text = candidate;
            break;
        }
    }

    if (!text.isEmpty()) {
        setLastError(QString());
        return text;
    }
    setLastError("Whisper 未输出有效文本");
    return QString();
}

/* ---- LLM 进程管理 (方案B: 常驻进程) ---- */

// 调用者 必须 持有 m_llmMutex
bool VoiceAssistant::ensureLlmRunning()
{
    if (m_llmProc && m_llmProc->state() == QProcess::Running && m_llmReady)
        return true;

    // 清理残余进程
    if (m_llmProc) {
        m_llmProc->kill();
        m_llmProc->waitForFinished(2000);
        delete m_llmProc;
        m_llmProc = nullptr;
        m_llmReady = false;
    }

    m_llmProc = new QProcess;
    m_llmProc->setProcessChannelMode(QProcess::MergedChannels);
    m_llmProc->setWorkingDirectory(m_llmDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LD_LIBRARY_PATH", m_llmDir + "/lib");
    m_llmProc->setProcessEnvironment(env);

    // taskset f0 → 绑定大核; 不再用 timeout 包裹，常驻进程由我们管理生命周期
    m_llmProc->start("taskset", QStringList()
        << "f0" << m_llmBin << m_llmModel << "256" << "512");

    if (!m_llmProc->waitForStarted(5000)) {
        setLastError(QString("LLM 启动失败: %1").arg(m_llmProc->errorString()));
        delete m_llmProc;
        m_llmProc = nullptr;
        return false;
    }

    // 等待模型加载完成 + 首个 "user:" 提示符（表示已就绪）
    QByteArray buf;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 30000) {
        if (m_llmProc->state() != QProcess::Running) {
            setLastError("LLM 进程加载模型时意外退出");
            delete m_llmProc;
            m_llmProc = nullptr;
            return false;
        }
        if (m_llmProc->waitForReadyRead(500)) {
            buf += m_llmProc->readAll();
            if (buf.contains("user:")) {
                m_llmReady = true;
                qDebug() << "[VoiceAssistant] LLM 常驻进程就绪";
                return true;
            }
        }
    }

    setLastError("LLM 模型加载超时(30s)");
    m_llmProc->kill();
    m_llmProc->waitForFinished(2000);
    delete m_llmProc;
    m_llmProc = nullptr;
    return false;
}

void VoiceAssistant::shutdownLlm()
{
    QMutexLocker lock(&m_llmMutex);
    if (!m_llmProc) return;

    if (m_llmProc->state() == QProcess::Running) {
        m_llmProc->write("exit\n");
        m_llmProc->waitForBytesWritten(1000);
        if (!m_llmProc->waitForFinished(5000)) {
            m_llmProc->kill();
            m_llmProc->waitForFinished(2000);
        }
    }
    delete m_llmProc;
    m_llmProc = nullptr;
    m_llmReady = false;
    qDebug() << "[VoiceAssistant] LLM 常驻进程已关闭";
}

/* ---- LLM 推理 ---- */

QString VoiceAssistant::runLlm(const QString &inputText)
{
    setLastError(QString());
    // 必须单行 prompt！llm_demo 按行分割输入，每行当独立对话轮。
    // 多行会导致 JSON 格式要求和实际指令分到不同轮，LLM 不出 JSON。
    QString prompt = QString(
        "你是车载语音助手,根据用户语音指令输出JSON,只输出一行JSON不要其他内容。"
        "格式:{\"action\":\"xxx\",\"param\":\"xxx\"}。"
        "支持的action:open_music,close_music,play_music,next_music,prev_music,pause_music,"
        "open_weather,open_map,open_camera,close_camera,open_radar,open_monitor,"
        "open_settings,unknown。"
        "用户说:%1"
    ).arg(inputText);

    QMutexLocker lock(&m_llmMutex);

    // 确保常驻进程在运行
    if (!ensureLlmRunning()) {
        setLastError("LLM 进程未就绪: " + m_lastError);
        return "{\"action\":\"unknown\",\"param\":\"\"}";
    }

    // 清空上一轮残留的输出
    m_llmProc->readAll();

    // 写入 prompt（单行，不带 exit）
    m_llmProc->write((prompt + "\n").toUtf8());
    if (!m_llmProc->waitForBytesWritten(3000)) {
        setLastError("LLM 写入 stdin 失败");
        m_llmProc->kill();
        m_llmProc->waitForFinished(2000);
        delete m_llmProc;
        m_llmProc = nullptr;
        m_llmReady = false;
        return "{\"action\":\"unknown\",\"param\":\"\"}";
    }

    // 等待推理完成: llm_demo 输出 "robot: ..." 后会打印 "user:" 作为下一轮提示
    QByteArray accumulated;
    QElapsedTimer timer;
    timer.start();
    bool gotResponse = false;

    while (timer.elapsed() < 30000) {
        if (m_llmProc->state() != QProcess::Running) {
            setLastError("LLM 进程推理时意外退出");
            delete m_llmProc;
            m_llmProc = nullptr;
            m_llmReady = false;
            return "{\"action\":\"unknown\",\"param\":\"\"}";
        }
        if (m_llmProc->waitForReadyRead(500)) {
            accumulated += m_llmProc->readAll();
            // "user:" 出现标志本轮推理完成
            if (accumulated.contains("user:")) {
                gotResponse = true;
                break;
            }
        }
    }

    if (!gotResponse) {
        setLastError("LLM 推理超时(30s)");
        m_llmProc->kill();
        m_llmProc->waitForFinished(2000);
        delete m_llmProc;
        m_llmProc = nullptr;
        m_llmReady = false;
        return "{\"action\":\"unknown\",\"param\":\"\"}";
    }

    QString output = QString::fromUtf8(accumulated);
    qDebug() << "[VoiceAssistant] LLM 原始输出:" << output.left(500);

    // 优先解析 robot: 行，避免误命中提示词里的 JSON 模板
    QRegularExpression robotRe("robot:\\s*(\\{[^\\n]*\\})");
    auto robotMatch = robotRe.match(output);
    if (robotMatch.hasMatch()) {
        QString robotJson = robotMatch.captured(1).trimmed();

        // 容错: 某些输出可能多一个 '}'，尝试裁剪到可解析
        while (!robotJson.isEmpty()) {
            QJsonDocument d = QJsonDocument::fromJson(robotJson.toUtf8());
            if (d.isObject()) {
                return robotJson;
            }
            if (robotJson.endsWith('}')) {
                robotJson.chop(1);
                continue;
            }
            break;
        }
    }

    // 提取所有 JSON，对 action 有效的对象优先，避免误命中提示词模板 {"action":"xxx"...}
    QString bestJson;
    QRegularExpression re("\\{[^{}]*\\}");
    auto it = re.globalMatch(output);
    while (it.hasNext()) {
        const QString candidate = it.next().captured(0).trimmed();
        QJsonDocument d = QJsonDocument::fromJson(candidate.toUtf8());
        if (!d.isObject()) continue;

        const QString action = d.object().value("action").toString().trimmed().toLower();
        if (action.isEmpty()) continue;
        bestJson = candidate;
        if (action != "xxx") {
            if (action == "unknow") {
                bestJson = "{\"action\":\"unknown\",\"param\":\"\"}";
            }
        }
    }
    if (!bestJson.isEmpty()) {
        return bestJson;
    }

    // fallback: 尝试从 robot: 行提取
    QRegularExpression re2("robot:\\s*(.+)");
    auto match2 = re2.match(output);
    if (match2.hasMatch()) {
        const QString robotLine = match2.captured(1).trimmed();
        QJsonDocument d = QJsonDocument::fromJson(robotLine.toUtf8());
        if (d.isObject()) {
            return robotLine;
        }
    }

    setLastError("LLM 输出中未解析到 JSON");
    return "{\"action\":\"unknown\",\"param\":\"\"}";
}
