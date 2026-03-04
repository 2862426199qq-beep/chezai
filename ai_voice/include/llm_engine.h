#pragma once

#include "i_llm_engine.h"
#include <string>

// LlmEngine: 基于 llama.cpp 的 DeepSeek 端侧推理引擎（ILlmEngine 插件实现）
// On-device LLM inference engine based on llama.cpp (DeepSeek-R1-1.5B)
class LlmEngine : public ILlmEngine {
public:
    LlmEngine() = default;
    ~LlmEngine() override;

    // 初始化：加载 GGUF 模型
    // model_path: 如 "models/DeepSeek-R1-1.5B-Q4_K_M.gguf"
    // n_threads:  推理线程数（RK3588S 大核为 4）
    bool init(const std::string &model_path, int n_threads = 4) override;

    // 批量对话推理（阻塞直到生成完毕）
    std::string chat(const std::string &user_input) override;

    // 流式对话推理（token 逐个回调，适合边生成边播报）
    // callback 返回 false 可提前终止生成
    bool chatStream(const std::string &user_input,
                    TokenCallback callback) override;

    // 从 LLM 回复中解析操作意图
    std::string parseIntent(const std::string &response) override;

private:
    void *m_model       = nullptr;  // llama_model*
    void *m_ctx         = nullptr;  // llama_context*
    bool  m_initialized = false;

    std::string m_system_prompt;

    // 构造完整 prompt（system + user）
    std::string buildPrompt(const std::string &user_input) const;
};
