#include "local_asr.h"
#include "mic_capture.h"
#include <iostream>
#include <string>

// -------------------------------------------------------
// 单独调试语言识别（ASR）模块的终端测试程序
//
// 用法：
//   asr_test [-D <device>] [-t <seconds>] [-m <model>] [-o <wav>]
//
//   -D <device>   ALSA 采集设备，默认 "default"
//                 耳麦通常是 "plughw:1,0"（先用 arecord -l 查看）
//   -t <seconds>  录音时长（秒），默认 5
//   -m <model>    whisper.cpp 模型路径，默认 "models/ggml-small-q5_1.bin"
//   -o <wav>      录音输出文件，默认 "/tmp/asr_test.wav"
//   --list        列举可用采集设备后退出
//
// 示例（Stub 模式，无需真实模型）：
//   ./asr_test
//
// 示例（真实模式，从耳麦录音后识别）：
//   ./asr_test -D plughw:1,0 -t 5 -m models/ggml-small-q5_1.bin
// -------------------------------------------------------

static void printUsage(const char *prog)
{
    std::cout
        << "用法: " << prog
        << " [-D device] [-t seconds] [-m model] [-o wav] [--list]\n"
        << "  -D <device>   ALSA 采集设备（默认: default）\n"
        << "                耳麦通常为 plughw:1,0，先用 arecord -l 查看\n"
        << "  -t <seconds>  录音时长，默认 5 秒\n"
        << "  -m <model>    whisper.cpp 模型路径\n"
        << "  -o <wav>      录音输出 WAV 文件路径\n"
        << "  --list        列举可用采集设备\n";
}

int main(int argc, char *argv[])
{
    std::string device  = "default";
    int         seconds = 5;
    std::string model   = "models/ggml-small-q5_1.bin";
    std::string wav_out = "/tmp/asr_test.wav";
    bool        do_list = false;

    // 简单参数解析
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list") {
            do_list = true;
        } else if (arg == "-D" && i + 1 < argc) {
            device = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            try {
                seconds = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "无效的录音时长: " << argv[i] << std::endl;
                return 1;
            }
            if (seconds <= 0) seconds = 5;
        } else if (arg == "-m" && i + 1 < argc) {
            model = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            wav_out = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "未知参数: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "======================================\n"
              << "  ASR 独立调试工具\n"
#ifdef USE_REAL_MODELS
              << "  模式：真实模型 (whisper.cpp)\n"
#else
              << "  模式：Stub（无需真实模型）\n"
#endif
#ifdef USE_ALSA
              << "  采集：ALSA 真实麦克风\n"
#else
              << "  采集：Stub（生成静音 WAV）\n"
#endif
              << "======================================\n";

    // --list：打印可用采集设备
    if (do_list) {
        std::cout << "\n可用 ALSA 采集设备：\n";
        auto devs = MicCapture::listDevices();
        if (devs.empty()) {
            std::cout << "  （未检测到设备，请确认 ALSA 已安装）\n";
        } else {
            for (const auto &d : devs) {
                std::cout << "  " << d << "\n";
            }
        }
        std::cout << "\n提示：耳麦通常对应 plughw:1,0 或 plughw:2,0\n"
                  << "      也可用 `arecord -l` 查看更详细的硬件信息\n";
        return 0;
    }

    // 1. 初始化 ASR 引擎
    LocalASR asr;
    if (!asr.init(model)) {
        std::cerr << "[asr_test] ASR 初始化失败" << std::endl;
        return 1;
    }

    // 2. 录音
    MicCapture mic;
    std::cout << "\n[asr_test] 正在从设备 [" << device << "] 录音 "
              << seconds << " 秒...\n"
              << "  > 请对着耳麦说话 <\n" << std::endl;

    if (!mic.record(device, wav_out, seconds)) {
        std::cerr << "[asr_test] 录音失败\n"
                  << "  提示：\n"
                  << "    1. 用 --list 查看可用设备\n"
                  << "    2. 耳麦通常为 plughw:1,0，用 -D plughw:1,0 指定\n"
                  << "    3. 确认耳麦已插入并被系统识别：arecord -l\n";
        return 1;
    }

    // 3. ASR 识别
    std::cout << "\n[asr_test] 开始识别: " << wav_out << std::endl;
    std::string result = asr.transcribe(wav_out);

    // 4. 输出结果
    std::cout << "\n======================================\n";
    if (result.empty()) {
        std::cout << "  识别结果：（空，未识别到有效语音）\n";
    } else {
        std::cout << "  识别结果：" << result << "\n";
    }
    std::cout << "======================================\n";

    return result.empty() ? 1 : 0;
}
