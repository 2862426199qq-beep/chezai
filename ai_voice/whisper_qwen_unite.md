# Whisper + Qwen2.5 语音 AI 流水线整合说明

> 工作目录：`/home/cat/chezai/ai_voice`  
> 记录日期：2026-03-09

---

## 一、功能概述

完整语音交互链路：

```
麦克风录音(es8388) → Whisper(STT语音转文字) → Qwen2.5-0.5B(LLM意图识别) → JSON指令 → Qt解析执行
```

| 组件 | 说明 |
|------|------|
| Whisper base 20s (RKNN) | 语音识别，NPU 加速，RTF ~0.46 |
| Qwen2.5-0.5B W8A8 (RKLLM) | 意图识别，将自然语言翻译成 JSON |
| es8388 声卡 | 板载麦克风/耳麦输入，card 2 |

---

## 二、运行方式

### 2.1 前置条件

- 声卡 card 2 (es8388) 正常
- 使用**带麦克风的 4 极 TRRS 耳机**（耳麦模式），或**拔掉耳机**用板载主麦克风
- 脚本会自动检测耳机 Jack 并选择对应模式

### 2.2 启动测试

```bash
cd /home/cat/chezai/ai_voice

# 完整流水线（录音 → 文字 → JSON），默认录音最长10秒
./voice_pipeline_test.sh

# 仅语音识别（不经过 LLM），快速测试 Whisper
./voice_pipeline_test.sh stt

# 完整流水线，最长录音15秒
./voice_pipeline_test.sh full 15

# 查看帮助
./voice_pipeline_test.sh --help
```

### 2.3 交互流程

```
1. 程序启动，检查依赖
2. 提示 "按 Enter 开始录音"
3. 按 Enter → 开始录音 → 说话
4. 再按 Enter 结束录音（或等待自动结束）
5. 终端显示 Whisper 识别到的文字
6. (full模式) 终端显示 LLM 生成的 JSON 指令
7. 按 Enter 继续下一轮，输入 q 退出
```

### 2.4 终端输出示例

```
============================================
    语音 AI 流水线测试
  麦克风 → Whisper(STT) → Qwen2.5(LLM) → JSON
============================================

[检查] 依赖文件全部就绪
[音频] 未检测到耳机 → 主麦克风模式
模式: full | 最大录音: 10秒

────────── 新一轮 ──────────

[录音] 按 Enter 开始录音 (最长10秒, 再按 Enter 提前结束)...

>>> 录音中，请说话... <<<

[录音完成] 1508364 bytes  Channels: 2, Sample Rate: 44100, Duration: 00:00:08.55
  音频峰值振幅: 0.037231
  已归一化音频到 -3dB
[Whisper] 正在识别语音...

┌──────────────────────────────────────────┐
│  语音识别结果: 幫我點一首周結輪的歌謝謝
└──────────────────────────────────────────┘
  Real Time Factor (RTF): 1.716 / 8.551 = 0.201

输入 q 退出, 按 Enter 继续...
```

---

## 三、JSON 指令格式

LLM 输出的 JSON 供 Qt 程序解析：

```json
{"action": "open_music", "param": "周杰伦"}
```

### 支持的 action 列表

| action | 含义 | param |
|--------|------|-------|
| `open_music` | 打开/播放音乐 | 歌名或歌手 |
| `close_music` | 关闭音乐 | 无 |
| `open_weather` | 查看天气 | 城市名 |
| `open_map` | 打开导航 | 目的地 |
| `open_camera` | 打开摄像头 | 无 |
| `close_camera` | 关闭摄像头 | 无 |
| `open_radar` | 打开雷达 | 无 |
| `open_dht11` | 查看温湿度 | 无 |
| `open_settings` | 打开设置 | 无 |
| `unknown` | 无法识别 | 原始文本 |

---

## 四、文件结构

```
/home/cat/chezai/ai_voice/
├── voice_pipeline_test.sh                  # 整合测试脚本（本文件说明的程序）
├── Qwen2.5-0.5B_W8A8_RK3588.rkllm         # LLM 模型 (763M)
├── demo_Linux_aarch64/
│   ├── llm_demo                            # LLM 推理程序
│   └── lib/                                # RKLLM 运行库
├── whisper/
│   ├── whisper_encoder_base_20s.rknn       # Whisper Encoder (43M)
│   ├── whisper_decoder_base_20s.rknn       # Whisper Decoder (153M)
│   ├── whisper_bringup.md                  # Whisper 部署记录
│   └── rknn_model_zoo/install/.../
│       └── rknn_whisper_demo               # Whisper 推理程序
└── voice/
    └── voice_bringup.md                    # 声卡调试记录
```

---

## 五、注意事项

1. **耳麦模式**：es8388 录音为双声道，Whisper demo 会自动转换为单声道
2. **LLM 首次加载**：约 10 秒，后续每轮推理约 3-5 秒
3. **录音时长**：建议 5-10 秒，语音指令一般不需要太长
4. **内存占用**：Whisper 和 LLM 串行运行，峰值约 1.5GB，4GB 板子足够

---

## 六、故障排除记录

### 6.1 【已解决】Whisper 识别结果一直为空

**现象**：脚本运行后录音文件有数据（非零 bytes），但 Whisper 输出空白，10 次测试仅 1 次成功。

**排查过程**：

1. **初始怀疑 `arecord -Dhw:2` 绕过 PulseAudio**：脚本最初用 `arecord -D hw:2,0` 录音，绕过 PulseAudio 导致 UCM 未初始化 ADC。改成 `parecord` 后仍然全零。

2. **发现 `parecord` 强制 16kHz 导致问题**：脚本用 `parecord --rate=16000` 强制降采样，但 PulseAudio source 原生 44100Hz，强制请求 16kHz 由 PulseAudio 重采样，可能导致数据异常。

3. **真正根因 —— 耳机没有麦克风**：
   - 用 `sox` 分析录音：`Maximum amplitude: 0.000000`，完全空数据
   - 对比成功的 test_zh.wav 衰减到同音量（max=0.038），Whisper **能正常识别**→ 排除音量问题
   - 检查 `rec2.wav`（用户手动用 voice_bringup.md 方法录的）：`Rough frequency: 14022Hz`，全是高频噪声，无人声
   - 用户使用的是 **3 极 TRS 普通耳机（无麦克风）**，耳机 Headset Mic 端口（Line 1）上无信号源

4. **验证**：
   - 拔掉耳机 → 切换到主麦克风模式（`audio_init_spk.sh`）→ `parecord` 后台录音 max=0.022（有信号）
   - 对着板载 MIC 说话 → 归一化后送 Whisper → **识别成功**

**修复措施（3 处改动）**：

| 改动 | 说明 |
|------|------|
| `arecord` → `parecord` | 走 PulseAudio，UCM 正确初始化 ADC |
| 去掉 `--rate=16000 --channels=2` | 用 PulseAudio 原生 44100Hz/2ch 录音，Whisper demo 内部自动 resample + convert_channels |
| 自动检测耳机 Jack | 读 `amixer -c 2 cget numid=37`（Headphone Jack），on→耳麦模式，off→主麦模式 |

**额外改进**：
- 录音后 `sox norm -3` 归一化到 -3dB，提升弱信号识别率
- 录音后显示 `Maximum amplitude`，方便判断是否有有效声音
- 振幅低于 0.002 时警告用户检查麦克风

### 6.2 LubanCat-4 硬件音频配置

**设备树音频路由**（`/proc/device-tree/es8388-sound/rockchip,audio-routing`）：

```
输出：Headphone → LOUT1/ROUT1 + Headphone Power
输入：LINPUT1/RINPUT1 → Headset Mic (Line 1, 耳机麦)
      LINPUT2/RINPUT2 → Main Mic    (Line 2, 板载麦)
```

**关键结论**：
- **无板载扬声器**：设备树没有 Speaker/amplifier 节点，UCM 中 Speaker 端口是模板默认值，硬件未连接
- **有板载主麦克风**：通过 `LINPUT2/RINPUT2`（Differential Mux = Line 2）接入
- **耳机 Jack 检测**：`numid=37`（Headphone Jack）、`numid=38`（Headset Mic Jack），只读 BOOLEAN

### 6.3 两种工作模式总结

| | 耳麦模式（有麦克风的 4 极 TRRS 耳机） | 主麦克风模式（拔掉耳机） |
|---|---|---|
| 初始化 | `audio_init_hp.sh` | `audio_init_spk.sh` |
| 输入路由 | Line 1 → Headset Mic | Line 2 → Main Mic |
| 输出 | 耳机（LOUT1/ROUT1） | 无（板卡无扬声器），需 HDMI/DP |
| Jack 检测 | numid=37 = on | numid=37 = off |
| 注意 | **必须是 4 极 TRRS 带麦耳机**，3 极 TRS 无麦耳机录音全零 | 对着板子上 MIC 小孔说话 |

### 6.4 实测结果

使用主麦克风模式，对着板载 MIC 说话：

```
[录音完成] 1508364 bytes  Channels: 2, Sample Rate: 44100, Duration: 00:00:08.55
  音频峰值振幅: 0.037231
  已归一化音频到 -3dB

┌──────────────────────────────────────────┐
│  语音识别结果: 幫我點一首周結輪的歌謝謝
└──────────────────────────────────────────┘
  Real Time Factor (RTF): 1.716 / 8.551 = 0.201
```

```
[录音完成] 1661484 bytes  Channels: 2, Sample Rate: 44100, Duration: 00:00:09.42
  音频峰值振幅: 0.029572
  已归一化音频到 -3dB

┌──────────────────────────────────────────┐
│  语音识别结果: Hello,我是你爸爸你知道嗎?
└──────────────────────────────────────────┘
  Real Time Factor (RTF): 1.183 / 9.419 = 0.126
```

RTF 0.12~0.20，实时性良好。中文识别输出为繁体（Whisper base 模型特性），不影响 LLM 理解。


