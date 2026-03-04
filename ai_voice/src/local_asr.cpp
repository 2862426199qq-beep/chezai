#include "local_asr.h"
#include <iostream>

// TODO: 接入 whisper.cpp 时，取消注释以下 include
// #include "whisper.h"

LocalASR::~LocalASR()
{
    if (m_initialized) {
        // TODO: whisper_free((whisper_context *)m_ctx);
        m_ctx = nullptr;
        m_initialized = false;
    }
}

bool LocalASR::init(const std::string &model_path)
{
    std::cout << "[LocalASR] 初始化模型: " << model_path << std::endl;

    // TODO: 接入 whisper.cpp
    // whisper_context_params cparams = whisper_context_default_params();
    // m_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    // if (!m_ctx) {
    //     std::cerr << "[LocalASR] 加载模型失败: " << model_path << std::endl;
    //     return false;
    // }

    // --- Stub: 暂时模拟初始化成功 ---
    m_initialized = true;
    std::cout << "[LocalASR] 模型初始化完成（stub 模式）" << std::endl;
    return true;
}

std::string LocalASR::transcribe(const std::string &wav_path)
{
    if (!m_initialized) {
        std::cerr << "[LocalASR] 错误：未初始化，请先调用 init()" << std::endl;
        return "";
    }

    std::cout << "[LocalASR] 识别音频: " << wav_path << std::endl;

    // TODO: 接入 whisper.cpp
    // whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    // wparams.language   = "zh";
    // wparams.n_threads  = 4;
    // wparams.translate  = false;
    //
    // // 读取 WAV 文件数据（16kHz, mono, float32）
    // std::vector<float> pcm_data = load_wav_file(wav_path);
    // if (whisper_full((whisper_context *)m_ctx, wparams,
    //                  pcm_data.data(), (int)pcm_data.size()) != 0) {
    //     std::cerr << "[LocalASR] 推理失败" << std::endl;
    //     return "";
    // }
    //
    // std::string result;
    // int n_segments = whisper_full_n_segments((whisper_context *)m_ctx);
    // for (int i = 0; i < n_segments; ++i) {
    //     result += whisper_full_get_segment_text((whisper_context *)m_ctx, i);
    // }
    // return result;

    // --- Stub: 返回模拟识别结果 ---
    std::string stub_result = "打开音乐";
    std::cout << "[LocalASR] 识别结果（stub）: " << stub_result << std::endl;
    return stub_result;
}
