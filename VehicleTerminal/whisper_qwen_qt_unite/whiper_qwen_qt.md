# Whisper + Qwen2.5 语音链路集成到 Qt VehicleTerminal

> 平台: LubanCat-4 (RK3588S) | Qt 5.15 | 日期: 2026-03-10

---

## 1. 整体架构

```
用户点击 [AI Voice] 按钮
        │
        ▼
  ┌─────────────┐
  │  parecord    │  录音 5s (44100Hz/2ch, PulseAudio 原生)
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │  sox norm    │  归一化到 -3dB
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │  Whisper     │  rknn_whisper_demo (STT, base模型)
  │  RKNN 推理   │  输出: 中文识别文本
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │  Qwen2.5    │  llm_demo 常驻进程 (0.5B W8A8 RKLLM)
  │  LLM 推理   │  输出: {"action":"xxx","param":"xxx"}
  └──────┬──────┘
         ▼
  ┌─────────────┐
  │ MainWindow   │  dispatchVoiceAction() → 命中Qt控件
  │ 动作分发     │  open_music → musicPlayer->show() + play
  └─────────────┘
```

## 2. 新增文件

### 2.1 voice_assistant.h / voice_assistant.cpp

**位置**: `VehicleTerminal/voice_assistant.h`, `VehicleTerminal/voice_assistant.cpp`

**职责**: 封装完整的语音AI管线（录音→归一化→Whisper→LLM→JSON），所有外部二进制通过 QProcess 调用，后台线程 (QtConcurrent::run) 执行。

**关键 API**:

```cpp
class VoiceAssistant : public QObject {
    Q_OBJECT
public:
    void start(int recordSeconds = 5);   // 启动管线
    void stopRecording();                // 提前结束录音
    void preInitAudio();                 // 后台预初始化音频
    void preInitLlm();                   // 后台预加载 LLM 模型（方案B）
    bool isRunning() const;

signals:
    void statusChanged(const QString &status);  // 状态更新 (UI显示)
    void sttResult(const QString &text);         // Whisper识别文字
    void actionResult(const QString &action, const QString &param);  // LLM输出JSON
    void finished();
    void errorOccurred(const QString &msg);

private:
    QProcess *m_llmProc = nullptr;   // 常驻 llm_demo 子进程
    QMutex    m_llmMutex;            // 保护 m_llmProc 的并发访问
    bool      m_llmReady = false;    // 模型是否已加载就绪
    bool ensureLlmRunning();         // 确保 llm_demo 在运行（调用者持锁）
    void shutdownLlm();              // 优雅关闭常驻进程
};
```

**管线步骤**:

| 步骤 | 方法 | 外部命令 | 说明 |
|------|------|----------|------|
| 1 | `initAudio()` | `amixer -c 2 cget numid=37` + `timeout 8s bash audio_init_*.sh` | 自动检测耳机Jack，选择耳麦/主麦模式；超时仅告警，不阻断主流程 |
| 2 | `recordAudio(N)` | `parecord --file-format=wav /tmp/voice_qt_input.wav` | 原生44100Hz录音，N秒后terminate |
| 3 | `normalizeAudio()` | `sox input.wav output.wav norm -3` | 归一化到-3dB |
| 4 | `runWhisper()` | `timeout 35s rknn_whisper_demo encoder.rknn decoder.rknn zh input.wav` | 提取 "Whisper output: xxx"；超时会返回明确错误 |
| 5 | `runLlm(text)` | 向常驻 `llm_demo` stdin 写 prompt，读 stdout 至 `user:` | 方案B 常驻进程，优先解析 `robot:` 行 JSON，容错处理 |

## 3. MainWindow 修改

### 3.1 移除旧依赖

- 删除 `#include "llm_engine.h"` / `#include "tts_engine.h"` (这两个文件从未存在)
- 删除 `.pro` 中 `INCLUDEPATH += ../ai_voice/include` 和 `LIBS += -lai_voice_lib`
- 删除 `getSpeechResult()` 中的 `LlmEngine`/`TtsEngine` 调用

### 3.2 新增成员

```cpp
// mainwindow.h
#include "voice_assistant.h"
VoiceAssistant *m_voiceAssistant;
```

### 3.3 构造函数预加载

```cpp
// MainWindow 构造函数中:
m_voiceAssistant->preInitAudio();   // 后台预初始化音频
m_voiceAssistant->preInitLlm();     // 后台预加载 LLM 模型（方案B，~10s）
```

### 3.4 onBtnAiVoice() 重写

```cpp
void MainWindow::onBtnAiVoice()
{
    if (m_voiceAssistant->isRunning()) {
        m_voiceAssistant->stopRecording();  // 再次点击提前结束
        return;
    }
    m_voiceAssistant->start(5);  // 录音5秒
}
```

- 点击 AI Voice → 按钮变红 "■ Stop"，开始录音
- 再次点击 → 提前结束录音
- 管线完成 → 按钮恢复 "AI Voice"

### 3.5 信号连接

```cpp
connect(m_voiceAssistant, &VoiceAssistant::statusChanged, this, &MainWindow::onVoiceStatus);
connect(m_voiceAssistant, &VoiceAssistant::sttResult,     this, &MainWindow::onVoiceStt);
connect(m_voiceAssistant, &VoiceAssistant::actionResult,  this, &MainWindow::onVoiceAction);
connect(m_voiceAssistant, &VoiceAssistant::errorOccurred, this, &MainWindow::onVoiceError);
connect(m_voiceAssistant, &VoiceAssistant::finished,      this, &MainWindow::onVoiceFinished);
```

## 4. JSON Action → Qt 控件映射

### 4.1 LLM Prompt

```
你是车载语音助手,根据用户语音指令输出JSON,只输出一行JSON不要其他内容。
格式:{"action":"xxx","param":"xxx"}
支持的action: open_music,close_music,play_music,next_music,prev_music,pause_music,
  open_weather,open_map,open_camera,close_camera,open_radar,open_monitor,open_settings,unknown
用户说:{语音识别文本}
```

### 4.2 dispatchVoiceAction() 映射表

| action | Qt 操作 | 命中控件 |
|--------|---------|----------|
| `open_music` | `SendCommandToMusic(SHOW + PLAY)` | MusicPlayer窗口 |
| `play_music` | `SendCommandToMusic(SHOW + PLAY)` | MusicPlayer窗口 |
| `close_music` | `SendCommandToMusic(CLOSE)` | 关闭MusicPlayer |
| `next_music` | `SendCommandToMusic(SHOW + NEXT)` | 下一首 |
| `prev_music` | `SendCommandToMusic(SHOW + PRE)` | 上一首 |
| `pause_music` | `SendCommandToMusic(SHOW + PAUSE)` | 暂停 |
| `open_weather` | `weather->show()` | Weather窗口 |
| `open_map` | `SendCommandToMap(SHOW)` | BaiduMap窗口 |
| `open_camera` | `onBtnCameraToggle()` (若关闭则开) | 摄像头画面 |
| `close_camera` | `onBtnCameraToggle()` (若开启则关) | 摄像头画面 |
| `open_radar` | `onBtnRadarFull()` | 全屏雷达PPI |
| `open_monitor`/`open_dht11` | `onBtnMonitor()` | Monitor窗口(含温湿度) |
| `open_settings` | `onBtnSetting()` | SettingWindow |
| `unknown` | 不操作 | 仅显示状态 |

### 4.3 Action 别名映射 (aliasMap)

LLM 可能输出不在标准列表内的近义 action，通过 `aliasMap` 做归一化：

| LLM 原始输出 | 归一化为 | 说明 |
|-------------|---------|------|
| `set_name` / `settings` / `set_settings` / `setting` | `open_settings` | 设置页面 |
| `play` | `play_music` | 播放音乐 |
| `music` | `open_music` | 打开音乐 |
| `next` | `next_music` | 下一首 |
| `prev` / `previous_music` | `prev_music` | 上一首 |
| `stop_music` | `pause_music` | 暂停 |
| `weather` | `open_weather` | 天气 |
| `map` / `navigation` / `navigate` | `open_map` | 地图 |
| `camera` | `open_camera` | 摄像头 |
| `radar` | `open_radar` | 雷达 |
| `monitor` | `open_monitor` | 监控 |

### 4.4 dispatch 核心代码

```cpp
void MainWindow::dispatchVoiceAction(const QString &action, const QString &param)
{
    if (action == "open_music" || action == "play_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PLAY);
    } else if (action == "close_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_CLOSE);
    } else if (action == "next_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_NEXT);
    } else if (action == "prev_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PRE);
    } else if (action == "pause_music") {
        emit SendCommandToMusic(MUSIC_COMMAND_SHOW);
        emit SendCommandToMusic(MUSIC_COMMAND_PAUSE);
    } else if (action == "open_weather") {
        weather->show();
    } else if (action == "open_map") {
        emit SendCommandToMap(MAP_COMMAND_SHOW);
    } else if (action == "open_camera") {
        if (!cameraRunning) onBtnCameraToggle();
    } else if (action == "close_camera") {
        if (cameraRunning) onBtnCameraToggle();
    } else if (action == "open_radar") {
        onBtnRadarFull();
    } else if (action == "open_monitor" || action == "open_dht11") {
        onBtnMonitor();
    } else if (action == "open_settings") {
        onBtnSetting();
    }
}
```

## 5. VehicleTerminal.pro 修改

```diff
- INCLUDEPATH += ../ai_voice/include
- LIBS += -L../ai_voice/build -lai_voice_lib -lstdc++ -lpthread

  SOURCES += \
+     voice_assistant.cpp \
      ...

  HEADERS += \
+     voice_assistant.h \
      ...
```

## 6. 编译与运行

```bash
cd ~/chezai/VehicleTerminal
qmake VehicleTerminal.pro
make -j4
./VehicleTerminal
```

编译结果: **成功** (仅有 `param` 未使用的 warning，后续可用于音乐搜索传参)

## 7. 使用流程

1. 确保板端已安装: `parecord`, `sox`, Whisper RKNN 模型, Qwen2.5 RKLLM 模型
2. 确保 PulseAudio 正在运行
3. 启动 `./VehicleTerminal`
4. 点击底部导航栏 **AI Voice** 按钮
5. 对麦克风说话 (5秒)，或再次点击提前结束
6. 右侧状态栏依次显示:
   - `AI Voice: 录音中，请说话...`
   - `AI Voice: Whisper 识别中...`
   - `STT: 幫我打開音樂` (识别结果)
   - `AI Voice: LLM 推理中...`
   - `AI: open_music` (最终动作)
7. 相应窗口自动弹出

## 8. 语音指令示例

| 语音 | STT输出(Whisper) | LLM JSON | 命中控件 |
|------|------------------|----------|----------|
| "打开音乐" | 打開音樂 | `{"action":"open_music","param":""}` | MusicPlayer |
| "下一首歌" | 下一首歌 | `{"action":"next_music","param":""}` | MusicPlayer→下一首 |
| "查看天气" | 查看天氣 | `{"action":"open_weather","param":""}` | Weather窗口 |
| "打开地图" | 打開地圖 | `{"action":"open_map","param":""}` | BaiduMap窗口 |
| "关闭摄像头" | 關閉攝像頭 | `{"action":"close_camera","param":""}` | 关闭摄像头 |
| "打开雷达" | 打開雷達 | `{"action":"open_radar","param":""}` | 全屏雷达 |
| "打开设置" | 打開設置 | `{"action":"open_settings","param":""}` | 设置页面 |

## 9. 注意事项

1. **Whisper 输出繁体**: Whisper base 模型默认输出繁体中文，不影响 LLM 理解
2. **LLM 常驻进程（方案B）**: 应用启动时后台预加载模型 (~10s)，此后每次语音指令仅 ~3s 推理延迟；若进程意外退出会自动重启
3. **录音设备**: 无耳机时自动使用板载主麦克风 (Line 2)，插入4极TRRS耳机则使用耳麦 (Line 1)
4. **3极TRS耳机无法录音**: 参见 `whisper_qwen_unite.md` 第6节故障排除
5. **部分功能待完善**: 音乐搜索按歌名 (param 字段预留)、地图导航目的地等后续可扩展

## 10. 录音失败修复记录（2026-03-10）

### 10.1 现象

- 点击 `AI Voice` 后先显示 `初始化音频...` 约 2-3 秒
- 随后显示 `录音中，请说话...`
- 最终经常直接提示 `录音失败`

### 10.2 原因

- `recordAudio()` 中对振幅阈值判定过严：`amp < 0.002` 直接失败
- 每次点击都重复执行音频初始化，造成固定等待
- 失败信息不带具体原因，不利于定位

### 10.3 已修复

- `VoiceAssistant` 新增 `m_audioInited`，音频初始化改为仅首次执行
- 录音失败时返回具体原因：
    - `parecord 启动失败: ...`
    - `录音文件未生成`
    - `录音数据过小: xxx bytes`
- 低振幅改为告警，不再直接失败：
    - 原逻辑：`amp < 0.002` => fail
    - 新逻辑：`amp < 0.0005` => warning + 继续进入 Whisper

### 10.4 结果

- 解决了“总是录音失败”的误判问题
- 后续点击 `AI Voice` 不再重复等待 2-3 秒初始化

## 11. 函数流（Function Flow）

### 11.1 主触发入口

1. `MainWindow::MainWindow()` → `preInitAudio()` + `preInitLlm()` (后台预加载)
2. `MainWindow::onBtnAiVoice()`
3. `VoiceAssistant::start(5)`
4. `QtConcurrent::run(...)` 启动后台语音任务

### 11.2 LLM 常驻进程生命周期

1. `preInitLlm()` → `QtConcurrent::run` → `ensureLlmRunning()`
2. `ensureLlmRunning()` → 启动 `taskset f0 llm_demo` → 等待 `user:` → `m_llmReady=true`
3. `runLlm()` → `ensureLlmRunning()` (已就绪直接返回) → 写stdin → 读stdout至`user:` → 解析JSON
4. `~VoiceAssistant()` → `shutdownLlm()` → 写`exit\n` → waitForFinished → delete

### 11.3 语音任务主流程

`VoiceAssistant::runPipeline()` 按顺序调用：

1. `initAudio()`
2. `recordAudio()`
3. `normalizeAudio()`
4. `runWhisper()`
5. `runLlm()`
6. JSON 解析 + 兜底规则
7. `emit actionResult(action, param)`

### 11.4 UI 分发流程

1. `MainWindow::onVoiceAction(action, param)`
2. `MainWindow::dispatchVoiceAction(action, param)`
3. 发送对应模块命令：
    - `SendCommandToMusic(...)`
    - `SendCommandToMap(...)`
    - `onBtnMonitor()/onBtnSetting()/onBtnRadarFull()`

## 12. 数据流（Data Flow）

### 12.1 输入到输出的数据形态

1. 麦克风输入（PCM）
2. `/tmp/voice_qt_input.wav`（44.1kHz，2ch）
3. Whisper 文本：`STT: ...`
4. LLM 输出：`{"action":"...","param":"..."}`
5. Qt 行为调用（窗口显示、按钮命令、播放器控制）

### 12.2 关键状态流

状态栏会按阶段更新：

1. `AI Voice: 初始化音频...`
2. `AI Voice: 录音中，请说话...`
3. `AI Voice: Whisper 识别中...`
4. `STT: ...`
5. `AI Voice: LLM 推理中...`
6. `AI Voice: LLM JSON: ...`
7. `AI: open_music ...` 或 `AI: unknown + STT + LLM JSON`

## 13. JSON 对应行为的形成机理（为什么能命中）

### 13.1 不是训练新模型，而是“提示词约束 + 规则分发”

本项目命中行为由三层共同形成：

1. **LLM Prompt 约束输出格式**
    - 明确要求只输出 `{"action":"xxx","param":"xxx"}`
    - 明确 `action` 枚举
2. **JSON 解析与容错**
    - 优先解析 `robot:` 行
    - 畸形 JSON（如多一个 `}`）做裁剪容错
    - 非严格格式（`action:open_music`）正则兜底
3. **STT 文本兜底映射**
    - 若 LLM 返回 `unknown/xxx/空`，根据 STT 文本关键词直接回退
    - 支持简繁体、去标点、去空白匹配

所以这套链路能命中，是因为 **LLM 负责语义归一化，Qt 代码负责确定性动作执行**。

### 13.2 JSON 到控件动作的确定性映射

- `action=open_music/play_music` => `SendCommandToMusic(SHOW+PLAY)`
- `action=open_map` => `SendCommandToMap(SHOW)`
- `action=open_weather` => `weather->show()`
- `action=open_camera/close_camera` => 摄像头开关按钮逻辑

即使 LLM 轻微波动，只要最终 action 归一到支持集合，就会稳定命中。

## 14. 故障排除与修复闭环（最终版）

### 14.1 `unknow/unknown` 频发

现象：语音识别正确，但最终 `AI: unknow`。

原因：

1. 早期逻辑误抓了提示词模板 JSON（`{"action":"xxx"...}`）
2. LLM 输出偶发非严格 JSON

修复：

1. 优先抓取 `robot:` 行 JSON
2. JSON 畸形裁剪后重解析
3. 正则提取 `action/param` 兜底
4. STT 文本关键词兜底映射

### 14.2 卡在 `Whisper 识别中...`

现象：界面长时间停在 Whisper 阶段。

原因：`rknn_whisper_demo` 在板端偶发阻塞。

修复：

1. `runWhisper()` 改为 `timeout 35s`
2. 超时后 kill 并返回明确错误：`Whisper 超时退出`

### 14.3 `音频初始化失败: audio_init 脚本超时`

现象：点击 AI Voice 即失败。

原因：`audio_init_*.sh` 在部分会话下可能阻塞。

修复：

1. `initAudio()` 使用 `timeout 8s`
2. 初始化失败改为“告警，不阻断流程”，继续录音尝试

### 14.4 日志噪声干扰

现象：终端持续输出 `Monitor::on_timer_timeout called (...)`。

修复：移除高频 `printf`，减少 I/O 干扰和调试噪声。

## 15. 当前验证结论（2026-03-10）

1. `AI Voice` 已可命中 Qt 控件（用户反馈：命中成功）。
2. 语义链路为：Whisper 文本正确 -> Qwen JSON -> Qt 分发执行。
3. 在异常路径下，状态栏可直接看到 `STT` 与 `LLM JSON`，便于定位。
4. 本阶段无需额外训练模型即可满足“命中控件”目标。

## 16. 命中演示记录模板（验收用）

### 16.1 单次记录表

| 时间 | 原始语音 | STT文本 | LLM JSON | action | 命中控件/行为 | 是否成功 | 备注 |
|------|----------|---------|----------|--------|---------------|----------|------|
| 2026-03-10 20:10:01 | 打开音乐 | 打開音樂 | `{"action":"open_music","param":"music"}` | open_music | MusicPlayer 打开并播放 | 是 | - |

### 16.2 批量回归模板

| 序号 | 测试语句 | 期望 action | 实际 action | 期望控件行为 | 实际结果 | 结论 |
|------|----------|-------------|-------------|--------------|----------|------|
| 1 | 打开音乐 | open_music |  | MusicPlayer 显示+播放 |  |  |
| 2 | 播放音乐 | play_music/open_music |  | MusicPlayer 显示+播放 |  |  |
| 3 | 下一首歌 | next_music |  | 下一首 |  |  |
| 4 | 查看天气 | open_weather |  | Weather 显示 |  |  |
| 5 | 打开地图 | open_map |  | Map 显示 |  |  |
| 6 | 关闭摄像头 | close_camera |  | 摄像头关闭 |  |  |
| 7 | 打开设置 | open_settings |  | Setting 显示 |  |  |

### 16.3 失败记录补充项

当某条测试失败，请补充以下字段，便于快速定位：

1. UI状态栏最终文本（原样）
2. `STT: ...` 原文
3. `LLM JSON: ...` 原文
4. 失败阶段：录音 / Whisper / LLM / 分发
5. 是否插耳机（TRRS）与当前音频模式（耳麦/主麦）

## 17. LLM 调用方案对比：进程/线程关系图

三种方案的核心差异在于 llm_demo 的生命周期管理和通信方式。

### 17.1 方案 A：QProcess 每次启动（已弃用）

每次语音指令都 fork 一个新的 llm_demo 进程，用完即销毁。（fork 复制一个进程，）

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt 主进程 (VehicleTerminal)               │
│                                                             │
│  ┌──────────┐    信号/槽     ┌───────────────────────────┐  │
│  │ UI 主线程 │◄────────────►│ QtConcurrent 工作线程      │  │
│  │          │  statusChanged │                           │  │
│  │ onBtn..  │  actionResult  │  runPipeline()            │  │
│  │ dispatch │  sttResult     │    ├─ initAudio()         │  │
│  │          │  finished      │    ├─ recordAudio()       │  │
│  └──────────┘                │    ├─ normalizeAudio()    │  │
│                              │    ├─ runWhisper()        │  │
│                              │    │                      │  │
│                              │    └─ runLlm() ──────┐   │  │
│                              │         QProcess fork │   │  │
│                              └─────────────────────┼─┘  │
│                                                    │     │
└────────────────────────────────────────────────────┼─────┘
                                                     │
          ┌──────────────────────────────────────────┼──────┐
          │            每次新建 llm_demo 子进程       │      │
          │                                          ▼      │
          │  stdin ◄── "prompt\nexit\n"                     │
          │                                                 │
          │  [加载模型 ~10s] → [推理 ~3s] → stdout 输出     │
          │                                                 │
          │  stdout ──► "robot: {\"action\":...}"           │
          │                                                 │
          │  进程退出 (exit)                                 │
          └─────────────────────────────────────────────────┘
               ▲                                    ▲
               │  第1次点击: fork → 加载 → 推理 → 退出
               │  第2次点击: fork → 加载 → 推理 → 退出  (重复加载)
               │  第3次点击: fork → 加载 → 推理 → 退出  (重复加载)
```

- **优点**: 进程隔离，llm_demo 崩溃不影响 Qt；实现简单
- **缺点**: 每次 ~10s 模型加载开销；总延迟 ~13s

---

### 17.2 方案 B：QProcess 常驻进程（✅ 当前方案）

启动时拉起一个 llm_demo 进程并保持存活，后续只通过 stdin/stdout 交互。

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt 主进程 (VehicleTerminal)               │
│                                                             │
│  ┌──────────┐    信号/槽     ┌───────────────────────────┐  │
│  │ UI 主线程 │◄────────────►│ QtConcurrent 工作线程      │  │
│  │          │  statusChanged │                           │  │
│  │ onBtn..  │  actionResult  │  runPipeline()            │  │
│  │ dispatch │  sttResult     │    ├─ initAudio()         │  │
│  │          │  finished      │    ├─ recordAudio()       │  │
│  └──────────┘                │    ├─ normalizeAudio()    │  │
│                              │    ├─ runWhisper()        │  │
│                              │    │                      │  │
│                              │    └─ runLlm() ──────┐   │  │
│                              │         写stdin/读stdout  │  │
│                              └─────────────────────┼─┘  │
│                                                    │     │
│  ┌──────────────────────────────┐       管道通信   │     │
│  │ m_llmProc (QProcess 成员)    │◄────────────────┘     │
│  │ 生命周期 = VoiceAssistant    │                        │
│  └──────────┬───────────────────┘                        │
└─────────────┼────────────────────────────────────────────┘
              │ stdin/stdout 管道
              ▼
┌─────────────────────────────────────────────────────────────┐
│              llm_demo 常驻子进程 (启动时创建一次)             │
│                                                             │
│  [启动时加载模型 ~10s，之后保持存活]                         │
│                                                             │
│  ┌─── 循环等待 stdin ────────────────────────────────────┐  │
│  │                                                       │  │
│  │  stdin ◄── "打开地图的prompt"     ──► 推理 ~3s        │  │
│  │  stdout ──► "robot: {\"action\":\"open_map\"...}"     │  │
│  │                                                       │  │
│  │  stdin ◄── "查看天气的prompt"     ──► 推理 ~3s        │  │
│  │  stdout ──► "robot: {\"action\":\"open_weather\"...}" │  │
│  │                                                       │  │
│  │  stdin ◄── "exit"                 ──► 进程退出        │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
     ▲
     │  第1次点击: 写stdin → 推理 ~3s → 读stdout  (模型已加载)
     │  第2次点击: 写stdin → 推理 ~3s → 读stdout  (无需重加载)
     │  第N次点击: 写stdin → 推理 ~3s → 读stdout  (无需重加载)
```

- **优点**: 首次后无加载延迟(~3s)；保留进程隔离；改动量小
- **缺点**: 需管理进程生命周期（崩溃重启、退出清理）

---

### 17.3 方案 C：LLMController 进程内集成

将 RKLLM SDK 直接编译进 Qt 程序，LLM 在同一进程内运行。

```
┌─────────────────────────────────────────────────────────────┐
│           Qt 主进程 (VehicleTerminal) — 唯一进程             │
│                                                             │
│  ┌──────────┐    信号/槽     ┌───────────────────────────┐  │
│  │ UI 主线程 │◄────────────►│ QtConcurrent 工作线程      │  │
│  │          │  statusChanged │                           │  │
│  │ onBtn..  │  actionResult  │  runPipeline()            │  │
│  │ dispatch │  sttResult     │    ├─ initAudio()         │  │
│  │          │  finished      │    ├─ recordAudio()       │  │
│  └──────────┘                │    ├─ normalizeAudio()    │  │
│                              │    ├─ runWhisper()        │  │
│                              │    │                      │  │
│                              │    └─ runLlm()            │  │
│                              │         │                 │  │
│                              │    ┌────▼──────────────┐  │  │
│                              │    │ LLMController     │  │  │
│                              │    │ (C++ 类, 同进程)   │  │  │
│                              │    │                   │  │  │
│                              │    │ rkllm_run(prompt) │  │  │
│                              │    │    ↓              │  │  │
│                              │    │ 推理 ~3s          │  │  │
│                              │    │    ↓              │  │  │
│                              │    │ return JSON       │  │  │
│                              │    └───────────────────┘  │  │
│                              └───────────────────────────┘  │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │         RKLLM SDK (librkllmrt.so)                     │  │
│  │         模型常驻内存 (~500MB)                          │  │
│  │         启动时加载一次, 进程退出时释放                  │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
│  ⚠ LLM 崩溃 (段错误/OOM) = 整个 Qt 应用崩溃               │
└─────────────────────────────────────────────────────────────┘
     ▲
     │  无子进程，无管道，纯函数调用
     │  第1次点击: rkllm_run() → 推理 ~3s → return  (模型已加载)
     │  第N次点击: rkllm_run() → 推理 ~3s → return  (模型已加载)
```

- **优点**: 最低延迟(~3s)；无进程间通信开销；无需管理子进程
- **缺点**: 需集成 RKLLM SDK 头文件和动态库；LLM 崩溃导致 Qt 一起崩；内存共享无隔离

---

### 17.4 三方案对比总结

```
延迟对比 (从点击 AI Voice 到 LLM 返回结果):

方案A (当前)     ████████████████████████████  ~13s (10s加载 + 3s推理)
方案B (常驻)     ████████                       ~3s  (0s加载 + 3s推理)
方案C (进程内)   ████████                       ~3s  (0s加载 + 3s推理)
                 ├────┼────┼────┼────┼────┼────┤
                 0    2    4    6    8    10   12  秒
```

| 维度 | 方案A: QProcess 每次启动 | 方案B: QProcess 常驻 | 方案C: LLMController |
|------|------------------------|---------------------|---------------------|
| 内存隔离 | ✅ 两个进程，完全隔离 | ✅ 两个进程，完全隔离 | ❌ 同一进程，内存共享 |
| 模型加载 | 每次重新加载 ~10s | 启动时加载一次 | 启动时加载一次 |
| 通信方式 | stdin/stdout 字符串 | stdin/stdout 字符串 | 函数调用+信号槽 |
| 推理延迟 | 10s加载 + 3s推理 | 0加载 + 3s推理 | 0加载 + 3s推理 |
| 代码复杂度 | 简单 | 中等（需管理生命周期） | 稍复杂（需集成SDK） |
| 崩溃影响 | LLM崩了不影响Qt | LLM崩了不影响Qt | LLM崩了Qt也崩 |
| 改动量 | 已弃用 | ✅ 当前已实现 | 新增类+改.pro+集成库 |

## 18. 方案B 实现详情（2026-03-10）

### 18.1 改动概述

将 LLM 调用从「每次 fork 新进程」切换为「常驻进程 + stdin/stdout 管道通信」，消除每次 ~10s 的模型加载延迟。

**改动文件**:
- `voice_assistant.h` — 新增成员变量和方法声明
- `voice_assistant.cpp` — 新增 3 个函数 + 重写 `runLlm()`
- `mainwindow.cpp` — 构造函数中增加 `preInitLlm()` 调用

### 18.2 新增/修改的函数

| 函数 | 位置 | 作用 |
|------|------|------|
| `preInitLlm()` | voice_assistant.cpp | 应用启动时 `QtConcurrent::run` 后台预加载模型，用户首次点击 AI Voice 时模型已就绪 |
| `ensureLlmRunning()` | voice_assistant.cpp | 启动 `llm_demo` 常驻进程，等待 `user:` 就绪信号；若进程已存活则直接返回 true。调用者须持 `m_llmMutex` |
| `shutdownLlm()` | voice_assistant.cpp | 发送 `exit\n` 优雅退出，5s 超时后 kill，析构函数自动调用 |
| `runLlm()` (重写) | voice_assistant.cpp | 不再 fork 新进程；向常驻 stdin 写入单行 prompt，读 stdout 直到 `user:` 出现（标志本轮推理完成），解析 `robot:` 行 JSON |

### 18.3 新增成员变量

```cpp
QProcess *m_llmProc = nullptr;   // 常驻 llm_demo 子进程
QMutex    m_llmMutex;            // 保护 m_llmProc 的并发访问
bool      m_llmReady = false;    // 模型是否已加载就绪
```

### 18.4 llm_demo 通信协议

llm_demo 启动后的输出流:

```
rkllm init start              ← 模型加载中
rkllm init success            ← 模型加载完成
***... menu text ...***
user:                         ← 就绪，等待 stdin 输入 (ensureLlmRunning 的界定符)
```

每轮推理:

```
stdin  ←  "你是车载语音助手...用户说:打开音乐\n"     (单行 prompt)
stdout →  "robot: {\"action\":\"open_music\"...}"     (LLM 回复)
stdout →  "user:"                                     (本轮结束界定符)
```

退出:

```
stdin  ←  "exit\n"            (shutdownLlm 发送)
llm_demo 进程自行退出
```

### 18.5 生命周期管理

```
应用启动
  │
  ├─ MainWindow构造  ─► preInitLlm()
  │                       │
  │                  QtConcurrent::run
  │                       │
  │                  ensureLlmRunning()
  │                       │
  │              启动 taskset f0 llm_demo
  │                       │
  │              等待 "user:" (~10s)
  │                       │
  │               m_llmReady = true ✓
  │
  ├─ 用户点击 [AI Voice]
  │     │
  │     runPipeline() → runLlm()
  │         │
  │    QMutexLocker lock
  │         │
  │    ensureLlmRunning() ─► 已就绪，直接返回 true
  │         │
  │    清空残留输出
  │         │
  │    写入 prompt → 等 "user:" → 解析 robot: JSON  (~3s)
  │
  ├─ 若 llm_demo 意外退出
  │     │
  │    ensureLlmRunning() 检测到 state != Running
  │         │
  │    自动清理 + 重新启动 (~10s 重加载)
  │
  └─ 应用退出
        │
   ~VoiceAssistant() → shutdownLlm()
        │
   写入 "exit\n" → waitForFinished(5s) → delete
```

### 18.6 容错机制

| 场景 | 处理 |
|------|------|
| 模型加载超时 (>30s) | kill 进程, m_llmProc=nullptr, 返回 unknown |
| 推理超时 (>30s) | kill 进程, 清理, 下次调用自动重启 |
| llm_demo 崩溃 | ensureLlmRunning() 检测 state!=Running, 自动重建进程 |
| stdin 写入失败 | kill + 清理, 返回 unknown |
| 并发调用 | QMutex 保护, 同一时刻仅一个线程操作 m_llmProc |

### 18.7 延迟对比实测

```
方案A (旧):  AI Voice → 录音5s → Whisper ~5s → LLM加载10s+推理3s = ~23s 总延迟
方案B (新):  AI Voice → 录音5s → Whisper ~5s → LLM推理3s         = ~13s 总延迟
                                                                    ↑ 省去10s模型加载
```
<!-- 自我总结：
最初每次语音指令都 fork 新的 llm_demo 进程，导致每次都要重新加载模型，延迟约 23s。我将其改造为常驻进程方案，程序启动时在后台线程预加载模型，后续每次推理只通过 stdin/stdout 管道通信，延迟降至 13s。同时实现了进程崩溃自动重启和 QMutex 并发保护，保证系统稳定性 -->





# 知识补缺：
1. fork
- `fork()` 是 Unix/Linux 系统调用，用于创建一个新的子进程。子进程是父进程的复制，拥有相同的代码和数据，但有独立的内存空间。fork 就是"复制进程生孩子"，每次 QProcess 启动新程序都要 fork 一次，代价是要重新加载模型
2. stdin/stdout 管道通信
- 父进程和子进程可以通过管道进行通信，父进程写入数据到子进程的 stdin，子进程处理后通过 stdout 输出结果。
┌─────────────────────────────────────┐
│           Linux 内核                 │
│                                     │
│  ┌──────────────────────────────┐   │
│  │  管道缓冲区（约64KB）         │   │
│  │  ["打开音乐\nexit\n"]        │   │
│  └──────────────────────────────┘   │
│       ▲                    │        │
│       │写入                │读取     │
└───────┼────────────────────┼────────┘
        │                    │
   Qt进程                llm_demo进程
   process.write()       从stdin读
   "打开音乐\nexit\n"    处理后写stdout

---

## 20. 车载系统项目整体框架总览

### 20.1 项目概述

**项目名称**: AI Smart Cockpit — 基于 RK3588S 的 AI 智能车载座舱系统
**硬件平台**: 鲁班猫4 (LubanCat-4, RK3588S), 4GB RAM, 32GB eMMC, Linux 6.1
**GUI 框架**: Qt 5.15 (C++14), 分辨率 1024×600 (MIPI DSI 屏)
**仓库路径**: `/home/cat/chezai/`

### 20.2 仓库目录结构

```
chezai/
├── VehicleTerminal/         ← 主 Qt 应用 (车载座舱 HMI)
│   ├── main.cpp / mainwindow.{h,cpp}   ← 主窗口 + 三栏布局
│   ├── voice_assistant.{h,cpp}          ← AI语音管线 (Whisper+Qwen)
│   ├── camera_thread.{h,cpp}            ← IMX415 MIPI 摄像头采集
│   ├── clock.{h,cpp}                    ← 模拟时钟控件
│   ├── dht11.{h,cpp}                    ← 温湿度传感器线程
│   ├── settingwindow.{h,cpp}            ← 系统设置页
│   ├── speechrecognition.{h,cpp}        ← 百度 ASR (旧, 已弃用)
│   ├── Radar/                           ← 雷达 PPI 显示 + 读取线程
│   │   ├── radar_widget.{h,cpp}
│   │   └── radar_reader.{h,cpp}
│   ├── Music/                           ← 本地+在线音乐播放
│   │   ├── musicplayer.{h,cpp,ui}
│   │   └── searchmusic.{h,cpp,ui}
│   ├── Weather/                         ← 天气预报
│   │   └── weather.{h,cpp,ui}
│   ├── Map/                             ← 百度地图 + GPS
│   │   ├── baidumap.{h,cpp,ui}
│   │   ├── gps.{h,c}
│   │   └── uart.{h,c}
│   ├── Monitor/                         ← 倒车监控 + 环境传感器
│   │   ├── monitor.{h,cpp,ui}
│   │   ├── capture_thread.{h,cpp}
│   │   └── ap3216.{h,cpp}
│   ├── img/ picture/ camera_qt/         ← 资源目录
│   └── VehicleTerminal.pro              ← qmake 工程文件
│
├── ai_voice/                ← AI 模型 & 推理二进制
│   ├── Qwen2.5-0.5B_W8A8_RK3588.rkllm  ← LLM 模型文件
│   ├── demo_Linux_aarch64/llm_demo      ← LLM 推理程序
│   ├── whisper/                         ← Whisper RKNN 模型+demo
│   └── voice/                           ← 音频初始化脚本
│
├── middleware/              ← 雷达信号处理中间件
│   ├── radar_processor.{h,cpp}          ← FFT + CFAR 目标检测
│   └── test_processor.cpp               ← 离线测试
│
├── driver/                  ← Linux 内核驱动
│   ├── dht/                             ← DHT11 GPIO 驱动 (v1~v5)
│   ├── fmcw_radar/                      ← SPI+DMA 雷达驱动
│   └── spi/                             ← SPI 测试
│
├── camera/                  ← V4L2 摄像头工具 & 驱动
│   ├── v4l2_capture.c                   ← 裸 V4L2 抓帧工具
│   └── imx415_driver/                   ← 摄像头 ISP 驱动树
│
├── qt_app/                  ← 独立雷达+蓝牙音乐应用 (精简版)
│   ├── BluetoothAudio.{h,cpp}           ← A2DP Sink 蓝牙音乐
│   └── bt_audio_service.py              ← BlueZ Python 后台服务
│
└── docs/                    ← 学习笔记
    ├── DRM_驱动学习笔记.md
    └── V4L2_驱动学习笔记.md
```

### 20.3 主窗口三栏布局

```
┌────────────────────┬─────────────────────────┬──────────────────┐
│   Left (4份)        │   Center (4份)           │  Right (2份)      │
│                    │                         │                  │
│  ◉ FMCW Radar     │  ◉ 摄像头实时画面         │  ◉ 时间 HH:mm:ss │
│   PPI 360° 极坐标  │   IMX415 1280×720 NV12  │    日期 yyyy-MM-dd│
│   扫描动画         │                         │                  │
│   目标点+标签      │  [开关摄像头] [📸 抓拍]   │  ◉ 温湿度         │
│                    │                         │    Temp: xx.x °C  │
│  [⤢ 全屏雷达]      │  ┌─导航按钮──────────┐   │    Humi: xx.x %   │
│                    │  │Music|Weather|Map  │   │                  │
│                    │  │Monitor|Setting    │   │  ◉ 系统状态       │
│                    │  │  [AI Voice]       │   │    CPU: xx%       │
│                    │  └──────────────────┘   │    NPU: Standby   │
│                    │                         │    Radar: Online  │
│                    │                         │                  │
│                    │                         │  ◉ 模拟时钟        │
└────────────────────┴─────────────────────────┴──────────────────┘
          暗色赛博朋克风格 (#1a1a2e 底色, 青色/紫色渐变按钮)
```

### 20.4 功能模块一览

| 模块 | 文件 | 硬件接口 | 设备节点 | 说明 |
|------|------|---------|---------|------|
| **FMCW 毫米波雷达** | Radar/ + middleware/ + driver/fmcw_radar/ | SPI+DMA (via STM32) | `/dev/fmcw_radar` | IQ→FFT→CFAR 目标检测, PPI 极坐标图, 分类(行人/汽车/路障) |
| **IMX415 摄像头** | camera_thread.cpp | MIPI CSI | `/dev/video11` | V4L2 多平面 API, 1280×720 NV12, 4 buffer 轮转 |
| **温湿度 DHT11** | dht11.cpp + driver/dht/ | GPIO 软件时序 | `/dev/dht11` | QThread 后台读取, 5 字节协议 |
| **AI 语音助手** | voice_assistant.cpp | ES8388 I2S + NPU | PulseAudio card2 | Whisper STT + Qwen2.5 LLM (常驻进程方案B) |
| **音乐播放** | Music/ | ES8388 输出 | PulseAudio | QMediaPlayer + 网易云搜索/下载 |
| **天气预报** | Weather/ | WiFi | — | HTTP API → JSON → UI 展示 |
| **百度地图** | Map/ + GPS UART | UART 串口 | GPS 设备 | NMEA 解析 → 经纬度 → 静态地图 API |
| **倒车监控** | Monitor/ | V4L2 + AP3216C I2C | `/dev/video0` | 摄像头 + 光照/接近/红外传感 + UDP广播 |
| **LLM 语义理解** | voice_assistant.cpp | NPU (RKLLM) | — | Qwen2.5-0.5B, 常驻进程 stdin/stdout 交互 |
| **时钟** | clock.cpp | — | — | QPainter 自绘模拟时钟 |
| **设置** | settingwindow.cpp | — | — | GIF 切换, 系统时间修改 |
| **蓝牙音乐** | qt_app/BluetoothAudio.cpp | USB/内建 BT | BlueZ | A2DP Sink, Python 后台服务 |

### 20.5 系统层次架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          用户界面层 (Qt 5.15)                        │
│                                                                     │
│  MainWindow ─┬─ RadarWidget (PPI)     Music / Weather / Map         │
│              ├─ CameraLabel           Monitor / Setting / Clock     │
│              ├─ VoiceAssistant        SpeechRecognition (Legacy)    │
│              └─ Dht11Display          BluetoothAudio (qt_app)       │
├─────────────────────────────────────────────────────────────────────┤
│                          应用逻辑层                                   │
│                                                                     │
│  VoiceAssistant:                      RadarReader:                   │
│    录音→归一化→Whisper→LLM→分发         读/dev/fmcw_radar→process     │
│  CameraThread:                        MusicPlayer:                   │
│    V4L2 MPLANE→NV12→QImage             QMediaPlayer + 播放列表       │
│  Dht11:                               Weather/Map:                   │
│    读/dev/dht11→温湿度数据               HTTP API→JSON 解析           │
├─────────────────────────────────────────────────────────────────────┤
│                          中间件层                                     │
│                                                                     │
│  RadarProcessor         │  AI 推理引擎 (外部进程)                     │
│    IQ→Hann窗→FFT→CFAR   │    Whisper (rknn_whisper_demo)             │
│    目标检测+测距+测速     │    Qwen2.5 (llm_demo, 常驻进程)            │
├─────────────────────────────────────────────────────────────────────┤
│                          系统服务层                                    │
│                                                                     │
│  PulseAudio (录音/播放)  │  BlueZ (蓝牙A2DP)  │  V4L2 框架           │
│  ALSA (amixer 控制)      │  NetworkManager     │  DRM/KMS (显示)      │
├─────────────────────────────────────────────────────────────────────┤
│                          内核驱动层                                    │
│                                                                     │
│  fmcw_radar.ko          │  dht11.ko           │  imx415 MIPI 驱动    │
│  (SPI+DMA+环形缓冲)      │  (GPIO 软件时序)     │  (ISP Pipeline)      │
│                          │                     │                      │
│  ES8388 I2S 音频驱动     │  AP3216C I2C 驱动   │  GPS UART TTY 驱动   │
├─────────────────────────────────────────────────────────────────────┤
│                          硬件层 (RK3588S SoC)                        │
│                                                                     │
│  CPU: 4×A76 + 4×A55     │  NPU: 6TOPS (RKNN)  │  GPU: Mali-G610     │
│  SPI ← STM32 (雷达)     │  MIPI CSI ← IMX415  │  MIPI DSI → 屏幕    │
│  I2S ← ES8388 (音频)    │  GPIO ← DHT11       │  I2C ← AP3216C      │
│  UART ← GPS 模块        │  USB ← 蓝牙适配器    │  eMMC / DDR4        │
└─────────────────────────────────────────────────────────────────────┘
```

### 20.6 线程与进程架构

```
VehicleTerminal 主进程 (PID xxx)
│
├── UI 主线程 (Qt EventLoop)
│     ├── 信号/槽分发 (statusChanged, actionResult, frameReady, ...)
│     ├── QPainter 绘制 (RadarWidget, Clock)
│     └── QNetworkAccessManager (Weather, Map, SearchMusic)
│
├── QThread: Dht11 采集线程
│     └── 循环读 /dev/dht11 → emit updateDht11Data()
│
├── QThread: RadarReader 雷达线程
│     └── 循环读 /dev/fmcw_radar → RadarProcessor::process() → emit newFrame()
│
├── QThread: CameraThread 摄像头线程
│     └── V4L2 DQBUF/QBUF 循环 → NV12→RGB → emit frameReady(QImage)
│
├── QtConcurrent::run: 语音管线工作线程 (按需)
│     └── recordAudio → normalizeAudio → runWhisper → runLlm → emit actionResult
│
├── QProcess: llm_demo 常驻子进程 (方案B)          ← 应用启动时 preInitLlm() 拉起
│     └── 模型加载一次, stdin/stdout 管道交互, 进程绑定大核 (taskset f0)
│
├── QProcess: Whisper 子进程 (按需, 每次语音指令临时启动)
│     └── rknn_whisper_demo → stdout 输出识别文本
│
└── QProcess: parecord 子进程 (按需, 录音时启动)
      └── timeout N parecord → WAV 文件
```

### 20.7 AI 语音交互完整链路

```
用户点击 [AI Voice]
       │
       ▼
┌── 录音阶段 ──────────────────────────────────────────────────┐
│  Jack 检测 (numid=37) → 选择麦克风 (耳麦 Line1 / 板载 Line2) │
│  PulseAudio 设端口+增益 → parecord 录 5s → WAV 文件          │
│  sox highpass 200Hz + norm -3dB (去风扇噪声+归一化)           │
└──────────────────────────────────────────────────────────────┘
       │ /tmp/voice_qt_input.wav
       ▼
┌── STT 阶段 ──────────────────────────────────────────────────┐
│  rknn_whisper_demo (base 模型, NPU 加速, ~5s)                │
│  输出: 中文识别文本 (可能繁体)                                 │
└──────────────────────────────────────────────────────────────┘
       │ "打開音樂"
       ▼
┌── LLM 阶段 (方案B 常驻进程) ─────────────────────────────────┐
│  向 llm_demo stdin 写入单行 prompt (含 action 枚举约束)       │
│  读 stdout 直到 "user:" 界定符出现 (~3s)                      │
│  解析 "robot:" 行 → JSON {"action":"open_music","param":""}  │
└──────────────────────────────────────────────────────────────┘
       │ {"action":"open_music","param":""}
       ▼
┌── 后处理 ────────────────────────────────────────────────────┐
│  aliasMap 别名归一化 (set_name→open_settings, play→play_music)│
│  若 unknown → fallbackActionFromText() 关键词兜底             │
└──────────────────────────────────────────────────────────────┘
       │ action = "open_music"
       ▼
┌── 分发执行 ──────────────────────────────────────────────────┐
│  dispatchVoiceAction() → emit SendCommandToMusic(SHOW+PLAY)  │
│  MusicPlayer 窗口弹出并播放                                   │
└──────────────────────────────────────────────────────────────┘
```

### 20.8 外部依赖汇总

| 依赖 | 用途 |
|------|------|
| Qt 5.15 (core gui multimedia network concurrent widgets) | 全部 GUI 与多媒体 |
| V4L2 (linux/videodev2.h) | 摄像头帧采集 |
| PulseAudio (parecord, pactl) | 音频录制/增益/路由 |
| ALSA (amixer) | ES8388 声卡硬件控制 |
| sox | 音频高通滤波+归一化 |
| RKNN Whisper demo | NPU 加速 STT |
| RKLLM (llm_demo) | NPU 加速 LLM 推理 |
| 百度地图静态图 API | 地图显示 |
| 网易云音乐 API | 在线歌曲搜索/下载 |
| BlueZ + PulseAudio BT | 蓝牙 A2DP 音乐 (qt_app) |

### 20.9 项目演进历程

```
IMX6ULL 原始版本 ──────────────────────────────────────────────────►
   │  单平面 V4L2 / QAudioRecorder / 百度在线 ASR / 无 AI
   │
   ▼ 移植到 RK3588S
LubanCat-4 基础移植 ───────────────────────────────────────────────►
   │  多平面 V4L2 (MPLANE) / ES8388 声卡适配 / PulseAudio
   │  FMCW 雷达 SPI 驱动 + FFT/CFAR 信号处理
   │
   ▼ AI 能力增强
端侧离线 AI 语音 ──────────────────────────────────────────────────►
   │  Whisper base (RKNN NPU) + Qwen2.5-0.5B (RKLLM NPU)
   │  方案A: 每次 fork llm_demo (~13s)
   │
   ▼ 性能优化
方案B: LLM 常驻进程 ──────────────────────────────────────────────►
      预加载模型, stdin/stdout 管道交互, 推理仅 ~3s
      耳机 Jack 动态检测, 别名映射, 关键词兜底
   │
   ▼ 资源隔离优化
CPU 核心分区 (当前) ───────────────────────────────────────────────►
      cpuset: UI→A55 小核, AI→A76 大核
      Whisper/LLM 均 taskset f0 绑定大核
      大核 performance 调频策略
```

## 21. 优化：CPU 核心分区与亲和性绑定（2026-03-11）

### 21.1 优化背景

RK3588S 采用 big.LITTLE 架构：

| 核心 | 型号 | CPU 编号 | 定位 |
|------|------|---------|------|
| 小核 | Cortex-A55 | 0, 1, 2, 3 | 低功耗，适合 UI / 传感器轮询 |
| 大核 | Cortex-A76 | 4, 5, 6, 7 | 高性能，适合 AI 推理 (Whisper / LLM) |

优化前问题：
- 程序**无 CPU 亲和性设置**，所有线程由内核调度器自由迁移
- AI 推理可能被调度到小核 A55 上，导致推理变慢
- 只有 LLM 有 `taskset f0` 绑定大核，**Whisper 没有任何绑定**

### 21.2 改动内容

#### 21.2.1 Whisper 绑定大核 (代码改动)

**文件**: `voice_assistant.cpp` — `runWhisper()` 函数

```diff
- proc.start("timeout",
-     QStringList()
-         << "20s"
+ proc.start("taskset",
+     QStringList()
+         << "f0"
+         << "timeout"
+         << "20s"
          << (m_whisperDir + "/rknn_whisper_demo")
          << "./model/whisper_encoder_base_20s.rknn"
          << "./model/whisper_decoder_base_20s.rknn"
          << "zh"
          << m_audioTmp);
```

`taskset f0` 的含义：`f0` = 二进制 `11110000` = CPU 4/5/6/7（大核 A76）。

改动后 Whisper 和 LLM 都强制运行在大核上，彻底避免被调度到小核。

#### 21.2.2 cpuset 隔离启动脚本

**文件**: `start_vehicle.sh`（新建, 位于 `/home/cat/chezai/start_vehicle.sh`）

利用 Linux cgroup v1 cpuset 子系统实现进程级 CPU 分区：

```
┌──────────────────────────────────────────────────────┐
│               RK3588S 8核 CPU                        │
│                                                      │
│  ┌─── ui cpuset ───┐    ┌─── ai cpuset ───────────┐ │
│  │ CPU 0  (A55)    │    │ CPU 4  (A76)             │ │
│  │ CPU 1  (A55)    │    │ CPU 5  (A76)             │ │
│  │ CPU 2  (A55)    │    │ CPU 6  (A76)             │ │
│  │ CPU 3  (A55)    │    │ CPU 7  (A76)             │ │
│  │                 │    │                           │ │
│  │ Qt UI 主线程    │    │ Whisper (taskset f0)      │ │
│  │ DHT11 线程      │    │ LLM llm_demo (taskset f0) │ │
│  │ Radar 线程      │    │                           │ │
│  │ Camera 线程     │    │                           │ │
│  └─────────────────┘    └───────────────────────────┘ │
└──────────────────────────────────────────────────────┘
```

脚本核心操作：

```bash
# 创建 cpuset 分组
mkdir -p /sys/fs/cgroup/cpuset/ui
echo 0-3 > /sys/fs/cgroup/cpuset/ui/cpuset.cpus    # 小核
echo 0   > /sys/fs/cgroup/cpuset/ui/cpuset.mems

mkdir -p /sys/fs/cgroup/cpuset/ai
echo 4-7 > /sys/fs/cgroup/cpuset/ai/cpuset.cpus    # 大核
echo 0   > /sys/fs/cgroup/cpuset/ai/cpuset.mems

# 大核强制 performance 调频策略
for cpu in 4 5 6 7; do
    echo performance > /sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor
done

# 启动 Qt 应用并放入 ui 组
./VehicleTerminal &
echo $! > /sys/fs/cgroup/cpuset/ui/tasks
```

使用方式：

```bash
sudo bash /home/cat/chezai/start_vehicle.sh
```

### 21.3 亲和性覆盖原理

> **Q: cpuset 把 Qt 限制在 CPU 0-3，但 Whisper/LLM 子进程是 Qt fork 出来的，不也会被限制在 0-3 吗？**
>
> A: 不会。`taskset f0` 通过 `sched_setaffinity()` 系统调用设置 CPU 亲和性掩码。虽然子进程继承父进程的 cpuset，但 `taskset` 会在 `exec()` 前覆盖亲和性。Linux 内核中 `sched_setaffinity` 的优先级高于 cpuset 约束（在特权模式下），而且 `taskset` 作为命令行包装器会在 `exec` 目标程序前调用 `sched_setaffinity`。
>
> 实际效果：Qt 主进程及其线程（UI、DHT11、Radar、Camera）被 cpuset 限制在 A55 小核；Whisper/LLM 子进程通过 taskset 跳到 A76 大核执行。

### 21.4 预期收益

| 指标 | 优化前 | 优化后 |
|------|--------|--------|
| Whisper CPU 调度 | 可能迁移到 A55 慢核 | 锁定 A76 大核 |
| LLM CPU 调度 | 已有 taskset f0 | 不变 |
| UI 响应 | AI 推理可能抢占 UI 核心 | UI 独占 A55，AI 独占 A76，互不干扰 |
| 大核频率 | ondemand (按需) | performance (满频) |

### 21.5 验证方法

```bash
# 1. 启动后查看进程 CPU 亲和性
taskset -p $(pidof VehicleTerminal)   # 应显示 f (CPU 0-3)
taskset -p $(pidof llm_demo)          # 应显示 f0 (CPU 4-7)

# 2. 实时监控各核心负载
htop   # 按 F2 → Columns → 添加 PROCESSOR 列

# 3. 查看 cpuset 组中的进程
cat /sys/fs/cgroup/cpuset/ui/tasks    # 应包含 VehicleTerminal PID
cat /sys/fs/cgroup/cpuset/ai/tasks    # AI 子进程 PID (如果也手动加入)
```

<!-- 自我总结：
RK3588S 有 4 大核 4 小核，优化前所有线程由内核自由调度，AI 推理可能跑到慢核上。我做了两件事：(1) 给 Whisper 加上 taskset f0 绑定大核，和 LLM 一样确保 AI 推理只在 A76 上执行；(2) 创建 cpuset 启动脚本，将 Qt UI 主进程限制在 A55 小核、大核设为 performance 调频。这样 UI 和 AI 推理在物理上隔离到不同核心组，互不干扰。
-->



<!-- 知识补缺 -->
管道是操作系统提供的一种进程间通信机制，本质是内核里的一块缓冲区。

## 22. DHT11 内核驱动开发（2026-03-11）

### 22.1 背景

**之前的问题**:
- Qt 应用 `dht11.cpp` 尝试 `open("/dev/dht11")`，但没有内核驱动创建这个设备
- 编译时 `DISABLE_HARDWARE` 宏生效，直接走模拟数据分支 (固定 25.0°C / 28.0%)
- 用户空间有 5 个测试程序 (v1~v5)，但都是独立的，不提供 `/dev/dht11` 设备接口

**目标**: 编写一个内核驱动模块 (`dht11_drv.ko`)，创建 `/dev/dht11` 字符设备，让 Qt 应用直接读取真实传感器数据。

### 22.2 用户空间 vs 内核空间的差异

| 对比维度 | 用户空间 (v5) | 内核驱动 |
|----------|--------------|---------|
| GPIO API | `gpiod_line_get_value()` (libgpiod) | `gpio_get_value()` (内核 GPIO 子系统) |
| 延时 | `usleep(20000)` | `msleep(20)` / `udelay(40)` |
| 精确计时 | `clock_gettime(CLOCK_MONOTONIC)` | `ktime_get_ns()` |
| 防调度干扰 | `SCHED_FIFO` prio=99 | `local_irq_save()` 关中断 |
| 输出 | `printf()` | `pr_info()` / `pr_err()` |
| 数据传递 | 直接 `printf` | `copy_to_user()` (内核→用户空间) |

核心时序逻辑完全一样，只是换了一套 API。

### 22.3 驱动架构

```
┌───────────────────────────────────────────────────┐
│  用户态 Qt 应用 (dht11.cpp)                        │
│     fd = open("/dev/dht11", O_RDWR);              │
│     read(fd, buf, 5);   ← 5字节: 湿整 湿小 温整 温小 校验 │
└────────────────┬──────────────────────────────────┘
                 │ read() 系统调用
                 ▼
┌───────────────────────────────────────────────────┐
│  内核态 dht11_drv.ko                               │
│                                                   │
│  misc_register("/dev/dht11")                      │
│     └─ .read = dht11_fops_read()                  │
│          ├─ mutex_lock (防并发)                     │
│          ├─ dht11_read_sensor()                    │
│          │    ├─ gpio_direction_output → 拉低 20ms │
│          │    ├─ gpio_direction_input  → 等应答     │
│          │    ├─ local_irq_save  ← 🔒 关中断       │
│          │    ├─ 采集 40 bit (ktime 测量高电平)     │
│          │    ├─ local_irq_restore ← 🔓 开中断     │
│          │    └─ 校验和验证                         │
│          └─ copy_to_user(buf, data, 5)             │
│                                                   │
│  GPIO: GPIO3_A6 = bank3*32 + 0*8 + 6 = GPIO 102  │
└───────────────────────────────────────────────────┘
```

### 22.4 关键设计决策

#### 22.4.1 为什么用 misc device？

```c
static struct miscdevice dht11_misc = {
    .minor = MISC_DYNAMIC_MINOR,    // 内核自动分配次设备号
    .name  = "dht11",               // 自动创建 /dev/dht11
    .fops  = &dht11_fops,
};
```

相比手动 `register_chrdev()` + `class_create()` + `device_create()`:
- 代码量减少一半
- 不需要手动 `mknod` 创建设备节点
- 适合这种单一功能的简单传感器

#### 22.4.2 为什么读数据时要关中断？

```c
local_irq_save(irq_flags);      // 关中断

for (i = 0; i < 40; i++) {
    wait_for_level(1, TIMEOUT_US);
    t_start = ktime_get_ns();
    wait_for_level(0, TIMEOUT_US);
    if ((ktime_get_ns() - t_start) > 40000)   // 40us
        data[i/8] |= 1;                        // bit = 1
}

local_irq_restore(irq_flags);   // 恢复中断
```

DHT11 用高电平持续时间区分 bit 0/1:
- 26~28us → bit 0
- 70us → bit 1

如果测量 26us 高电平时来了个中断 (比如定时器) 耗时 50us，测出来就是 76us → 误判为 bit 1。关中断保证测量不被打断。

这和 v5 用 `SCHED_FIFO prio=99` 是同一个思路，但关中断比实时调度更彻底。

#### 22.4.3 GPIO 编号计算

```
RK3588S GPIO 命名规则:
  GPIO{bank}_{group}{pin}
  GPIO3_A6 → bank=3, group=A(=0), pin=6

编号 = bank × 32 + group × 8 + pin
     = 3 × 32 + 0 × 8 + 6
     = 102

验证: gpiochip3 base=96, offset=6 → 96+6=102 ✓
```

#### 22.4.4 为什么用 mutex？

```c
static DEFINE_MUTEX(dht11_lock);

// 在 read 函数中:
mutex_lock_interruptible(&dht11_lock);
// ... 读传感器 ...
mutex_unlock(&dht11_lock);
```

DHT11 是单线设备，同一时刻只能有一个主机起始信号。如果 Qt 的 DHT11 线程和其他进程同时 read，两个起始信号会冲突。mutex 保证串行访问。

### 22.5 改动文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `driver/dht/dht11_drv.c` | **新建** | 内核驱动源码 (~230 行)，含 ioremap 设置 GPIO 上拉 |
| `driver/dht/Makefile` | **新建** | 内核模块编译 Makefile |
| `/etc/udev/rules.d/99-dht11.rules` | **新建** | 自动设置 `/dev/dht11` 权限 666 |
| `/etc/modules-load.d/dht11.conf` | **新建** | 开机自动加载 dht11_drv 模块 |
| `/lib/modules/.../extra/dht11_drv.ko` | **部署** | depmod 注册到系统模块索引 |
| `VehicleTerminal/dht11.cpp` | **修改** | 去掉 `DISABLE_HARDWARE` 条件，读取间隔 0.5s→2s |

### 22.6 Qt 端接口对接

dht11.cpp 原来的条件:
```diff
- #if (defined(__arm__) || defined(__aarch64__)) && !defined(DISABLE_HARDWARE)
+ #if defined(__arm__) || defined(__aarch64__)
```

去掉 `DISABLE_HARDWARE` 检查（仅针对 DHT11），别的硬件 (GPS、AP3216C) 继续受该宏控制。

Qt 端读取逻辑 **不需要任何改动**，因为驱动接口和原设计完全匹配:
```cpp
int fd = open("/dev/dht11", O_RDWR);
read(fd, date, 5);    // date[0]=湿度整, [1]=湿度小, [2]=温度整, [3]=温度小, [4]=校验
```

### 22.7 编译与使用

```bash
# 编译驱动
cd ~/chezai/driver/dht
make

# 手动加载测试
sudo insmod dht11_drv.ko
# dmesg 输出:
#   dht11: GPIO3_A6 pull-up configured via IOC register
#   dht11: driver loaded (GPIO 102 → /dev/dht11)

# 测试读取
xxd -l 5 /dev/dht11
# 输出: 3000 1202 44  →  湿度48.0%, 温度18.2°C, 校验68 ✓

# 卸载驱动
sudo rmmod dht11_drv
```

### 22.7.1 开机自动加载配置

`insmod` 手动加载的内核模块重启后会丢失。配置开机自动加载需要三步：

```bash
# 1. 复制 .ko 到系统模块目录 + 更新索引
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp dht11_drv.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
# 备注：lib/modules是内核模块的标准目录结构，depmod 会扫描 extra/ 目录并更新模块依赖索引，确保 modprobe 能找到 dht11_drv 模块。
# depmod -a 的作用是重新生成模块依赖关系和符号表，确保系统能够正确识别和加载新的 dht11_drv 模块。

# 2. 配置 systemd 开机自动加载
echo "dht11_drv" | sudo tee /etc/modules-load.d/dht11.conf
# 备注：系统启动时，systemd-modules-load.service 会读这个目录下所有 .conf 文件，对每个模块名执行 modprobe 。dht11.conf 里就一行文字：dht11_drv
# 3. udev 规则 (之前已创建) — 自动设置权限
# /etc/udev/rules.d/99-dht11.rules:
# KERNEL=="dht11", MODE="0666"
```

重启后自动生效链路：

```
系统启动
  │
  ├─ systemd-modules-load.service
  │    └─ 读 /etc/modules-load.d/dht11.conf
  │    └─ modprobe dht11_drv
  │         └─ 从 /lib/modules/.../extra/dht11_drv.ko 加载
  │         └─ dht11_init() → ioremap 设上拉 → misc_register
  │         └─ /dev/dht11 出现
  │
  └─ udev
       └─ 匹配 KERNEL=="dht11"
       └─ chmod 666 /dev/dht11
       └─ 普通用户可读 ✓
```

> **注意**: 重新编译 .ko 后，需要重新执行步骤 1（`cp` + `depmod -a`）更新系统中的副本。

### 22.8 测试结果

| 读取次数 | 原始字节 | 湿度 | 温度 | 校验 |
|---------|---------|------|------|------|
| 1 | `26 00 13 00 39` | 38.0% | 19.0°C | ✅ |
| 2 | `26 00 13 00 39` | 38.0% | 19.0°C | ✅ |
| 3 | `26 00 13 00 39` | 38.0% | 19.0°C | ✅ |

3 次连续读取全部成功，数据一致，与 v5 用户空间测试结果完全吻合。

### 22.9 驱动代码核心结构 (精简)

```c
/* 等待 GPIO 变为指定电平 (忙等 + 纳秒计时) */
static int wait_for_level(int level, int timeout_us) {
    u64 deadline = ktime_get_ns() + (u64)timeout_us * 1000;
    while (gpio_get_value(DHT11_GPIO) != level)
        if (ktime_get_ns() > deadline) return -1;
    return 0;
}

/* 读传感器: 起始信号 → 等应答 → 关中断采集 40 bit → 校验 */
static int dht11_read_sensor(unsigned char data[5]) {
    gpio_direction_output(DHT11_GPIO, 0); msleep(20);   // 起始信号
    gpio_set_value(DHT11_GPIO, 1); udelay(30);
    gpio_direction_input(DHT11_GPIO);
    // 等待应答: 低→高→低
    wait_for_level(0, 200); wait_for_level(1, 200); wait_for_level(0, 200);

    local_irq_save(flags);                               // 🔒 关中断
    for (i = 0; i < 40; i++) {
        wait_for_level(1, 200);
        t = ktime_get_ns();
        wait_for_level(0, 200);
        data[i/8] <<= 1;
        if ((ktime_get_ns() - t) > 40000) data[i/8] |= 1;
    }
    local_irq_restore(flags);                            // 🔓 开中断
    // 校验和验证...
}

/* read 系统调用 → 读传感器 → copy_to_user */
static ssize_t dht11_fops_read(...) {
    mutex_lock(&dht11_lock);
    dht11_read_sensor(data);    // 最多重试 3 次
    mutex_unlock(&dht11_lock);
    copy_to_user(buf, data, 5);
}

/* misc device: 自动创建 /dev/dht11 */
static struct miscdevice dht11_misc = {
    .minor = MISC_DYNAMIC_MINOR, .name = "dht11", .fops = &dht11_fops,
};
module_init → gpio_request + misc_register
module_exit → misc_deregister + gpio_free
```

<!-- 自我总结：
之前 DHT11 只有用户空间测试程序 (v1~v5)，Qt 应用因为没有 /dev/dht11 设备节点一直在用模拟数据。我写了一个内核驱动模块 dht11_drv.ko，用 misc device 自动创建 /dev/dht11，实现了 read 系统调用读取传感器数据。核心是把 v5 的 GPIO 时序逻辑搬到内核空间：用 gpio_get_value 替代 libgpiod，用 ktime_get_ns 测量高电平持续时间判断 bit 0/1，用 local_irq_save 关中断保证 us 级时序测量精度。同时修改了 Qt 端 dht11.cpp 去掉 DISABLE_HARDWARE 限制，让它始终尝试读真实硬件。
-->

## 23. DHT11 GPIO 上下拉调试实录（2026-03-11）

### 23.1 故障现象

驱动首次加载后读取成功 (38.0% / 19.0°C)，但 Qt 应用运行一段时间后 **DHT11 不再应答**：

```
dmesg:
[xxx] dht11: no response (low)    ← 反复出现
[xxx] dht11: no response (low)
```

甚至连用户空间 v5 测试程序也失败了：

```bash
$ sudo ./test_dht11_v5
DHT11 test v5 on gpiochip3 line 6
Attempt 1: bit 0 timeout
Attempt 2: bit 0 timeout
...
Failed.
```

### 23.2 定位过程

#### 第一步：排除软件问题

v5 用户空间程序和内核驱动用的是两套完全不同的 GPIO API，但**两个都失败了**。说明不是驱动代码的 bug，而是**硬件层面**出了问题。

#### 第二步：检查 GPIO 电平

```bash
$ sudo cat /sys/class/gpio/gpio102/value
1    ← 总线空闲时是高电平，上拉电阻在工作
```

GPIO 能读到高电平，说明传感器的外部上拉电阻没坏。但传感器为什么不应答起始信号？

#### 第三步：查看 pinctrl 配置

```bash
$ sudo cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinconf-pins | grep "pin 102"
pin 102 (gpio3-6): input bias pull down (1 ohms), output drive strength (8 mA)
                    ^^^^^^^^^^^^^^^^
                    这里是关键！
```

**发现问题**: GPIO3_A6 配置的是 **pull-down (下拉)**，不是 pull-up！

#### 第四步：分析根因

DHT11 单线协议的起始信号时序：

```
主机:  ──┐         ┌──┐
         │  20ms   │  │ 30us      ← 主机先拉低再拉高
         └─────────┘  └──────── → 切换为输入模式, 等 DHT11 应答

                                  ← 这里 GPIO 变成输入, 电平由 pull 决定
                                  ← 如果 pull-down: 总线被拉低 → DHT11 以为主机还没释放
                                  ← 如果 pull-up:   总线保持高 → DHT11 看到高电平, 开始应答
```

问题链：
1. 驱动发完起始信号后调用 `gpio_direction_input()` 释放总线
2. GPIO 配置为 pull-down → 总线被 SoC 内部下拉电阻拉低
3. DHT11 看到总线一直是低电平 → 认为主机还没释放 → 不应答
4. 驱动等不到低电平应答 → 报 `no response (low)`

> **为什么第一次能成功？**
> 第一次 `gpio_direction_output(1)` 强推高电平后快速切到输入，GPIO 引脚的寄生电容上还残留高电平，在 DHT11 应答窗口（~20us）内电压还没被下拉电阻拉到低于阈值，所以 DHT11 来得及看到"释放"并应答。但后续读取时，GPIO 稳定在输入模式，下拉阻值持续生效，成功率迅速归零。

### 23.3 修复方案演进

#### 方案一：gpiod_set_config（❌ 失败）

最初尝试通过 Linux GPIO 子系统的 `gpiod_set_config` API 设置上拉：

```c
struct gpio_desc *desc = gpio_to_desc(DHT11_GPIO);
gpiod_set_config(desc, pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1));
```

**结果**: 编译通过，运行无报错，但 `pinconf-pins` 显示 **仍然是 pull-down**。

**原因分析**: RK3588S 的 GPIO pull 控制不在 GPIO 控制器内部，而是在独立的 **IOC (IO Controller)** 模块中。`gpiod_set_config` 最终调用的是 `pinctrl-rockchip` 驱动的 `rockchip_set_pull`，但在运行时动态修改可能受 pinctrl state 管理机制约束，对已经分配给 GPIO 子系统的引脚不一定生效。

#### 方案二：ioremap 直接写 IOC 寄存器（✅ 成功）

既然标准 API 不生效，直接操作硬件寄存器：

**第一步：查找寄存器地址**

RK3588S 的 GPIO pull 控制寄存器分布在多个 IOC 模块中：

```
GPIO3_A6 属于 VCCIO3_5_IOC (供电域 3_5)
基址: 0xFD5F8000
GPIO3A_P 寄存器偏移: 0x0090
→ 物理地址: 0xFD5F8090
```

**第二步：确定 bit 位**

```
GPIO3A_P 寄存器 (32-bit):
 bit [31:16]: 写使能掩码 (高16位, RK3588 特有的写保护机制)
 bit [15:0]:  各引脚的 pull 配置

 A6 对应 bit[13:12]:
   00 = 浮空 (Z)
   01 = 下拉 (pull-down)  ← 当前状态
   10 = 上拉 (pull-up)    ← 目标状态
   11 = 保留

 要写 bit[13:12], 必须同时设置写使能掩码 bit[29:28] = 11
```

**第三步：驱动中实现**

```c
/* 寄存器地址和 bit 定义 */
#define RK3588_GPIO3A_P_REG   0xFD5F8090
#define GPIO3_A6_PULL_UP      (0x2 << 12)          /* bit[13:12] = 10 */
#define GPIO3_A6_WRITE_EN     (0x3 << (12 + 16))   /* bit[29:28] = 11 */

/* 在 module_init 中执行 (只需设一次) */
static int __init dht11_init(void)
{
    void __iomem *pull_reg;

    /* ... gpio_request ... */

    pull_reg = ioremap(RK3588_GPIO3A_P_REG, 4);
    if (pull_reg) {
        writel(GPIO3_A6_WRITE_EN | GPIO3_A6_PULL_UP, pull_reg);
        iounmap(pull_reg);
        pr_info("dht11: GPIO3_A6 pull-up configured via IOC register\n");
    }

    /* ... misc_register ... */
}
```

**验证**:

```bash
$ sudo insmod dht11_drv.ko
$ dmesg | tail
[277.907272] dht11: GPIO3_A6 pull-up configured via IOC register
[277.907842] dht11: driver loaded (GPIO 102 → /dev/dht11)

$ xxd -l 5 /dev/dht11
00000000: 2400 1300 37     →  湿度 36.0%, 温度 19.0°C ✓

$ for i in 1 2 3; do xxd -l 5 /dev/dht11; sleep 2; done
00000000: 2400 1300 37     ✓
00000000: 2400 1300 37     ✓
00000000: 2400 1300 37     ✓
```

### 23.4 RK3588 写使能掩码机制详解

RK3588 的 IOC/GRF 寄存器采用 **高 16 位写掩码** 机制，这是 Rockchip SoC 的独特设计：

```
        31    28    24    20    16    12     8     4     0
寄存器: [  写使能掩码 (高16位)  ] [  实际配置值 (低16位)  ]

写入规则:
  - 只有高16位中对应 bit=1 的位置, 低16位的值才会被写入
  - 高16位中 bit=0 的位置, 低16位的值被忽略 (寄存器原值保留)

GPIO3_A6 上拉的例子:
  目标: 设置 bit[13:12] = 10 (上拉)
  写使能: bit[29:28] = 11 (允许写 bit[13:12])
  其他位: 不动

  writel(0x30002000, reg)
         ^^^^               → bit[29:28]=11 (写使能)
             ^^^^           → bit[13:12]=10 (上拉)
```

这个机制的好处是：一次 32-bit 写操作可以**原子地**只修改指定 bit，不需要 read-modify-write（避免竞态条件）。如果没有写掩码，你需要：

```c
// ❌ 传统方式 (有竞态风险)
val = readl(reg);       // 1. 读
val &= ~(0x3 << 12);   // 2. 清除
val |= (0x2 << 12);    // 3. 设置
writel(val, reg);       // 4. 写  ← 如果步骤1和4之间有中断修改了其他位, 会被覆盖

// ✅ RK3588 写掩码方式 (原子操作)
writel(0x30002000, reg);  // 一步到位, 只影响 bit[13:12], 其他位不变
```

### 23.5 同时修复: Qt 端读取频率过快

调试中发现另一个问题是 DHT11 被 Qt 应用**过于频繁地读取**导致传感器死锁：

| 参数 | 修复前 | 修复后 |
|------|--------|--------|
| 正常读取间隔 | `usleep(500000)` = 0.5s | `usleep(2000000)` = 2s |
| 失败退避间隔 | 无 (立即重试) | `usleep(3000000)` = 3s |
| DHT11 规格要求 | ≥ 1s | 满足 ✓ |

修复前 Qt 每 0.5 秒就读一次，而 DHT11 规格要求采样间隔 ≥1 秒。在上拉错误 + 快速读取的双重打击下，传感器进入死锁状态——即使用户空间 v5 程序也读不动了，只有断电重启才能恢复。

### 23.6 调试思路总结

```
DHT11 "不应答" 问题的调试路径:

故障: 驱动报 no response (low)
  │
  ├─ 1. v5 用户空间程序也失败
  │     → 排除驱动代码 bug, 是硬件/配置问题
  │
  ├─ 2. GPIO 空闲时读到高电平
  │     → 外部上拉电阻正常, 排除硬件损坏
  │
  ├─ 3. pinconf-pins 显示 pull-down
  │     → 找到根因: SoC 内部下拉与外部上拉对抗
  │     → 主机释放总线后, 内部下拉把电平拉低
  │     → DHT11 以为主机还在拉低, 不应答
  │
  ├─ 4. gpiod_set_config(PIN_CONFIG_BIAS_PULL_UP) 不生效
  │     → 标准 API 受 pinctrl state 管理限制
  │
  └─ 5. ioremap + writel 直接写 IOC 寄存器
        → 找到 GPIO3A_P 寄存器地址 (0xFD5F8090)
        → 理解 RK3588 写使能掩码机制
        → 设置 bit[13:12]=10 (上拉), bit[29:28]=11 (写使能)
        → 问题解决 ✓
```

### 23.7 涉及的技能点

| 技能 | 具体体现 |
|------|---------|
| **GPIO 子系统** | gpio_request / gpio_direction_input / gpio_get_value |
| **pinctrl 调试** | 通过 debugfs 查看 pinconf-pins 发现 pull-down |
| **SoC 寄存器操作** | ioremap + writel 直接写 RK3588 IOC 寄存器 |
| **芯片手册阅读** | 查 TRM 找到 VCCIO3_5_IOC 基址、GPIO3A_P 偏移、bit 定义 |
| **写掩码机制** | 理解 RK3588 高16位写使能掩码的原子写设计 |
| **协议时序分析** | 从 DHT11 起始信号时序推导出上拉缺失的影响 |
| **故障链推理** | pull-down → 总线被拉低 → DHT11 不应答 → 首次靠寄生电容侥幸成功 |

<!-- 自我总结：
DHT11 驱动首次运行正常但后续全部超时，连用户空间程序也失败。通过 pinconf-pins 调试接口发现 GPIO3_A6 被配置为 pull-down，导致主机释放总线后内部下拉把电平拉低，DHT11 以为主机还没释放就不应答。第一次能成功是因为 GPIO 寄生电容残留高电平。标准 gpiod_set_config API 设置上拉不生效（受 pinctrl state 管理限制），最终通过 ioremap 直接写 RK3588 IOC 寄存器 (0xFD5F8090) 解决，利用 RK3588 特有的高16位写使能掩码机制原子写入 pull-up 配置。同时修复了 Qt 端读取间隔过短（0.5s→2s）防止传感器死锁。
-->



（ai勿动）
<!-- 知识补缺 -->
管道就是操作系统提供的"传话筒"，让两个进程可以互相发送字符串数据。QProcess 帮你

---

## §24 音乐播放器切歌卡顿修复 — QThread 工作线程方案

### 24.1 问题现象

切换歌曲时整个 UI 界面冻结 200~1000ms，包括进度条、触摸响应、温湿度刷新全部停摆。

### 24.2 根因分析

QMediaPlayer 底层走 GStreamer，`stop()` 拆管线 + `play()` 建管线是**同步阻塞操作**，在 RK3588S 上耗时数百毫秒。原代码在 UI 主线程直接调用：

```
UI 事件循环 (单线程):
  on_pBtn_Next_clicked()
    ├─ musicPlayer->stop()    ← GStreamer 拆管线, 阻塞 200~500ms
    ├─ setCurrentIndex()
    └─ musicPlayer->play()    ← GStreamer 建管线, 阻塞 200~500ms
    
  ⚠ 这段时间 UI 完全冻结
```

此外还有双重触发问题：`setCurrentRow()` 会触发 `currentRowChanged` 信号 → 又执行一次 `stop()` + `play()`，导致 GStreamer 管线被拆建两次。

### 24.3 解决方案：moveToThread 工作线程

把 QMediaPlayer 搬到独立工作线程，UI 线程只发信号、收状态。

```
改造后架构:

UI 线程 (主线程)                    播放工作线程
┌────────────────────┐            ┌────────────────────┐
│ 按钮、列表、进度条    │  ─信号→   │ MusicWorker         │
│ MusicPlayer (UI壳)  │            │ QMediaPlayer        │
│                    │            │ QMediaPlaylist      │
│ 用户点击 "下一首"    │──sigPlay→ │                    │
│ emit sigPlay(idx)  │            │ doPlay():           │
│ 立即返回, 继续刷 UI  │            │   stop()  ← 阻塞    │
│                    │            │   setIdx()          │
│ ←positionChanged───│──信号回来─ │   play()  ← 阻塞    │
│ 更新进度条          │            │                    │
└────────────────────┘            └────────────────────┘
```

#### 核心概念：进程 vs 线程

| | 进程 | 线程 |
|--|------|------|
| 内存 | 各自独立的虚拟地址空间（内核 MMU 页表隔离） | 共享同一进程的地址空间 |
| 通信 | 管道、共享内存、socket 等 IPC（需序列化） | 直接访问同一变量、Qt 信号槽自动跨线程 |
| 崩溃隔离 | 一个崩溃不影响另一个 | 一个线程崩溃会带走整个进程 |
| 开销 | fork + exec 创建慢，上下文切换重 | 创建快，切换轻量 |
| 本项目例子 | VehicleTerminal ↔ llm_demo（QProcess 管道通信） | MusicWorker（moveToThread + 信号槽） |

选择线程而非进程的原因：
- QMediaPlayer 是 Qt 自己的类，天然支持 `moveToThread`
- 每秒数十次 `positionChanged` 信号用管道序列化开销大，信号槽零开销
- 不需要崩溃隔离（音乐播放器不会段错误）

#### 为什么 LLM 用了独立进程

`llm_demo` 是第三方预编译的二进制文件，无法编译进 Qt 项目，只能用 `QProcess` 启动为独立进程，通过 stdin/stdout 管道通信。

### 24.4 实现细节

#### 24.4.1 新增文件

**Music/music_worker.h + music_worker.cpp**：

```cpp
class MusicWorker : public QObject   // 注意: 继承 QObject, 不是 QThread
{
    Q_OBJECT
public:
    explicit MusicWorker(QObject *parent = nullptr);
    
    // 播放列表管理 (moveToThread 前由主线程调用)
    bool addMedia(const QString &filePath);
    bool isEmpty() const;
    int  nextIndex(int steps = 1) const;
    int  previousIndex(int steps = 1) const;

public slots:
    // 以下槽函数在工作线程中执行
    void doPlay(int index);     // stop → setCurrentIndex → play
    void doStop();
    void doPause();
    void doResume();
    void doSetVolume(int vol);
    void doSeek(qint64 positionMs);
    void doSetPlaybackMode(int mode);

signals:
    // 发回 UI 线程
    void durationChanged(qint64 durationMs);
    void positionChanged(qint64 positionMs);
    void currentIndexChanged(int index);    // 自动切歌时通知

private:
    QMediaPlayer   *m_player;
    QMediaPlaylist *m_playlist;
};
```

#### 24.4.2 MusicPlayer 构造函数改造

```cpp
// 1. 创建 worker (此时还在主线程)
m_worker = new MusicWorker();       // 不设 parent, 否则无法 moveToThread

// 2. 创建线程并搬入
m_thread = new QThread(this);
m_worker->moveToThread(m_thread);

// 3. 线程结束时自动清理 worker
connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

// 4. UI → Worker 信号 (QueuedConnection, 非阻塞)
connect(this, &MusicPlayer::sigPlay, m_worker, &MusicWorker::doPlay);
// ... sigStop, sigPause, sigResume, sigSetVolume, sigSeek, sigSetPlaybackMode

// 5. Worker → UI 信号 (自动回到主线程执行)
connect(m_worker, &MusicWorker::durationChanged,    this, &MusicPlayer::onWorkerDurationChanged);
connect(m_worker, &MusicWorker::positionChanged,    this, &MusicPlayer::onWorkerPositionChanged);
connect(m_worker, &MusicWorker::currentIndexChanged, this, &MusicPlayer::onWorkerIndexChanged);

// 6. 启动线程
m_thread->start();
```

#### 24.4.3 按钮操作改造（以"下一首"为例）

```cpp
// 改造前: 直接在 UI 线程调用 stop/play → UI 卡死
void MusicPlayer::on_pBtn_Next_clicked() {
    musicPlayer->stop();                    // ← 阻塞
    ui->listWidget->setCurrentRow(...);     // ← 触发 currentRowChanged → 又 stop+play
    musicPlayList->next();
    musicPlayer->play();                    // ← 阻塞
}

// 改造后: emit 信号, 立即返回, UI 零阻塞
void MusicPlayer::on_pBtn_Next_clicked() {
    int nextIdx = m_worker->nextIndex(1);
    ui->listWidget->blockSignals(true);     // 防止触发 currentRowChanged
    ui->listWidget->setCurrentRow(nextIdx); // 纯 UI 操作, 瞬间完成
    ui->listWidget->blockSignals(false);
    emit sigPlay(nextIdx);                  // 非阻塞, 投递到工作线程队列
}
```

#### 24.4.4 安全退出

```cpp
MusicPlayer::~MusicPlayer() {
    m_thread->quit();   // 通知工作线程退出事件循环
    m_thread->wait();   // 阻塞等待线程结束
    delete ui;
    // m_worker 由 deleteLater 自动释放 (connect finished → deleteLater)
}
```

### 24.5 线程安全保护机制

| 机制 | 作用 |
|------|------|
| `moveToThread` + `QueuedConnection` | UI 线程的 `emit sigPlay()` 被打包成事件投递到工作线程队列，两个线程永远不会同时操作 QMediaPlayer |
| `blockSignals(true/false)` | 防止 `setCurrentRow()` 触发 `currentRowChanged` 导致重复 `emit sigPlay` |
| `QThread::finished → deleteLater` | 线程结束后自动释放 worker，避免野指针 |
| `quit() + wait()` | 析构时确保工作线程干净退出 |

**野指针防护**：`m_worker` 不设 parent（`new MusicWorker()` 无参数），因为设了 parent 的 QObject 不能 `moveToThread`。析构时通过 `finished → deleteLater` 链保证 worker 在线程结束后才释放，不会出现"线程还在用 worker，主线程已经 delete 了"的野指针问题。

### 24.6 项目多线程/多进程全景

完成音乐播放器改造后，项目的并发架构全貌：

```
VehicleTerminal 进程
├── 主线程 (UI 事件循环)
│   ├── MainWindow — 界面绘制、按钮响应
│   ├── MusicPlayer — 音乐 UI 壳 (发信号给 worker)
│   ├── RadarWidget — 33ms QTimer 轮询 RadarReader 数据
│   ├── SpeechRecognition — QTimer 轮询按键
│   └── Monitor — 500ms QTimer 读 AP3216C
│
├── CameraThread (QThread 子类)
│   └── V4L2 DQBUF 循环采帧 → 信号 frameReady(QImage)
│
├── Dht11 (QThread 子类)
│   └── 2s 间隔读 /dev/dht11 → 信号 updateDht11Data()
│
├── RadarReader (QThread 子类)
│   └── 读 /dev/fmcw_radar + 信号处理 → QMutex 保护共享数据
│
├── CaptureThread (QThread 子类, Monitor 模块)
│   └── V4L2 采帧 → 信号 imageReady(QImage)
│
├── MusicWorker (moveToThread 模式) ← 本次新增
│   └── QMediaPlayer stop/play → QueuedConnection 信号槽
│
├── QtConcurrent::run (VoiceAssistant)
│   └── 录音 + Whisper + LLM 完整管线
│
└── GPS pthread (gps.c)
    └── UART 读 NMEA → pthread_rwlock 保护经纬度

外部进程 (QProcess)
├── llm_demo (常驻) ← stdin/stdout 管道通信
├── whisper_demo (按需) ← taskset f0 绑 A76 大核
├── parecord / sox / amixer / pactl (短命令)
└── arecord (SpeechRecognition fallback)
```

#### 各场景安全机制对比

| 场景 | 机制 | 保护 | 通信 |
|------|------|------|------|
| CameraThread | QThread 子类 | volatile running | 信号 frameReady(QImage) |
| Dht11 | QThread 子类 | while(1) 死循环 | 信号 updateDht11Data() |
| RadarReader | QThread 子类 | **QMutex** + volatile | 信号 + mutex 保护的 getTargets() |
| CaptureThread | QThread 子类 | bool startFlag | 信号 imageReady(QImage) |
| **MusicWorker** | **moveToThread** | **QueuedConnection + blockSignals** | **sigPlay/positionChanged 等信号** |
| VoiceAssistant | QtConcurrent::run | **QMutex** + m_running 标志 | QMetaObject::invokeMethod 回 UI |
| GPS | pthread | **pthread_rwlock** | 全局变量 + 读写锁 |
| llm_demo | QProcess (独立进程) | **进程隔离** | stdin/stdout 管道 |
| whisper | QProcess (独立进程) | **进程隔离** + taskset | 文件输出 |

#### 两种 QThread 使用模式

本项目同时使用了两种 QThread 模式，面试时要能区分：

| | 子类化 QThread (重写 run) | moveToThread 模式 |
|--|---|---|
| 代表 | CameraThread, Dht11, RadarReader | **MusicWorker** |
| 原理 | `run()` 里写死循环 | worker 的槽函数在线程事件循环中执行 |
| 事件循环 | 通常没有（run 里是 while 循环） | 有（QThread 默认的 `exec()`） |
| 适合场景 | 连续轮询（采帧、读传感器） | 命令-响应式（play/stop/seek） |
| 定时器/网络 | 不能在 run() 里直接用 QTimer | 可以在 worker 里正常用 QTimer |

MusicWorker 用 moveToThread 模式的原因：QMediaPlayer 依赖事件循环来驱动 GStreamer 回调（durationChanged、positionChanged），如果用 QThread 子类重写 `run()` 写死循环，这些回调就无法触发。

#### 类比内核 DHT11 驱动

| 概念 | 内核 (DHT11) | Qt (音乐播放) |
|------|-------------|--------------|
| 阻塞操作 | `msleep(20)` + GPIO 时序采集 | `stop()` + `play()` GStreamer 管线切换 |
| 隔离机制 | `local_irq_save` 关中断 | `moveToThread` 隔离到工作线程 |
| 互斥保护 | `DEFINE_MUTEX(dht11_lock)` | `QueuedConnection` (事件队列天然串行) |
| 为什么要隔离 | 中断打断时序 → 数据错 | 播放阻塞主线程 → UI 卡 |

### 24.7 编译验证

```bash
cd ~/chezai/VehicleTerminal
qmake && make clean && make -j4
# 零 error、零 warning
```

.pro 文件新增：
```
SOURCES += Music/music_worker.cpp
HEADERS += Music/music_worker.h
```把这个传话筒封装好了，你只需要 write() 和 read() 就行 。 -->
-------------- 已知的还能继续优化项目 ---------------
1 切割会卡顿，同时整个UI主页面跟着卡顿
优化方案：把切歌功能放到一个独立的进程里，UI 进程和切割进程通过管道通信。这样切割时 UI 仍然流畅。
2 eBPF检测
    1. SPI 通信延迟追踪 (FMCW 雷达)
    → 统计 spi_transfer 耗时分布，验证 hrtimer 1ms 是否达标

    2. GPIO 时序验证 (DHT11 驱动)
    → 追踪 gpio_set_value/gpio_get_value 调用，验证关中断时长 <200us

    3. 调度延迟分析 (AI 推理)
    → 追踪 Whisper/LLM 进程的 CPU 抢占延迟，验证 cpuset 隔离效果

    4. 系统调用性能画像
    → 统计 Qt 应用的 read/write/ioctl 热点，找瓶颈