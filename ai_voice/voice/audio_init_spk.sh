#!/bin/bash
# audio_init_spk.sh - 初始化 es8388 音频配置（扬声器+内置麦克风模式）
# 注意：扬声器端口在耳机插入时为 "not available"（UCM JackHWMute），需拔出耳机

SINK="alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink"
SOURCE="alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"
CARD=2

# 1. 激活扬声器输出端口（触发 UCM EnableSequence: Speaker Switch=on）
pactl set-sink-port "$SINK" '[Out] Speaker'

# 2. 激活内置麦克风端口（UCM: Differential Mux=Line2, Main Mic Switch=on）
pactl set-source-port "$SOURCE" '[In] Headset'
sleep 0.2
pactl set-source-port "$SOURCE" '[In] Mic'

# 3. 提升 PGA 录音增益
amixer -c $CARD cset numid=27 8
amixer -c $CARD cset numid=28 8

echo "ES8388 扬声器+内置麦克风模式初始化完成 (card $CARD)"
echo -n "  Speaker Switch: "; amixer -c $CARD cget numid=40 | grep ": values"
echo -n "  Main Mic Switch: "; amixer -c $CARD cget numid=41 | grep ": values"
