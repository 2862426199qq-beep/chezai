#pragma once

#include <string>

// LlmEngine: 基于 llama.cpp 的 DeepSeek 端侧推理引擎
// On-device LLM inference engine based on llama.cpp (DeepSeek-R1-1.5B)
class LlmEngine {
public:
    LlmEngine() = default;
    ~LlmEngine();

    // 初始化：加载 GGUF 模型
    // model_path: GGUF 模型文件路径，如 "models/DeepSeek-R1-1.5B-Q4_K_M.gguf"
    // n_threads:  推理线程数（RK3588S 大核为 4）
    bool init(const std::string &model_path, int n_threads = 4);

    // 对话推理：输入用户文本，返回模型回复
    // user_input: 用户语音识别文本
    std::string chat(const std::string &user_input);

    // 意图解析：从 LLM 回复中提取操作意图
    // 返回意图字符串，如 "OPEN_MUSIC"、"OPEN_MAP"、"OPEN_WEATHER"、"UNKNOWN"
    std::string parseIntent(const std::string &response);

private:
    // llama.cpp 模型/上下文指针（void* 避免对头文件的强依赖）
    void *m_model       = nullptr;  // llama_model*
    void *m_ctx         = nullptr;  // llama_context*
    bool  m_initialized = false;

    // 系统提示词（车载助手 system prompt）
    std::string m_system_prompt;
};
