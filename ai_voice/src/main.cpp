#include "local_asr.h"
#include "llm_engine.h"
#include "tts_engine.h"
#include <iostream>
#include <string>

// AI 语音交互链路测试程序
// 测试完整流程：ASR → LLM 推理 → 意图解析 → TTS 播报
int main(int argc, char *argv[])
{
    std::cout << "======================================" << std::endl;
    std::cout << "  AI Voice Pipeline Test (stub 模式)  " << std::endl;
    std::cout << "======================================" << std::endl;

    // --- 1. 初始化各模块 ---
    LocalASR asr;
    LlmEngine llm;
    TtsEngine tts;

    // 模型路径（实际使用时替换为真实路径）
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

    std::cout << std::endl;

    // --- 2. 模拟语音输入（实际使用时替换为真实 WAV 文件路径）---
    const std::string wav_file = (argc > 1) ? argv[1] : "test_audio.wav";

    // ASR：语音 → 文字
    std::string asr_text = asr.transcribe(wav_file);
    if (asr_text.empty()) {
        std::cerr << "[main] ASR 识别结果为空" << std::endl;
        return 1;
    }

    std::cout << "[main] ASR 识别文本: " << asr_text << std::endl;

    // --- 3. LLM 推理 ---
    std::string llm_response = llm.chat(asr_text);

    // --- 4. 意图解析 ---
    std::string intent = llm.parseIntent(llm_response);
    std::cout << "[main] 最终意图: " << intent << std::endl;

    // --- 5. 根据意图执行操作（此处仅打印，Qt 集成时发送信号）---
    if (intent == "OPEN_MUSIC") {
        std::cout << "[main] 执行：打开音乐播放器" << std::endl;
    } else if (intent == "OPEN_MAP") {
        std::cout << "[main] 执行：打开地图导航" << std::endl;
    } else if (intent == "OPEN_WEATHER") {
        std::cout << "[main] 执行：打开天气预报" << std::endl;
    } else if (intent == "OPEN_MONITOR") {
        std::cout << "[main] 执行：打开倒车监控" << std::endl;
    } else if (intent == "READ_TEMP") {
        std::cout << "[main] 执行：读取温湿度传感器" << std::endl;
    } else {
        std::cout << "[main] 未知意图，无操作" << std::endl;
    }

    // --- 6. TTS 播报回复 ---
    // 使用 parseIntent 已提取意图；去掉 [INTENT:XXX] 标记再播报
    auto tag_pos = llm_response.find("[INTENT:");
    std::string speak_text = (tag_pos != std::string::npos)
                             ? llm_response.substr(0, tag_pos)
                             : llm_response;
    tts.speak(speak_text);

    std::cout << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "  测试完成" << std::endl;
    std::cout << "======================================" << std::endl;
    return 0;
}
