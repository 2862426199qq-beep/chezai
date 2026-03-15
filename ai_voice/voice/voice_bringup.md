# 野火 RK3588 音频功能启动记录

> 参考教程：https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/audio/audio.html  
> 记录日期：2026-03-08  
> 工作目录：`/home/cat/chezai/ai_voice/voice`

---

## 一、声卡设备

### 1.1 获取录音设备

```bash
arecord -l
```

**实际输出：**
```
**** List of CAPTURE Hardware Devices ****
card 2: rockchipes8388 [rockchip-es8388], device 0: dailink-multicodecs ES8323.7-0011-0 [dailink-multicodecs ES8323.7-0011-0]
   Subdevices: 1/1
   Subdevice #0: subdevice #0
```

> **注意**：本板卡（RK3588S）上 es8388 录音设备是 **card 2**，教程中是 card 1，注意区分。

### 1.2 获取播放设备

```bash
aplay -l
```

**实际输出：**
```
**** List of PLAYBACK Hardware Devices ****
card 0: rockchipdp0 [rockchip-dp0], device 0: rockchip-dp0 spdif-hifi-0 [rockchip-dp0 spdif-hifi-0]
   Subdevices: 1/1
   Subdevice #0: subdevice #0
card 1: rockchiphdmi0 [rockchip-hdmi0], device 0: rockchip-hdmi0 i2s-hifi-0 [rockchip-hdmi0 i2s-hifi-0]
   Subdevices: 1/1
   Subdevice #0: subdevice #0
card 2: rockchipes8388 [rockchip-es8388], device 0: dailink-multicodecs ES8323.7-0011-0 [dailink-multicodecs ES8323.7-0011-0]
   Subdevices: 1/1
   Subdevice #0: subdevice #0
```

| card | 设备 | 说明 |
|------|------|------|
| card 0 | rockchip-dp0 | DP 接口音频输出 |
| card 1 | rockchip-hdmi0 | HDMI 音频输出 |
| card 2 | rockchip-es8388 | 板载音频芯片（耳机/扬声器/麦克风） |

### 1.3 声卡驱动目录

```bash
ls /dev/snd/
```

**实际输出：**
```
by-path    controlC1  pcmC0D0p  pcmC2D0c  seq
controlC0  controlC2  pcmC1D0p  pcmC2D0p  timer
```

| 节点 | 说明 |
|------|------|
| `controlC0` | DP 声卡控制 |
| `controlC1` | HDMI 声卡控制 |
| `controlC2` | es8388 声卡控制 |
| `pcmC0D0p` | DP 播放设备 |
| `pcmC1D0p` | HDMI 播放设备 |
| `pcmC2D0p` | es8388 播放设备 |
| `pcmC2D0c` | es8388 录音设备 |

```bash
ls -l /dev/snd/by-path/
```

**实际输出：**
```
lrwxrwxrwx 1 root root 12 Jun 18  2024 platform-dp0-sound -> ../controlC0
lrwxrwxrwx 1 root root 12 Jun 18  2024 platform-es8388-sound -> ../controlC2
lrwxrwxrwx 1 root root 12 Jun 18  2024 platform-hdmi0-sound -> ../controlC1
```

---

## 二、配置声卡（amixer）

本节以板载 **es8388（card 2）** 为操作对象。

### 2.1 列出所有控制器

```bash
amixer controls -c 2
```

**实际输出（关键控制器摘录）：**
```
numid=39,iface=MIXER,name='Headphone Switch'       # 耳机开关
numid=40,iface=MIXER,name='Speaker Switch'         # 扬声器开关
numid=41,iface=MIXER,name='Main Mic Switch'        # 主麦克风开关
numid=42,iface=MIXER,name='Headset Mic Switch'     # 耳机麦克风开关
numid=25,iface=MIXER,name='Capture Digital Volume' # 录音数字音量
numid=34,iface=MIXER,name='Output 1 Playback Volume' # 输出1播放音量
numid=35,iface=MIXER,name='Output 2 Playback Volume' # 输出2播放音量
numid=27,iface=MIXER,name='Left Channel Capture Volume'  # 左声道录音增益
numid=28,iface=MIXER,name='Right Channel Capture Volume' # 右声道录音增益
numid=16,iface=MIXER,name='ALC Capture Function'   # ALC 自动增益控制
```

### 2.2 查看当前开关状态

```bash
# 扬声器开关
amixer -c 2 cget numid=40
# 耳机开关
amixer -c 2 cget numid=39
# 主麦克风开关
amixer -c 2 cget numid=41
# 耳机麦克风开关
amixer -c 2 cget numid=42
```

**当前初始状态：**
```
Speaker Switch:    values=off   ← 扬声器关闭
Headphone Switch:  values=on    ← 耳机开启
Main Mic Switch:   values=off   ← 主麦克风（板载MIC）关闭
Headset Mic Switch: values=on   ← 耳机麦克风开启
```

### 2.3 查看音量配置

```bash
amixer -c 2 cget numid=34   # Output 1 Playback Volume
amixer -c 2 cget numid=35   # Output 2 Playback Volume
amixer -c 2 cget numid=25   # Capture Digital Volume
```

**当前值：**
```
Output 1 Playback Volume: values=27,27  (范围 0~33, 对应 -45dB ~ 0dB)
Output 2 Playback Volume: values=27,27  (范围 0~33)
Capture Digital Volume:   values=192,192 (范围 0~192, 对应 -96dB ~ 0dB)
```

### 2.4 常用配置命令

```bash
# 开启扬声器播放
amixer -c 2 cset numid=40 on

# 关闭耳机，切换到扬声器
amixer -c 2 cset numid=39 off
amixer -c 2 cset numid=40 on

# 开启主麦克风（板载MIC）
amixer -c 2 cset numid=41 on
amixer -c 2 cset numid=42 off   # 同时关闭耳机麦克风（二选一）

# 调节播放音量（最大 33）
amixer -c 2 cset numid=34 "30,30"
amixer -c 2 cset numid=35 "30,30"

# 调节录音增益（最大 192）
amixer -c 2 cset numid=25 "192,192"
```

### 2.5 图形化配置（alsamixer）

```bash
alsamixer
```
- F6：选择声卡（选 rockchip-es8388）
- F4：切换到录音配置模式
- 上下键调整音量，M 键静音/取消静音

---

## 三、录音与播放

> **核心命令前提**：本板卡 es8388 对应 card 2，所有命令使用 `-Dhw:2`

### 3.1 命令行录音

```bash
# 录制 CD 音质，持续 10 秒，保存到 test.wav
arecord -f cd -Dhw:2 -d 10 test.wav

# 参数说明：
# -f cd    : CD 音质（16bit, 44100Hz, 立体声）
# -Dhw:2   : 使用 card 2（板载 es8388）
# -d 10    : 录制时长 10 秒
# test.wav : 输出文件名
```

**✅ 实测（5 秒录音）：**
```bash
arecord -f cd -Dhw:2 -d 5 test_record.wav
# 结果：生成 862K 的 wav 文件，录音成功
```

### 3.2 命令行播放

```bash
# 播放 wav 文件（aplay 只支持 wav 格式）
aplay -Dhw:2 test.wav
```

**✅ 实测播放：**
```bash
aplay -Dhw:2 test_record.wav
# 结果：Playing WAVE... Signed 16 bit Little Endian, Rate 44100 Hz, Stereo — 播放成功
```

### 3.3 边录音边播放（回声测试）

```bash
# 用 card 2 录制，并实时用 card 2 播放（回声监听）
arecord -f cd -Dhw:2 | aplay -Dhw:2
```

### 3.4 多格式播放（SoX）

```bash
# 安装 sox
sudo apt install sox libsox-fmt-all

# 播放各种格式
play -d hw:2 xxx.mp3
play -d hw:2 xxx.wav
play -d hw:2 xxx.flac
```

### 3.5 ffplay 播放（已预装）

```bash
# 只听音频，关闭图形界面
ffplay -showmode 0 -nodisp xxx.mp3

# 播放并显示波形图
ffplay -showmode 1 xxx.mp3

# 播放并显示频谱图
ffplay -showmode 2 xxx.mp3
```

> ffplay 如需通过 es8388 输出声音，需提前配置声卡或结合 ALSA 配置文件使用。

---

## 四、与教程的差异对比

| 项目 | 教程（LubanCat 标准板卡） | 本板卡（RK3588S） |
|------|--------------------------|------------------|
| es8388 card 编号 | card 1 | **card 2** |
| 录音设备 | card 1 | **card 2** |
| HDMI 音频 | card 1、2 | card 1 |
| DP 音频 | 无 | **card 0**（新增） |
| 录音 PCM 节点 | pcmC1D0c | **pcmC2D0c** |
| 播放 PCM 节点 | pcmC1D0p | **pcmC2D0p** |

---

## 五、两种使用模式操作指南

> ⚠️ **核心要点**：系统运行 PulseAudio，它通过 UCM 接管了声卡。**不能直接用 `amixer` 设置开关**，必须用 `pactl set-sink/source-port` 切换端口来触发 UCM 的 `EnableSequence`。**录放音也必须走 PulseAudio 接口**（`paplay` / `parecord`），不能用 `aplay -Dhw:2` / `arecord -Dhw:2`。

---

### 模式一：耳麦模式（插 3.5mm 耳机麦克风）

**前提**：耳机已插入（`Headphone Jack = on`，`Headset Mic Jack = on`）

**第一步：初始化**
```bash
./audio_init_hp.sh
```
或手动执行：
```bash
SINK="alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink"
SOURCE="alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"

pactl set-sink-port "$SINK" '[Out] Headphones'   # 开耳机输出
pactl set-source-port "$SOURCE" '[In] Mic'        # 先切走（触发 Disable）
sleep 0.2
pactl set-source-port "$SOURCE" '[In] Headset'   # 再切回（触发 Enable，开耳机麦）
amixer -c 2 cset numid=27 8                       # 左路 PGA 增益最大（+24dB）
amixer -c 2 cset numid=28 8                       # 右路 PGA 增益最大
```

**第二步：验证（生成测试音）**
```bash
sox -n -r 44100 -c 2 -b 16 /tmp/tone.wav synth 3 sine 440
paplay /tmp/tone.wav   # 耳机应听到 440Hz 音调
```

**第三步：录音 & 播放**
```bash
# 录音（Ctrl+C 停止，或用 -d 限时）
parecord --file-format=wav rec.wav

# 播放
paplay rec.wav

# 检查录音是否有内容
sox rec.wav -n stat 2>&1 | grep "Maximum amplitude"
# > 0.01 说明麦克风正常
```

---

### 模式二：扬声器模式（板载扬声器 + 主麦克风，无需插耳机）

> ⚠️ **扬声器端口受 UCM `JackHWMute` 保护**：耳机插入时扬声器端口硬件级锁定为 `not available`，`pactl` 也无法强制切换。**必须先物理拔出耳机**，Speaker / Internal Mic 端口才会变为 available。

**第一步：拔出耳机，确认端口可用**
```bash
pactl list cards | grep -E "Speaker|Mic|available"
# 应看到：
# [Out] Speaker: ... available        ← 拔耳机后才出现
# [In] Mic: Internal Microphone ... available
```

**第二步：初始化**
```bash
./audio_init_spk.sh
```
或手动执行：
```bash
SINK="alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink"
SOURCE="alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"

pactl set-sink-port "$SINK" '[Out] Speaker'       # 开扬声器输出
pactl set-source-port "$SOURCE" '[In] Headset'    # 先切走（触发 Disable）
sleep 0.2
pactl set-source-port "$SOURCE" '[In] Mic'        # 再切回（触发 Enable，开主麦）
amixer -c 2 cset numid=27 8
amixer -c 2 cset numid=28 8
```

**第三步：录音 & 播放**
```bash
parecord --file-format=wav rec.wav   # 录音
paplay rec.wav                        # 播放（从扬声器输出）
```

---

### 模式速查表

| 项目 | 耳麦模式 | 扬声器模式 |
|------|---------|-----------|
| 硬件条件 | 插入 3.5mm 耳机麦克风 | 拔出耳机 |
| 初始化脚本 | `./audio_init_hp.sh` | `./audio_init_spk.sh` |
| 输出端口 | `[Out] Headphones` | `[Out] Speaker` |
| 输入端口 | `[In] Headset` | `[In] Mic` |
| UCM 麦克风路径 | Differential Mux = Line 1 | Differential Mux = Line 2 |
| 播放命令 | `paplay xxx.wav` | `paplay xxx.wav` |
| 录音命令 | `parecord --file-format=wav` | `parecord --file-format=wav` |

---

## 六、已知问题 & 踩坑记录

### 6.1 【已解决】amixer 直接设置开关无效 / 录音静音

**现象**：
- 用 `amixer -c 2 cset numid=39 on` 手动开耳机，要么不生效，要么 PulseAudio 激活设备时立即重置
- `arecord -Dhw:2` 录音，sox stat 显示 `Maximum amplitude: -0.000031`，完全静音

**根因**：  
系统运行了 **PulseAudio**，它通过 UCM 接管声卡初始化。UCM 的 `EnableSequence` 会在设备激活时先把所有开关置 `off`，再由各端口的 `EnableSequence` 来开对应的 switch。**直接 amixer 操作会被 PulseAudio 覆盖**，且 `arecord -Dhw:2` 绕过 PulseAudio 时 ADC 没有被正确初始化。

**正确操作流程**：
```bash
SINK="alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink"
SOURCE="alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"

# 用 pactl 切换端口，触发 UCM EnableSequence
pactl set-sink-port "$SINK" '[Out] Headphones'
pactl set-source-port "$SOURCE" '[In] Mic'
sleep 0.2
pactl set-source-port "$SOURCE" '[In] Headset'

# 提升 PGA 增益
amixer -c 2 cset numid=27 8
amixer -c 2 cset numid=28 8

# 用 PulseAudio 接口录放
paplay  audio.wav
parecord --file-format=wav rec.wav
```

**验证方法**：
```bash
# 用 sox 生成测试音播放
sox -n -r 44100 -c 2 -b 16 /tmp/tone.wav synth 3 sine 440
paplay /tmp/tone.wav   # 应听到 440Hz 音调

# 录音后检查波形是否有能量
parecord --file-format=wav /tmp/test.wav &
sleep 3 && kill %1
sox /tmp/test.wav -n stat 2>&1 | grep "Maximum amplitude"
# Maximum amplitude > 0.01 说明麦克风正常工作
```

### 6.2 为什么切换源端口需要"先切走再切回"

`pactl set-source-port '[In] Headset'` 如果端口已经是 Headset，不会触发任何 Sequence。需要先切到 `[In] Mic`（触发 DisableSequence 清空旧配置），再切回 `[In] Headset`（触发 EnableSequence 重新初始化）。

---

## 七、后续计划

- [ ] 集成音频采集到 VehicleTerminal Qt 应用（用 ALSA API 或 Qt Multimedia）
- [ ] 对接 Whisper 语音识别模块（`ai_voice/whisper/`）
- [ ] 测试板载主麦克风（Main Mic）录音效果
- [ ] 测试 SoX 安装后播放 mp3 格式
- [ ] 验证 arecord 录音 → Qwen2.5 RKLLM 推理的完整语音输入链路
