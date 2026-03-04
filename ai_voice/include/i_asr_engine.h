#pragma once

#include <string>
#include <functional>

// IAsr Engine: ASR 模块抽象接口（插件化）
// All ASR backends must implement this interface.
class IAsrEngine {
public:
    virtual ~IAsrEngine() = default;

    // 初始化模型
    virtual bool init(const std::string &model_path) = 0;

    // 批量转写：WAV 文件 → 文字
    // wav_path: 16kHz 单声道 WAV
    virtual std::string transcribe(const std::string &wav_path) = 0;

    // 流式转写回调（可选实现，不支持时返回 false）
    // callback: 每当产生一段文字时调用，参数为当前段落文本
    using StreamCallback = std::function<void(const std::string &segment)>;
    virtual bool transcribeStream(const std::string &wav_path,
                                  StreamCallback callback)
    {
        // 默认：退化为批量转写后整体回调一次
        std::string result = transcribe(wav_path);
        if (!result.empty()) {
            callback(result);
        }
        return !result.empty();
    }
};
