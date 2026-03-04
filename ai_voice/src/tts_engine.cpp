#include "tts_engine.h"
#include <iostream>
#include <cstdlib>
#include <thread>

// 将文本中的危险 shell 字符过滤掉，只保留中文、字母、数字和常见标点
static std::string sanitizeForShell(const std::string &text)
{
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text) {
        // 保留多字节 UTF-8 字符（高位置 1），以及安全的 ASCII 字符
        if (c > 0x7F || std::isalnum(c) || c == ' ' || c == ',' ||
            c == '.' || c == '!' || c == '?' || c == '\'' ) {
            result += static_cast<char>(c);
        }
        // 丢弃 shell 元字符（$、`、"、\、; 等）
    }
    return result;
}

void TtsEngine::speak(const std::string &text)
{
    if (text.empty()) {
        return;
    }

    std::cout << "[TtsEngine] 语音播报: " << text << std::endl;

    // TODO: 后续改为 libespeak-ng API 调用（避免 fork + shell 开销）
    // 使用 execv 形式更安全，此处先用 sanitize 后的字符串
    std::string safe_text = sanitizeForShell(text);
    if (safe_text.empty()) {
        return;
    }

    // 使用 espeak-ng 命令行进行离线 TTS
    // -v zh:  中文语音  -s 150: 语速  -a 80: 音量
    std::string cmd = "espeak-ng -v zh -s 150 -a 80 -- \"" + safe_text + "\" 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[TtsEngine] espeak-ng 执行失败（是否已安装？apt install espeak-ng）"
                  << std::endl;
    }
}

void TtsEngine::speakAsync(const std::string &text)
{
    // 将 safe_text 按值捕获，确保即使 TtsEngine 对象销毁，线程仍能安全运行
    std::string safe_text = sanitizeForShell(text);
    if (safe_text.empty()) {
        return;
    }
    std::thread([safe_text]() {
        std::string cmd = "espeak-ng -v zh -s 150 -a 80 -- \"" + safe_text + "\" 2>/dev/null";
        std::system(cmd.c_str());
    }).detach();
}
