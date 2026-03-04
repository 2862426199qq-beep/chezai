#include "local_asr.h"
#include <iostream>

// -------------------------------------------------------
// 接入 whisper.cpp 真实模型时的步骤：
//   1. 编译 whisper.cpp，得到 libwhisper.a 或 libwhisper.so
//   2. 在 CMakeLists.txt 中启用 USE_REAL_MODELS 并设置 WHISPER_DIR
//   3. 取消下方 #ifdef USE_REAL_MODELS 块内的代码注释
// API 参考：https://github.com/ggerganov/whisper.cpp/blob/master/whisper.h
// -------------------------------------------------------
#ifdef USE_REAL_MODELS
#include "whisper.h"

// 从 WAV 文件读取 PCM float32 数据（16kHz, mono）
// 这是 whisper.cpp 官方示例中的标准辅助函数
static bool read_wav(const std::string &path,
                     std::vector<float> &pcm, int &sample_rate)
{
    // TODO: 使用 drwav / libsndfile / 手写 WAV 解析读取 PCM
    // 参考 whisper.cpp/examples/common.cpp: read_wav()
    (void)path; (void)pcm; (void)sample_rate;
    return false;
}
#endif

LocalASR::~LocalASR()
{
    if (m_initialized) {
#ifdef USE_REAL_MODELS
        whisper_free(static_cast<whisper_context *>(m_ctx));
#endif
        m_ctx = nullptr;
        m_initialized = false;
    }
}

bool LocalASR::init(const std::string &model_path)
{
    std::cout << "[LocalASR] 加载模型: " << model_path << std::endl;

#ifdef USE_REAL_MODELS
    // --- 真实 whisper.cpp 初始化 ---
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // RK3588S 无 CUDA，可改为 RKNN 后端
    m_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!m_ctx) {
        std::cerr << "[LocalASR] 加载模型失败: " << model_path << std::endl;
        return false;
    }
#else
    // --- Stub 模式 ---
    (void)model_path;
#endif

    m_initialized = true;
    std::cout << "[LocalASR] 初始化完成" << std::endl;
    return true;
}

std::string LocalASR::transcribe(const std::string &wav_path)
{
    if (!m_initialized) {
        std::cerr << "[LocalASR] 未初始化，请先调用 init()" << std::endl;
        return "";
    }

    std::cout << "[LocalASR] 转写: " << wav_path << std::endl;

#ifdef USE_REAL_MODELS
    // --- 真实 whisper.cpp 推理 ---
    std::vector<float> pcm;
    int sample_rate = 0;
    if (!read_wav(wav_path, pcm, sample_rate)) {
        std::cerr << "[LocalASR] WAV 文件读取失败: " << wav_path << std::endl;
        return "";
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.language   = "zh";
    wparams.n_threads  = 4;
    wparams.translate  = false;
    wparams.print_progress = false;

    auto *ctx = static_cast<whisper_context *>(m_ctx);
    if (whisper_full(ctx, wparams, pcm.data(), static_cast<int>(pcm.size())) != 0) {
        std::cerr << "[LocalASR] whisper_full 推理失败" << std::endl;
        return "";
    }

    std::string result;
    int n_seg = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_seg; ++i) {
        result += whisper_full_get_segment_text(ctx, i);
    }
    std::cout << "[LocalASR] 识别结果: " << result << std::endl;
    return result;
#else
    // --- Stub: 返回模拟结果 ---
    const std::string stub = "打开音乐";
    std::cout << "[LocalASR] 识别结果（stub）: " << stub << std::endl;
    return stub;
#endif
}

bool LocalASR::transcribeStream(const std::string &wav_path,
                                 StreamCallback callback)
{
    if (!m_initialized) {
        std::cerr << "[LocalASR] 未初始化，请先调用 init()" << std::endl;
        return false;
    }

#ifdef USE_REAL_MODELS
    // --- 真实 whisper.cpp 流式推理（逐段回调）---
    // whisper.cpp 支持通过 new_segment_callback 实现逐段输出
    std::vector<float> pcm;
    int sample_rate = 0;
    if (!read_wav(wav_path, pcm, sample_rate)) {
        return false;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.language   = "zh";
    wparams.n_threads  = 4;
    wparams.translate  = false;
    wparams.print_progress = false;
    // 流式回调：每完成一段就触发
    wparams.new_segment_callback = [](whisper_context *ctx, whisper_state *,
                                      int n_new, void *user_data) {
        auto *cb = static_cast<StreamCallback *>(user_data);
        int total = whisper_full_n_segments(ctx);
        for (int i = total - n_new; i < total; ++i) {
            (*cb)(whisper_full_get_segment_text(ctx, i));
        }
    };
    wparams.new_segment_callback_user_data = &callback;

    auto *ctx = static_cast<whisper_context *>(m_ctx);
    return whisper_full(ctx, wparams, pcm.data(),
                        static_cast<int>(pcm.size())) == 0;
#else
    // Stub: 退化为批量转写后一次回调
    return IAsrEngine::transcribeStream(wav_path, callback);
#endif
}
