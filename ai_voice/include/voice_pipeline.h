#pragma once

#include "i_asr_engine.h"
#include "i_llm_engine.h"
#include "i_tts_engine.h"
#include <string>
#include <functional>

// VoicePipeline: AI 语音交互全链路编排器
//
// 流水线流程（流式模式）：
//   录音/WAV → ASR(transcribeStream) → LLM(chatStream)
//         → 句子切分 → TTS.enqueue() → 双缓冲播报
//
// 同时支持批量模式：
//   WAV → ASR.transcribe() → LLM.chat() → parseIntent() → TTS.speak()
class VoicePipeline {
public:
    // intent_handler: 当意图解析完成时的回调
    // 参数1: 意图字符串（"OPEN_MUSIC" 等）
    // 参数2: LLM 完整回复文本
    using IntentHandler = std::function<void(const std::string &intent,
                                             const std::string &full_reply)>;

    VoicePipeline(IAsrEngine &asr, ILlmEngine &llm, ITtsEngine &tts);

    // 设置意图回调（可选，不设则只做 TTS 播报）
    void setIntentHandler(IntentHandler handler);

    // 批量模式：WAV → ASR → LLM → Intent → TTS（阻塞直到播报完成）
    // 返回解析到的意图字符串
    std::string run(const std::string &wav_path);

    // 流式模式：ASR 流式识别 → LLM 流式生成 → TTS 双缓冲队列
    // 低延迟：LLM 每生成一句就立即入队 TTS，无需等待全文生成
    // 返回解析到的意图字符串
    std::string runStream(const std::string &wav_path);

private:
    IAsrEngine &m_asr;
    ILlmEngine &m_llm;
    ITtsEngine &m_tts;
    IntentHandler m_intent_handler;
};
