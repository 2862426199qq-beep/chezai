#include "llm_engine.h"
#include <iostream>
#include <sstream>

// -------------------------------------------------------
// 接入 llama.cpp 真实模型时的步骤：
//   1. 编译 llama.cpp，得到 libllama.a
//   2. 在 CMakeLists.txt 中启用 USE_REAL_MODELS 并设置 LLAMA_DIR
//   3. 取消下方 #ifdef USE_REAL_MODELS 块内的代码注释
// API 参考：https://github.com/ggerganov/llama.cpp/blob/master/include/llama.h
// -------------------------------------------------------
#ifdef USE_REAL_MODELS
#include "llama.h"
#endif

LlmEngine::~LlmEngine()
{
    if (m_initialized) {
#ifdef USE_REAL_MODELS
        llama_free(static_cast<llama_context *>(m_ctx));
        llama_free_model(static_cast<llama_model *>(m_model));
        llama_backend_free();
#endif
        m_ctx   = nullptr;
        m_model = nullptr;
        m_initialized = false;
    }
}

bool LlmEngine::init(const std::string &model_path, int n_threads)
{
    std::cout << "[LlmEngine] 加载模型: " << model_path
              << "  线程数: " << n_threads << std::endl;

    // 车载智能助手 system prompt（DeepSeek R1 格式）
    m_system_prompt =
        "你是运行在 RK3588S 车载系统上的智能助手，名字叫「智行」。"
        "请用简洁中文回答用户的请求。"
        "每次回复末尾必须附上操作意图，格式：[INTENT:XXX]。"
        "可选意图：OPEN_MUSIC、OPEN_MAP、OPEN_WEATHER、OPEN_MONITOR、READ_TEMP、UNKNOWN。";

#ifdef USE_REAL_MODELS
    // --- 真实 llama.cpp 初始化 ---
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;  // CPU 推理；RKNN NPU 加速时改为 ggml_backend_rknn
    m_model = llama_load_model_from_file(model_path.c_str(), mparams);
    if (!m_model) {
        std::cerr << "[LlmEngine] 模型加载失败: " << model_path << std::endl;
        return false;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx     = 2048;
    cparams.n_threads = static_cast<uint32_t>(n_threads);
    m_ctx = llama_new_context_with_model(
                static_cast<llama_model *>(m_model), cparams);
    if (!m_ctx) {
        std::cerr << "[LlmEngine] 创建推理上下文失败" << std::endl;
        return false;
    }
#else
    (void)model_path; (void)n_threads;
#endif

    m_initialized = true;
    std::cout << "[LlmEngine] 初始化完成" << std::endl;
    return true;
}

std::string LlmEngine::buildPrompt(const std::string &user_input) const
{
    // DeepSeek / Qwen Chat 格式
    return "<|system|>\n" + m_system_prompt + "\n"
         + "<|user|>\n"   + user_input      + "\n"
         + "<|assistant|>\n";
}

std::string LlmEngine::chat(const std::string &user_input)
{
    if (!m_initialized) {
        std::cerr << "[LlmEngine] 未初始化，请先调用 init()" << std::endl;
        return "";
    }

    std::cout << "[LlmEngine] 用户输入: " << user_input << std::endl;

    std::string full_response;

#ifdef USE_REAL_MODELS
    // 利用 chatStream 收集全文
    chatStream(user_input, [&full_response](const std::string &token) -> bool {
        full_response += token;
        return true;
    });
#else
    // --- Stub: 关键词匹配 ---
    if (user_input.find("音乐") != std::string::npos ||
        user_input.find("歌") != std::string::npos) {
        full_response = "好的，正在为您打开音乐。[INTENT:OPEN_MUSIC]";
    } else if (user_input.find("地图") != std::string::npos ||
               user_input.find("导航") != std::string::npos) {
        full_response = "好的，正在为您打开地图导航。[INTENT:OPEN_MAP]";
    } else if (user_input.find("天气") != std::string::npos) {
        full_response = "好的，正在查询当前天气。[INTENT:OPEN_WEATHER]";
    } else if (user_input.find("温度") != std::string::npos ||
               user_input.find("温湿度") != std::string::npos) {
        full_response = "当前车内温湿度数据已读取。[INTENT:READ_TEMP]";
    } else if (user_input.find("倒车") != std::string::npos ||
               user_input.find("摄像") != std::string::npos) {
        full_response = "好的，正在打开倒车监控摄像头。[INTENT:OPEN_MONITOR]";
    } else {
        full_response = "抱歉，我暂时无法理解您的指令。[INTENT:UNKNOWN]";
    }
#endif

    std::cout << "[LlmEngine] 模型回复: " << full_response << std::endl;
    return full_response;
}

bool LlmEngine::chatStream(const std::string &user_input,
                            TokenCallback callback)
{
    if (!m_initialized) {
        std::cerr << "[LlmEngine] 未初始化，请先调用 init()" << std::endl;
        return false;
    }

#ifdef USE_REAL_MODELS
    // --- 真实 llama.cpp 流式推理 ---
    std::string prompt = buildPrompt(user_input);

    auto *model = static_cast<llama_model *>(m_model);
    auto *ctx   = static_cast<llama_context *>(m_ctx);

    // Tokenize prompt
    const int n_prompt_tokens = -llama_tokenize(
        model, prompt.c_str(), static_cast<int>(prompt.size()),
        nullptr, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt_tokens);
    if (llama_tokenize(model, prompt.c_str(), static_cast<int>(prompt.size()),
                       prompt_tokens.data(), n_prompt_tokens, true, true) < 0) {
        std::cerr << "[LlmEngine] tokenize 失败" << std::endl;
        return false;
    }

    // 推理循环（逐 token 解码）
    llama_batch batch = llama_batch_init(512, 0, 1);
    for (int i = 0; i < (int)prompt_tokens.size(); ++i) {
        llama_batch_add(batch, prompt_tokens[i], i, {0}, false);
    }
    batch.logits[batch.n_tokens - 1] = true;

    if (llama_decode(ctx, batch) != 0) {
        llama_batch_free(batch);
        return false;
    }

    // 创建采样器（在循环外创建一次，避免每次 token 分配）
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler *sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    int n_cur = batch.n_tokens;
    while (true) {
        llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

        if (llama_token_is_eog(model, new_token)) {
            break;
        }

        char buf[128];
        int n = llama_token_to_piece(model, new_token, buf, sizeof(buf), 0, true);
        if (n > 0) {
            std::string piece(buf, n);
            if (!callback(piece)) {
                break;  // 调用方请求提前终止
            }
        }

        llama_batch_clear(batch);
        llama_batch_add(batch, new_token, n_cur++, {0}, true);
        if (llama_decode(ctx, batch) != 0) {
            break;
        }
    }

    llama_sampler_free(sampler);
    llama_batch_free(batch);
    return true;
#else
    // Stub: 退化为 chat() 后整体回调
    std::string response = chat(user_input);
    if (!response.empty()) {
        callback(response);
    }
    return !response.empty();
#endif
}

std::string LlmEngine::parseIntent(const std::string &response)
{
    const std::string tag_start = "[INTENT:";
    const std::string tag_end   = "]";

    auto pos = response.find(tag_start);
    if (pos == std::string::npos) {
        return "UNKNOWN";
    }

    auto intent_start = pos + tag_start.size();
    auto end_pos      = response.find(tag_end, intent_start);
    if (end_pos == std::string::npos) {
        return "UNKNOWN";
    }

    std::string intent = response.substr(intent_start, end_pos - intent_start);
    std::cout << "[LlmEngine] 意图: " << intent << std::endl;
    return intent;
}
