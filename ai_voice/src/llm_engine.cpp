#include "llm_engine.h"
#include <iostream>
#include <algorithm>

// TODO: 接入 llama.cpp 时，取消注释以下 include
// #include "llama.h"

LlmEngine::~LlmEngine()
{
    if (m_initialized) {
        // TODO: llama_free((llama_context *)m_ctx);
        // TODO: llama_free_model((llama_model *)m_model);
        m_ctx   = nullptr;
        m_model = nullptr;
        m_initialized = false;
    }
}

bool LlmEngine::init(const std::string &model_path, int n_threads)
{
    std::cout << "[LlmEngine] 初始化模型: " << model_path
              << "，线程数: " << n_threads << std::endl;

    // 车载助手 system prompt
    m_system_prompt =
        "你是一个运行在 RK3588S 车载系统上的智能助手。"
        "你的名字是「智行」。"
        "请简洁地回答用户的问题，并在回复末尾输出操作意图，格式为 [INTENT:XXX]。"
        "支持的意图：OPEN_MUSIC（打开音乐）、OPEN_MAP（打开地图）、"
        "OPEN_WEATHER（查询天气）、OPEN_MONITOR（倒车监控）、"
        "READ_TEMP（读取温湿度）、UNKNOWN（无法识别）。";

    // TODO: 接入 llama.cpp
    // llama_backend_init();
    // llama_model_params mparams = llama_model_default_params();
    // m_model = llama_load_model_from_file(model_path.c_str(), mparams);
    // if (!m_model) {
    //     std::cerr << "[LlmEngine] 加载模型失败: " << model_path << std::endl;
    //     return false;
    // }
    // llama_context_params cparams = llama_context_default_params();
    // cparams.n_threads = n_threads;
    // m_ctx = llama_new_context_with_model((llama_model *)m_model, cparams);

    // --- Stub: 暂时模拟初始化成功 ---
    m_initialized = true;
    std::cout << "[LlmEngine] 模型初始化完成（stub 模式）" << std::endl;
    return true;
}

std::string LlmEngine::chat(const std::string &user_input)
{
    if (!m_initialized) {
        std::cerr << "[LlmEngine] 错误：未初始化，请先调用 init()" << std::endl;
        return "";
    }

    std::cout << "[LlmEngine] 用户输入: " << user_input << std::endl;

    // TODO: 接入 llama.cpp
    // std::string prompt = "<|system|>\n" + m_system_prompt + "\n"
    //                    + "<|user|>\n"   + user_input      + "\n"
    //                    + "<|assistant|>\n";
    // ... llama_tokenize / llama_decode 推理循环 ...

    // --- Stub: 基于关键词返回模拟回复 ---
    std::string response;
    if (user_input.find("音乐") != std::string::npos ||
        user_input.find("歌") != std::string::npos) {
        response = "好的，正在为您打开音乐。[INTENT:OPEN_MUSIC]";
    } else if (user_input.find("地图") != std::string::npos ||
               user_input.find("导航") != std::string::npos) {
        response = "好的，正在为您打开地图导航。[INTENT:OPEN_MAP]";
    } else if (user_input.find("天气") != std::string::npos) {
        response = "好的，正在查询当前天气。[INTENT:OPEN_WEATHER]";
    } else if (user_input.find("温度") != std::string::npos ||
               user_input.find("温湿度") != std::string::npos) {
        response = "当前车内温湿度数据已读取。[INTENT:READ_TEMP]";
    } else if (user_input.find("倒车") != std::string::npos ||
               user_input.find("摄像") != std::string::npos) {
        response = "好的，正在打开倒车监控摄像头。[INTENT:OPEN_MONITOR]";
    } else {
        response = "抱歉，我暂时无法理解您的指令。[INTENT:UNKNOWN]";
    }

    std::cout << "[LlmEngine] 模型回复: " << response << std::endl;
    return response;
}

std::string LlmEngine::parseIntent(const std::string &response)
{
    // 从回复中提取 [INTENT:XXX] 标记
    const std::string tag_start = "[INTENT:";
    const std::string tag_end   = "]";

    auto pos_start = response.find(tag_start);
    if (pos_start == std::string::npos) {
        return "UNKNOWN";
    }

    auto intent_start = pos_start + tag_start.size();
    auto pos_end      = response.find(tag_end, intent_start);
    if (pos_end == std::string::npos) {
        return "UNKNOWN";
    }

    std::string intent = response.substr(intent_start, pos_end - intent_start);
    std::cout << "[LlmEngine] 解析意图: " << intent << std::endl;
    return intent;
}
