#pragma once

#include <string>

// LocalASR: 基于 whisper.cpp 的离线语音识别
// Offline Automatic Speech Recognition based on whisper.cpp
class LocalASR {
public:
    LocalASR() = default;
    ~LocalASR();

    // 初始化：加载 whisper.cpp 模型
    // model_path: GGML 模型文件路径，如 "models/ggml-small-q5_1.bin"
    bool init(const std::string &model_path);

    // 语音转文字
    // wav_path: 16kHz 单声道 WAV 文件路径
    // 返回识别文本，失败返回空字符串
    std::string transcribe(const std::string &wav_path);

private:
    // whisper.cpp context 指针（使用 void* 避免对头文件的强依赖）
    void *m_ctx = nullptr;      // whisper_context*
    bool  m_initialized = false;
};
