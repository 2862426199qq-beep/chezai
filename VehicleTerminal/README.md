# VehicleTerminal 进度记录

## 2026-03-04

### 变更：AI Voice 按钮链路改造（避免固定“打开音乐”）
- 问题：点击 `AI Voice` 后总是触发固定回复“好的，正在为你打开音乐”。
- 原因：此前按钮直接调用 `ai_voice` 的 stub ASR，`LocalASR::transcribe()` 在 stub 模式下固定返回“打开音乐”。
- 修复方案：改为“AI按钮触发录音 -> 百度ASR识别文本 -> ai_voice LLM解析意图 + TTS播报”。

### 本次代码改动
- `mainwindow.h`
  - 新增状态字段：`bool m_aiVoicePending = false;`
- `mainwindow.cpp`
  - `onBtnAiVoice()`：
    - 防重复触发（pending 时忽略再次点击）
    - 点击后启动录音，2.5秒后自动停止
  - `getSpeechResult(QNetworkReply *reply)`：
    - 在 AI 模式下，使用百度 ASR 文本调用 `LlmEngine::chat()`
    - 使用 `LlmEngine::parseIntent()` 解析意图
    - 用 `TtsEngine::speak()` 播报回答（去掉 `[INTENT:XXX]` 标记）
    - 根据意图执行 UI 动作（音乐/地图/监控/天气）
    - AI 分支处理完后 `return`，不再走旧的硬编码关键词分支

### 构建验证
- 命令：`cd VehicleTerminal && make -j4`
- 结果：编译与链接成功。

### 当前状态
- AI 按钮不再固定打开音乐；行为将随百度 ASR 识别文本变化。
- 仍为“百度ASR + ai_voice LLM/TTS(stub/real可切)”混合链路。

### 增量修复：AI 点击“没反应”问题
- 现象：点击 `AI Voice` 后偶发无后续动作。
  - 在 `getSpeechResult()` 中补齐错误分支复位：网络错误、JSON解析失败、空结果时都复位 pending。
  - 在 `on_handleRecord()` 打不开录音文件时复位 pending，并提示 `Record Failed`。
### 增量验证
- 命令：`cd VehicleTerminal && make -j4`
- 结果：编译通过。

### 增量排查：RK3588S 耳麦录音链路（i.MX6ULL -> RK3588S 迁移）
- 系统层检查结果：
  - `cat /proc/asound/cards` 显示 `rockchip-es8388` 驱动已加载
- 关键发现：
  - `arecord -D hw:2,0 -r 16000 -c 1 ...` 失败（单声道参数不被该 hw 直通模式支持）
  - `arecord -D hw:2,0 -r 16000 -c 2 ...` 成功
  - `arecord -D plughw:2,0 -r 16000 -c 1 ...` 成功（软件转换）
- 代码迁移策略：
  - `speechrecognition` 默认改为 `arecord -D plughw:2,0 -f S16_LE -r 16000 -c 1 ./record.wav -q`
  - 保留 `QAudioRecorder` 作为回退路径（`arecord` 启动失败时）
  - `stopRecord()` 对 `arecord` 进程优先 `terminate/kill`，确保落盘后再触发识别

### 本次涉及文件
- `speechrecognition.h`：新增 `QProcess` 成员和 `m_usingArecord` 状态
### 迁移状态结论
- RK3588S 板端音频驱动可用，问题主要在“录音参数/接口选择”而不是驱动缺失。

### 增量检查：混音器开关状态
- `amixer -c 2 sget 'Capture Mute'` 当前为 `[off]`（疑似采集被静音）
- `amixer -c 2 sget 'Main Mic'` 当前为 `[off]`

### 建议执行（耳麦场景）
- `amixer -c 2 sset 'Capture Mute' on`
- `amixer -c 2 sset 'Headset Mic' on`
- 执行后可用：`arecord -D plughw:2,0 -f S16_LE -r 16000 -c 1 -d 3 /tmp/rk_mic_verify.wav`

### 增量定位：仍无法识别的根因
- 已通过直连百度接口复现错误：`{"err_no":3302,"err_msg":"Access token invalid or no longer valid"}`。
- 结论：当前项目内硬编码 token 已失效（时间戳对应 2024-11），即使录音正常也无法在线识别。

- `mainwindow.cpp`
  - 新增 `resolveBaiduAsrToken()`：优先读取环境变量 `BAIDU_ASR_TOKEN`，为空再用旧默认值。
  - AI 模式下增加音频大小校验（过短/无效直接提示 `Audio Too Short/Invalid`）。
  - 对 `err_no=3302` 给出明确状态提示：`Token Invalid(3302), set BAIDU_ASR_TOKEN`。
- `speechrecognition.cpp`
  - `arecord` 停止由 `SIGTERM` 改为 `SIGINT`，优先保证 WAV 头正确收尾，减少上传后解析失败概率。

- 已成功获取并保存 token 到本地文件：`VehicleTerminal/.baidu_asr_token`（权限 `600`）。
- 已验证文件存在且长度正常（71字节）。
  - `BAIDU_ASR_TOKEN="$(cat .baidu_asr_token)" ./VehicleTerminal`

> 说明：在无图形环境（无 DISPLAY）下启动 Qt 会报 `could not connect to display :0`，需在板子本地桌面会话中运行上述命令。

### 增量修复：AI Voice 一直停在 Recognizing...
- 症状：点击 AI 后状态长期停留在 `Recognizing...`（或 `Uploading...`），无后续结果。
- 修复内容：
  - 在 `onBtnAiVoice()` 新增 15 秒总超时保护：超时自动复位 pending 并显示 `AI Voice: Timeout`。
  - 在 `on_handleRecord()` 新增百度请求 10 秒超时保护：请求仍运行则主动 `abort()`，触发网络错误分支。
- 目的：避免任何录音回调/网络回调异常导致状态机永久卡死。
- 启动命令：`BAIDU_ASR_TOKEN="$(cat .baidu_asr_token)" ./VehicleTerminal`
- 点击 `AI Voice` 后观察状态：
  - 正常：`Running -> Recognizing -> Uploading -> AI Voice: OPEN_xxx`
  - 异常：最迟 15 秒内应变成 `AI Voice: Timeout` 或网络错误，不再无限卡住。

## 2026-03-05

### 变更：移除蓝牙相关功能
- 由于当前硬件方案不使用蓝牙芯片，已从 `VehicleTerminal` 移除全部蓝牙相关代码与文档。
- 已删除：`BluetoothAudio` 封装、`Bluetooth/` 目录下服务脚本与验证脚本、蓝牙专项报告。
- 已回退主界面中的 `BT Audio` 按钮与状态显示，保留非蓝牙功能不受影响。
