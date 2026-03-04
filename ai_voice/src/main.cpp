#include "local_asr.h"
#include "llm_engine.h"
#include "tts_engine.h"
#include "voice_pipeline.h"
#include <iostream>
#include <string>

// AI 语音交互全链路测试程序
// 测试完整流程：ASR → LLM chatStream → 句子切分 → TTS 双缓冲队列
int main(int argc, char *argv[])
{
    std::cout << "======================================" << std::endl;
    std::cout << "  AI Voice Pipeline Test" << std::endl;
#ifdef USE_REAL_MODELS
    std::cout << "  模式：真实模型" << std::endl;
#else
    std::cout << "  模式：Stub（无需真实模型）" << std::endl;
#endif
    std::cout << "======================================" << std::endl;

    // --- 1. 初始化各引擎 ---
    LocalASR  asr;
    LlmEngine llm;
    TtsEngine tts;

    const std::string asr_model = "models/ggml-small-q5_1.bin";
    const std::string llm_model = "models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf";

    if (!asr.init(asr_model)) {
        std::cerr << "[main] ASR 初始化失败" << std::endl;
        return 1;
    }
    if (!llm.init(llm_model, /*n_threads=*/4)) {
        std::cerr << "[main] LLM 初始化失败" << std::endl;
        return 1;
    }

    // --- 2. 创建 VoicePipeline ---
    VoicePipeline pipeline(asr, llm, tts);

    // 注册意图回调（Qt 集成时改为 emit 信号）
    pipeline.setIntentHandler([](const std::string &intent,
                                 const std::string & /*reply*/) {
        std::cout << "[main] === 意图回调 ===> " << intent << std::endl;
        if      (intent == "OPEN_MUSIC")   { std::cout << "[main] 执行：打开音乐" << std::endl; }
        else if (intent == "OPEN_MAP")     { std::cout << "[main] 执行：打开地图" << std::endl; }
        else if (intent == "OPEN_WEATHER") { std::cout << "[main] 执行：查询天气" << std::endl; }
        else if (intent == "OPEN_MONITOR") { std::cout << "[main] 执行：倒车监控" << std::endl; }
        else if (intent == "READ_TEMP")    { std::cout << "[main] 执行：读取温湿度" << std::endl; }
    });

    // --- 3. 运行流水线 ---
    const std::string wav_file = (argc > 1) ? argv[1] : "test_audio.wav";

    // 默认使用流式模式（低延迟）
    std::string intent = pipeline.runStream(wav_file);

    std::cout << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "  完成，最终意图: " << intent << std::endl;
    std::cout << "======================================" << std::endl;
    return 0;
}
