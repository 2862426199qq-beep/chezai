#include "voice_pipeline.h"
#include <iostream>
#include <string>

// 中文句末标点（UTF-8），检测时按实际字节长度比较
struct Delim { const char *seq; size_t len; };
static const Delim SENTENCE_DELIMS[] = {
    {"。", 3}, {"！", 3}, {"？", 3}, {"；", 3},
    {"\n",  1}, {".",   1}, {"!",   1}, {"?",   1},
    {nullptr, 0}
};

static bool isSentenceEnd(const std::string &accum)
{
    for (int i = 0; SENTENCE_DELIMS[i].seq; ++i) {
        size_t len = SENTENCE_DELIMS[i].len;
        if (accum.size() >= len &&
            accum.compare(accum.size() - len, len, SENTENCE_DELIMS[i].seq) == 0) {
            return true;
        }
    }
    return false;
}

VoicePipeline::VoicePipeline(IAsrEngine &asr, ILlmEngine &llm, ITtsEngine &tts)
    : m_asr(asr), m_llm(llm), m_tts(tts)
{
}

void VoicePipeline::setIntentHandler(IntentHandler handler)
{
    m_intent_handler = std::move(handler);
}

// 批量模式：WAV → ASR → LLM → Intent → TTS
std::string VoicePipeline::run(const std::string &wav_path)
{
    std::cout << "[Pipeline] 批量模式启动" << std::endl;

    // 1. ASR
    std::string asr_text = m_asr.transcribe(wav_path);
    if (asr_text.empty()) {
        std::cerr << "[Pipeline] ASR 识别结果为空" << std::endl;
        return "UNKNOWN";
    }
    std::cout << "[Pipeline] ASR: " << asr_text << std::endl;

    // 2. LLM
    std::string reply = m_llm.chat(asr_text);
    if (reply.empty()) {
        return "UNKNOWN";
    }

    // 3. 解析意图
    std::string intent = m_llm.parseIntent(reply);

    // 4. 意图回调
    if (m_intent_handler) {
        m_intent_handler(intent, reply);
    }

    // 5. TTS 播报（去掉 [INTENT:XXX] 标记）
    auto tag_pos = reply.find("[INTENT:");
    std::string speak_text = (tag_pos != std::string::npos)
                             ? reply.substr(0, tag_pos)
                             : reply;
    m_tts.speak(speak_text);

    return intent;
}

// 流式模式：ASR → LLM chatStream → 句子切分 → TTS.enqueue()
// 低延迟：每完成一句就立刻入队 TTS，无需等待 LLM 全文生成
std::string VoicePipeline::runStream(const std::string &wav_path)
{
    std::cout << "[Pipeline] 流式模式启动" << std::endl;

    // 1. ASR（流式识别，完整 utterance 后再送 LLM）
    std::string asr_text;
    m_asr.transcribeStream(wav_path, [&asr_text](const std::string &seg) {
        asr_text += seg;
        std::cout << "[Pipeline] ASR 段: " << seg << std::endl;
    });

    if (asr_text.empty()) {
        std::cerr << "[Pipeline] ASR 识别结果为空" << std::endl;
        return "UNKNOWN";
    }

    // 2. LLM 流式生成 → 句子切分 → TTS 队列
    std::string accum;       // 未成句的累积片段
    std::string full_reply;  // 完整回复（用于意图解析）

    m_llm.chatStream(asr_text, [&](const std::string &token) -> bool {
        accum      += token;
        full_reply += token;

        // 检测句末标点，凑够一句就入 TTS 队列
        if (isSentenceEnd(accum)) {
            // 去掉 [INTENT:...] 标记后入队
            auto tag = accum.find("[INTENT:");
            std::string to_speak = (tag != std::string::npos)
                                   ? accum.substr(0, tag)
                                   : accum;
            if (!to_speak.empty()) {
                m_tts.enqueue(to_speak);
                std::cout << "[Pipeline] TTS 入队: " << to_speak << std::endl;
            }
            accum.clear();
        }
        return true;  // 继续生成
    });

    // 剩余片段（最后一句可能没有句末标点）
    if (!accum.empty()) {
        auto tag = accum.find("[INTENT:");
        std::string to_speak = (tag != std::string::npos)
                               ? accum.substr(0, tag)
                               : accum;
        if (!to_speak.empty()) {
            m_tts.enqueue(to_speak);
        }
    }

    // 3. 等待 TTS 队列播完
    m_tts.waitDone();

    // 4. 解析意图并回调
    std::string intent = m_llm.parseIntent(full_reply);
    if (m_intent_handler) {
        m_intent_handler(intent, full_reply);
    }

    std::cout << "[Pipeline] 流式完成，意图: " << intent << std::endl;
    return intent;
}
