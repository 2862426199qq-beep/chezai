#pragma once

#include <string>

// TtsEngine: 基于 espeak-ng 的离线文本转语音
// Offline Text-to-Speech engine based on espeak-ng
class TtsEngine {
public:
    TtsEngine() = default;
    ~TtsEngine() = default;

    // 同步语音播报（阻塞直到播报完成）
    void speak(const std::string &text);

    // 异步语音播报（非阻塞，后台线程播报）
    void speakAsync(const std::string &text);
};
