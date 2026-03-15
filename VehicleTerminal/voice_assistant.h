#ifndef VOICE_ASSISTANT_H
#define VOICE_ASSISTANT_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QMutex>

/**
 * VoiceAssistant — 语音AI管线 Qt 封装
 *
 * 流程: parecord → sox归一化 → Whisper STT → Qwen2.5 LLM → JSON
 * 所有外部命令通过 QProcess 调用，后台线程执行。
 */
class VoiceAssistant : public QObject
{
    Q_OBJECT
public:
    explicit VoiceAssistant(QObject *parent = nullptr);
    ~VoiceAssistant();

    void start(int recordSeconds = 5);
    void stopRecording();
    bool isRunning() const { return m_running; }
    void preInitAudio();   // 启动时调用，预热音频通路
    void preInitLlm();     // 启动时调用，后台预加载 LLM 模型

signals:
    void statusChanged(const QString &status);
    void sttResult(const QString &text);
    void actionResult(const QString &action, const QString &param);
    void finished();
    void errorOccurred(const QString &msg);

private:
    void runPipeline(int recordSeconds);
    bool initAudio();
    bool recordAudio(int seconds);
    bool normalizeAudio();
    QString runWhisper();
    QString runLlm(const QString &inputText);
    bool ensureLlmRunning();  // 确保 LLM 常驻进程存活（内部不加锁，调用方需持有 m_llmMutex）
    void shutdownLlm();       // 发 exit 并清理 LLM 进程
    void setLastError(const QString &msg) { m_lastError = msg; }

    bool m_running = false;
    bool m_stopRequested = false;
    bool m_audioInited = false;
    QProcess *m_recordProc = nullptr;
    QProcess *m_llmProc = nullptr;    // LLM 常驻子进程
    QMutex m_llmMutex;               // 保护 m_llmProc 的并发访问
    bool m_llmReady = false;         // LLM 模型已加载并就绪
    QString m_lastError;

    // 路径配置
    QString m_whisperDir;
    QString m_llmDir;
    QString m_llmBin;
    QString m_llmModel;
    QString m_audioTmp;
    QString m_audioInitHp;
    QString m_audioInitSpk;
};

#endif // VOICE_ASSISTANT_H
