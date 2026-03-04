# ai_voice — 离线 AI 语音交互模块（插件化架构）

全离线、模块化语音交互流水线，参考 [LLM_Voice_Flow](https://github.com/superxiaobai-1/LLM_Voice_Flow) 架构设计，基于 **whisper.cpp**（ASR）+ **DeepSeek-R1-1.5B**（LLM）+ **espeak-ng**（TTS）。

核心特性：
- 🔌 **插件化接口**：`IAsr/ILlm/ITts Engine` 抽象接口，可随时替换底层实现
- ⚡ **流式低延迟**：LLM `chatStream()` 边生成边推 TTS 队列（`VoicePipeline::runStream`）
- 🔁 **双缓冲 TTS 队列**：`TtsEngine` 内置生产者-消费者队列，LLM 生成一句立刻播一句
- 🔧 **可激活真实模型**：通过 CMake `-DUSE_REAL_MODELS=ON` 切换，无需修改代码

---

## 模块结构

```
ai_voice/
├── include/
│   ├── i_asr_engine.h      # 抽象 ASR 接口（插件基类）
│   ├── i_llm_engine.h      # 抽象 LLM 接口（插件基类，含 chatStream）
│   ├── i_tts_engine.h      # 抽象 TTS 接口（插件基类，含 enqueue/waitDone）
│   ├── voice_pipeline.h    # 全链路编排器（run / runStream）
│   ├── local_asr.h         # LocalASR  实现（whisper.cpp）
│   ├── llm_engine.h        # LlmEngine 实现（llama.cpp + DeepSeek）
│   ├── tts_engine.h        # TtsEngine 实现（espeak-ng + 双缓冲队列）
│   └── mic_capture.h       # MicCapture 实现（ALSA 麦克风录音）
├── src/
│   ├── voice_pipeline.cpp  # 流水线逻辑（批量模式 + 流式模式）
│   ├── local_asr.cpp       # ASR：stub 或真实 whisper.cpp 推理
│   ├── llm_engine.cpp      # LLM：stub 或真实 llama.cpp 流式推理
│   ├── tts_engine.cpp      # TTS：espeak-ng + 工作线程队列
│   ├── mic_capture.cpp     # 麦克风采集：ALSA 录音 → WAV 文件
│   ├── asr_test.cpp        # ASR 单独调试工具（麦克风录音 → ASR 识别）
│   └── main.cpp            # 完整链路测试程序
├── CMakeLists.txt
└── README.md
```

## 数据流（流式模式）

```
麦克风/WAV
    │
    ▼ IAsrEngine::transcribeStream()
ASR (whisper.cpp)
    │ 逐段文字 → 拼接完整 utterance
    ▼ ILlmEngine::chatStream(token_callback)
LLM (DeepSeek-R1-1.5B)
    │ 每 token 回调 → 检测句末标点 → 入队
    ▼ ITtsEngine::enqueue(sentence)
TTS 双缓冲队列
    │ 工作线程消费队列 → 逐句播报
    ▼ espeak-ng（本地离线）
耳机/扬声器

延迟目标：首句播出 < 2s（RK3588S，4线程推理）
```

---

## 依赖

| 依赖 | 用途 | 安装方式 |
|------|------|---------|
| [whisper.cpp](https://github.com/ggerganov/whisper.cpp) | 离线中文 ASR | 源码编译 |
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | DeepSeek 端侧推理 | 源码编译 |
| espeak-ng | 中文 TTS | `apt install espeak-ng` |

---

## 编译（Stub 模式，无需真实依赖）

```bash
cd ai_voice/
mkdir build && cd build
cmake ..
make -j$(nproc)

./ai_voice_test                # 使用内置 stub 数据
./ai_voice_test /path/to.wav   # 指定 WAV 文件（stub 模式忽略文件内容）
```

## 编译（真实模型模式）

### 1. 准备依赖

```bash
# 编译 whisper.cpp
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp && mkdir build && cd build
cmake .. && make -j$(nproc)
cd ../../

# 编译 llama.cpp
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp && mkdir build && cd build
cmake .. && make -j$(nproc)
cd ../../
```

### 2. 下载模型

```bash
mkdir -p models/

# whisper small 量化版（~50MB）
wget -P models/ \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin

# DeepSeek-R1-1.5B Q4 量化版（~1GB）
pip install huggingface_hub
huggingface-cli download bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF \
    DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf --local-dir models/
```

### 3. 编译并运行

```bash
cd ai_voice/build
cmake .. \
    -DUSE_REAL_MODELS=ON \
    -DWHISPER_DIR=../../whisper.cpp \
    -DLLAMA_DIR=../../llama.cpp
make -j$(nproc)

# 录制测试音频（3秒）
arecord -f S16_LE -r 16000 -c 1 -d 3 test.wav

# 运行完整流水线
./ai_voice_test test.wav
```

---

## 单独调试语言识别（ASR）模块

### 快速终端测试（asr_test）

`asr_test` 是专门用于调试 ASR 的独立工具，支持从麦克风/耳麦直接录音后识别，无需启动完整流水线。

#### Stub 模式（无需任何依赖，验证编译）

```bash
cd ai_voice/build
cmake ..
make asr_test

./asr_test --list   # 查看 ALSA 识别到的采集设备（stub 模式返回示例列表）
./asr_test -t 3     # 录音 3 秒 → ASR 识别（stub 返回固定文本）
```

#### 真实麦克风模式（RK3588S 板子上运行）

```bash
# 1. 安装 ALSA 开发库
sudo apt install libasound2-dev

# 2. 编译（启用 ALSA 录音 + 真实 whisper.cpp 模型）
cd ai_voice/build
cmake .. \
    -DUSE_ALSA=ON \
    -DUSE_REAL_MODELS=ON \
    -DWHISPER_DIR=../../whisper.cpp
make asr_test

# 3. 查看系统识别到的采集设备（包括耳麦）
./asr_test --list
# 也可用系统命令：
arecord -l

# 4. 指定耳麦设备录音识别（耳麦通常是 plughw:1,0 或 plughw:2,0）
./asr_test -D plughw:1,0 -t 5 -m models/ggml-small-q5_1.bin
```

**耳麦无法识别的排查步骤：**
1. `arecord -l` — 确认系统已识别耳麦，记录对应的 card/device 编号
2. `arecord -D plughw:<card>,<device> -f S16_LE -r 16000 -c 1 -d 3 /tmp/test.wav` — 先用系统命令验证录音
3. `aplay /tmp/test.wav` — 回放确认音频内容正确
4. `./asr_test -D plughw:<card>,<device> -t 5` — 再用 asr_test 测试识别

---

## 集成到 VehicleTerminal Qt 主程序

`VehicleTerminal/mainwindow.h` 已预留 `onBtnAiVoice()` 槽和 `btnAiVoice` 按钮。

### Qt 集成示例

在 `VehicleTerminal.pro` 中添加：
```qmake
LIBS        += -L../ai_voice/build -lai_voice_lib -lstdc++ -lpthread
INCLUDEPATH += ../ai_voice/include
```

在 `mainwindow.cpp` 的 `onBtnAiVoice()` 中调用：
```cpp
#include "local_asr.h"
#include "llm_engine.h"
#include "tts_engine.h"
#include "voice_pipeline.h"

void MainWindow::onBtnAiVoice()
{
    // 使用 QtConcurrent 避免阻塞 UI 线程
    QtConcurrent::run([this]() {
        LocalASR  asr;
        LlmEngine llm;
        TtsEngine tts;

        asr.init("models/ggml-small-q5_1.bin");
        llm.init("models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf", 4);

        VoicePipeline pipeline(asr, llm, tts);
        pipeline.setIntentHandler([this](const std::string &intent,
                                         const std::string &) {
            // 回到 Qt 主线程执行 UI 操作
            QMetaObject::invokeMethod(this, [this, intent]() {
                if      (intent == "OPEN_MUSIC")   emit SendCommandToMusic(1);
                else if (intent == "OPEN_MAP")     emit SendCommandToMap(1);
                else if (intent == "OPEN_MONITOR") emit SendCommandToMonitor(1);
                else if (intent == "OPEN_WEATHER") { /* 切换天气界面 */ }
            }, Qt::QueuedConnection);
        });

        pipeline.runStream("/tmp/record.wav");
    });
}
```

---

## 意图列表

| 意图 | 触发词示例 | Qt 动作 |
|------|----------|---------|
| `OPEN_MUSIC`   | 打开音乐、播放歌曲 | `emit SendCommandToMusic(1)` |
| `OPEN_MAP`     | 打开地图、导航 | `emit SendCommandToMap(1)` |
| `OPEN_WEATHER` | 查天气 | 切换天气界面 |
| `OPEN_MONITOR` | 倒车、摄像头 | `emit SendCommandToMonitor(1)` |
| `READ_TEMP`    | 温度、温湿度 | 读取 DHT11 |
| `UNKNOWN`      | 其他 | TTS 提示 |

