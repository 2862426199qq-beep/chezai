#pragma once

#include "i_asr_engine.h"
#include <string>

// LocalASR: 基于 whisper.cpp 的离线语音识别（IAsrEngine 插件实现）
// Offline Automatic Speech Recognition based on whisper.cpp
class LocalASR : public IAsrEngine {
public:
    LocalASR() = default;
    ~LocalASR() override;

    // 初始化：加载 whisper.cpp GGML 模型
    // model_path: 如 "models/ggml-small-q5_1.bin"
    bool init(const std::string &model_path) override;

    // 批量转写：WAV 文件 → 文字（16kHz 单声道）
    std::string transcribe(const std::string &wav_path) override;

    // 流式转写：逐段回调（whisper.cpp 支持 VAD 分段）
    bool transcribeStream(const std::string &wav_path,
                          StreamCallback callback) override;

private:
    // whisper_context*（void* 避免对 whisper.h 的编译期依赖）
    void *m_ctx = nullptr;
    bool  m_initialized = false;
};
