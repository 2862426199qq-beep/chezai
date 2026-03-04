#pragma once

#include <string>
#include <functional>

// ILlmEngine: LLM 推理模块抽象接口（插件化）
// All LLM backends must implement this interface.
class ILlmEngine {
public:
    virtual ~ILlmEngine() = default;

    // 初始化模型
    // n_threads: 推理线程数（RK3588S 大核 4 个）
    virtual bool init(const std::string &model_path, int n_threads = 4) = 0;

    // 批量对话推理：返回完整回复
    virtual std::string chat(const std::string &user_input) = 0;

    // 流式对话推理：token 逐个通过 callback 输出
    // callback 返回 false 时提前终止生成
    using TokenCallback = std::function<bool(const std::string &token)>;
    virtual bool chatStream(const std::string &user_input,
                            TokenCallback callback) = 0;

    // 从 LLM 回复中解析操作意图
    // 返回如 "OPEN_MUSIC"、"OPEN_MAP"、"UNKNOWN" 等
    virtual std::string parseIntent(const std::string &response) = 0;
};
