# ai_voice — 离线 AI 语音交互模块

基于 **whisper.cpp**（ASR）+ **DeepSeek-R1-1.5B**（LLM）+ **espeak-ng**（TTS）的完全离线车载语音交互系统。

> 当前为 **stub 模式**：各模块均有完整结构和接口，但底层推理暂用模拟数据替代，等待 whisper.cpp / llama.cpp 依赖接入后即可激活。

---

## 模块结构

```
ai_voice/
├── include/
│   ├── local_asr.h     # 离线语音识别接口（whisper.cpp）
│   ├── llm_engine.h    # LLM 推理引擎接口（llama.cpp + DeepSeek）
│   └── tts_engine.h    # 离线 TTS 接口（espeak-ng）
├── src/
│   ├── local_asr.cpp   # ASR 实现（stub + TODO 注释）
│   ├── llm_engine.cpp  # LLM 实现（stub + 意图解析）
│   ├── tts_engine.cpp  # TTS 实现（espeak-ng system 调用）
│   └── main.cpp        # 完整链路测试程序
├── CMakeLists.txt
└── README.md
```

---

## 依赖

| 依赖 | 用途 | 安装方式 |
|------|------|---------|
| [whisper.cpp](https://github.com/ggerganov/whisper.cpp) | 离线中文 ASR | 源码编译 |
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | DeepSeek 端侧推理 | 源码编译 |
| espeak-ng | 中文 TTS 语音播报 | `apt install espeak-ng` |
| espeak-ng-data | 中文语音数据 | `apt install espeak-ng-data` |

---

## 模型下载

### whisper.cpp 模型（中文 ASR）

```bash
# 推荐：small 量化版（~50MB，适合 RK3588S）
cd models/
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin
```

### DeepSeek-R1-1.5B GGUF 模型（端侧 LLM）

```bash
# Q4 量化版（~1GB，6TOPS NPU 可加速）
# 从 Hugging Face 下载
huggingface-cli download bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF \
    DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf \
    --local-dir models/
```

---

## 编译（stub 模式，无需真实依赖）

```bash
cd ai_voice/
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行测试
./ai_voice_test
# 或指定 WAV 文件
./ai_voice_test /path/to/audio.wav
```

## 编译（完整模式，接入 whisper.cpp + llama.cpp）

```bash
# 1. 编译 whisper.cpp
git clone https://github.com/ggerganov/whisper.cpp.git
cd whisper.cpp && mkdir build && cd build
cmake .. -DWHISPER_BUILD_SHARED_LIBS=OFF
make -j$(nproc)
cd ../../

# 2. 编译 llama.cpp（RK3588S 可启用 RKNN NPU 加速）
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp && mkdir build && cd build
cmake .. -DGGML_RKNN=ON  # 可选：启用 RKNN NPU
make -j$(nproc)
cd ../../

# 3. 编译 ai_voice（指定依赖路径）
cd ai_voice/build
cmake .. \
    -DWHISPER_DIR=../../whisper.cpp \
    -DLLAMA_DIR=../../llama.cpp
# 并取消 CMakeLists.txt 中对应的注释行
make -j$(nproc)
```

---

## 测试

```bash
# stub 模式测试（无需真实音频）
./ai_voice_test

# 真实音频测试（需要 16kHz 单声道 WAV）
# 录制测试音频
arecord -f S16_LE -r 16000 -c 1 -d 3 test.wav
./ai_voice_test test.wav
```

---

## 集成到 VehicleTerminal Qt 主程序

1. 在 `VehicleTerminal.pro` 中添加：
   ```qmake
   LIBS += -L../ai_voice/build -lai_voice_lib -lstdc++ -lpthread
   INCLUDEPATH += ../ai_voice/include
   ```

2. 在 `mainwindow.h` 中已预留 `onBtnAiVoice()` 槽和 `btnAiVoice` 按钮。

3. 在 `mainwindow.cpp` 的 `onBtnAiVoice()` 中调用：
   ```cpp
   // 示例：异步启动语音识别 → LLM → 意图执行
   QtConcurrent::run([this]() {
       LocalASR asr;
       LlmEngine llm;
       TtsEngine tts;
       asr.init("models/ggml-small-q5_1.bin");
       llm.init("models/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf");
       std::string text    = asr.transcribe("/tmp/record.wav");
       std::string reply   = llm.chat(text);
       std::string intent  = llm.parseIntent(reply);
       tts.speakAsync(reply);
       // 根据 intent 发送 Qt 信号触发对应界面
   });
   ```

---

## 语音意图说明

| 意图 | 触发词示例 | Qt 动作 |
|------|----------|---------|
| `OPEN_MUSIC` | 打开音乐、播放歌曲 | `emit SendCommandToMusic(1)` |
| `OPEN_MAP` | 打开地图、导航 | `emit SendCommandToMap(1)` |
| `OPEN_WEATHER` | 查天气 | 切换到天气界面 |
| `OPEN_MONITOR` | 倒车、摄像头 | `emit SendCommandToMonitor(1)` |
| `READ_TEMP` | 温度、温湿度 | 读取 DHT11 传感器 |
| `UNKNOWN` | 其他 | TTS 提示无法理解 |
