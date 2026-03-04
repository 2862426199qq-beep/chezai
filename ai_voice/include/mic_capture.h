#pragma once

#include <string>
#include <vector>

// MicCapture: ALSA 麦克风采集，录制 16kHz/16-bit/单声道 PCM 并保存为 WAV 文件
// 用于将耳麦/麦克风输入直接送给 whisper.cpp ASR 识别
//
// 编译依赖：libasound2-dev（sudo apt install libasound2-dev）
// 链接选项：-lasound
class MicCapture {
public:
    // 列举系统中所有 ALSA 采集设备（hw:X,Y 格式）
    // 返回设备名称列表，可传入 record() 的 device 参数
    static std::vector<std::string> listDevices();

    // 从麦克风录音并保存为 WAV 文件（16kHz / 16-bit / 单声道）
    // device:    ALSA 设备名，如 "default"、"plughw:1,0"（耳麦通常为 hw:1,0）
    // out_path:  输出 WAV 文件路径
    // seconds:   录音时长（秒），默认 5 秒
    // 返回 true 表示成功
    bool record(const std::string &device,
                const std::string &out_path,
                int seconds = 5);

private:
    static constexpr int SAMPLE_RATE  = 16000;
    static constexpr int CHANNELS     = 1;
    static constexpr int BITS         = 16;

    // 写入标准 WAV 文件头（44 字节）
    static bool writeWavHeader(FILE *fp, int sample_rate, int channels,
                               int bits_per_sample, int data_bytes);
};
