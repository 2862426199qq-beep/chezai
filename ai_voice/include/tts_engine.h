#pragma once

#include "i_tts_engine.h"
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// TtsEngine: 基于 espeak-ng 的离线 TTS（ITtsEngine 插件实现）
// 内置双缓冲句子队列，支持流式 LLM 边生成边播报。
// Double-buffer sentence queue for low-latency streaming TTS.
class TtsEngine : public ITtsEngine {
public:
    TtsEngine();
    ~TtsEngine() override;

    // 同步播报（阻塞直到播报完成）
    void speak(const std::string &text) override;

    // 异步播报（非阻塞，启动后台线程）
    void speakAsync(const std::string &text) override;

    // 将一句话压入双缓冲队列（流式 LLM 使用）
    // 工作线程从队列中取出后依次调用 speak()
    void enqueue(const std::string &sentence) override;

    // 等待队列中所有句子播报完毕（VoicePipeline 在 runStream 末尾调用）
    void waitDone() override;

private:
    // 双缓冲队列的工作线程入口
    void workerLoop();

    std::queue<std::string>   m_queue;      // 待播放句子队列
    std::mutex                m_mtx;
    std::condition_variable   m_cv;
    std::condition_variable   m_cv_done;    // 用于 waitDone()
    std::thread               m_worker;
    std::atomic<bool>         m_running{false};
};
