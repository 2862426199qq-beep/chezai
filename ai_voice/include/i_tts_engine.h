#pragma once

#include <string>

// ITtsEngine: TTS 模块抽象接口（插件化）
// All TTS backends must implement this interface.
class ITtsEngine {
public:
    virtual ~ITtsEngine() = default;

    // 同步播报（阻塞直到播完）
    virtual void speak(const std::string &text) = 0;

    // 异步播报（非阻塞）
    virtual void speakAsync(const std::string &text) = 0;

    // 入队一句话（供流式 LLM 使用，双缓冲队列消费）
    // 实现应在内部维护一个播报队列，逐句消费
    virtual void enqueue(const std::string &sentence) = 0;

    // 等待队列清空（等待所有已入队句子播报完毕）
    virtual void waitDone() = 0;
};
