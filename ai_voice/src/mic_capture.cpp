#include "mic_capture.h"
#include <iostream>
#include <cstring>
#include <cstdint>

#ifdef USE_ALSA
#include <alsa/asoundlib.h>
#endif

// -------------------------------------------------------
// WAV 文件头（44 字节，小端）
// -------------------------------------------------------
static void write_le16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

bool MicCapture::writeWavHeader(FILE *fp, int sample_rate, int channels,
                                int bits_per_sample, int data_bytes)
{
    uint8_t hdr[44];
    int byte_rate    = sample_rate * channels * bits_per_sample / 8;
    int block_align  = channels * bits_per_sample / 8;

    memcpy(hdr + 0,  "RIFF", 4);
    write_le32(hdr + 4,  (uint32_t)(36 + data_bytes));
    memcpy(hdr + 8,  "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    write_le32(hdr + 16, 16);                              // PCM 块大小
    write_le16(hdr + 20, 1);                               // AudioFormat = PCM
    write_le16(hdr + 22, (uint16_t)channels);
    write_le32(hdr + 24, (uint32_t)sample_rate);
    write_le32(hdr + 28, (uint32_t)byte_rate);
    write_le16(hdr + 32, (uint16_t)block_align);
    write_le16(hdr + 34, (uint16_t)bits_per_sample);
    memcpy(hdr + 36, "data", 4);
    write_le32(hdr + 40, (uint32_t)data_bytes);

    return fwrite(hdr, 1, sizeof(hdr), fp) == sizeof(hdr);
}

// -------------------------------------------------------
// 列举 ALSA 采集设备
// -------------------------------------------------------
std::vector<std::string> MicCapture::listDevices()
{
    std::vector<std::string> devices;
#ifdef USE_ALSA
    void **hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
        return devices;
    }
    for (void **n = hints; *n; ++n) {
        char *name = snd_device_name_get_hint(*n, "NAME");
        char *ioid = snd_device_name_get_hint(*n, "IOID");
        // 只收录采集设备（Input 或 NULL 表示双向）
        if (name && (ioid == nullptr || strcmp(ioid, "Input") == 0)) {
            devices.emplace_back(name);
        }
        if (name) free(name);
        if (ioid) free(ioid);
    }
    snd_device_name_free_hint(hints);
#else
    // Stub：返回常见耳麦设备名供参考
    devices.push_back("default");
    devices.push_back("plughw:0,0");
    devices.push_back("plughw:1,0");
#endif
    return devices;
}

// -------------------------------------------------------
// 录音并保存 WAV
// -------------------------------------------------------
bool MicCapture::record(const std::string &device,
                        const std::string &out_path,
                        int seconds)
{
    std::cout << "[MicCapture] 设备: " << device
              << "  时长: " << seconds << "s"
              << "  输出: " << out_path << std::endl;

#ifdef USE_ALSA
    snd_pcm_t *handle = nullptr;
    int err = snd_pcm_open(&handle, device.c_str(),
                           SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "[MicCapture] 打开设备失败: " << snd_strerror(err)
                  << "\n  提示：使用 -D <device> 指定正确的耳麦设备，"
                  << "可先用 arecord -l 查看可用设备" << std::endl;
        return false;
    }

    // 设置硬件参数：16kHz / S16_LE / 单声道
    snd_pcm_hw_params_t *params = nullptr;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, (unsigned)CHANNELS);
    unsigned int rate = (unsigned int)SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);

    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        std::cerr << "[MicCapture] 硬件参数设置失败: "
                  << snd_strerror(err) << std::endl;
        snd_pcm_close(handle);
        return false;
    }

    static constexpr snd_pcm_uframes_t FRAME_BUFFER_FRAMES = 512;
    snd_pcm_uframes_t frames = FRAME_BUFFER_FRAMES;
    int frame_bytes = CHANNELS * BITS / 8;
    int total_frames = SAMPLE_RATE * seconds;
    int total_bytes  = total_frames * frame_bytes;

    FILE *fp = fopen(out_path.c_str(), "wb");
    if (!fp) {
        std::cerr << "[MicCapture] 无法创建文件: " << out_path << std::endl;
        snd_pcm_close(handle);
        return false;
    }

    // 先写占位 header，录完后再回填实际字节数
    writeWavHeader(fp, SAMPLE_RATE, CHANNELS, BITS, 0);

    std::vector<int16_t> buf(frames * (size_t)CHANNELS);
    int recorded = 0;

    std::cout << "[MicCapture] 开始录音，请说话..." << std::endl;
    while (recorded < total_frames) {
        snd_pcm_uframes_t to_read =
            std::min(frames, (snd_pcm_uframes_t)(total_frames - recorded));
        snd_pcm_sframes_t n = snd_pcm_readi(handle, buf.data(), to_read);
        if (n == -EPIPE) {
            snd_pcm_prepare(handle);
            continue;
        } else if (n < 0) {
            std::cerr << "[MicCapture] 读取错误: " << snd_strerror((int)n)
                      << std::endl;
            break;
        }
        fwrite(buf.data(), sizeof(int16_t), (size_t)n * CHANNELS, fp);
        recorded += (int)n;
    }
    std::cout << "[MicCapture] 录音结束" << std::endl;

    // 回填 WAV header
    int actual_data_bytes = recorded * frame_bytes;
    rewind(fp);
    writeWavHeader(fp, SAMPLE_RATE, CHANNELS, BITS, actual_data_bytes);
    fclose(fp);
    snd_pcm_close(handle);

    return recorded > 0;
#else
    // Stub 模式：生成静音 WAV（方便在没有 ALSA 的开发机上编译测试）
    FILE *fp = fopen(out_path.c_str(), "wb");
    if (!fp) {
        std::cerr << "[MicCapture] 无法创建文件: " << out_path << std::endl;
        return false;
    }
    int total_bytes = SAMPLE_RATE * CHANNELS * (BITS / 8) * seconds;
    writeWavHeader(fp, SAMPLE_RATE, CHANNELS, BITS, total_bytes);
    // 写入静音 PCM 数据
    std::vector<int16_t> silence(total_bytes / sizeof(int16_t), 0);
    fwrite(silence.data(), sizeof(int16_t), silence.size(), fp);
    fclose(fp);
    std::cout << "[MicCapture] Stub 模式：已写入静音 WAV（"
              << seconds << "s）" << std::endl;
    return true;
#endif
}
