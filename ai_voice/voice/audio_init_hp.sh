#!/bin/bash
# audio_init_hp.sh - 初始化 es8388 音频配置（耳机+耳机麦克风模式）
# 正确方式：通过 PulseAudio UCM port 切换触发 EnableSequence，不能只靠 amixer

SINK="alsa_output.platform-es8388-sound.HiFi__hw_rockchipes8388__sink"
SOURCE="alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source"
CARD=2

# 1. 激活耳机输出端口（触发 UCM EnableSequence: Headphone Switch=on）
pactl set-sink-port "$SINK" '[Out] Headphones'

# 2. 激活耳机麦克风端口（触发 UCM EnableSequence: Differential Mux=Line1, Headset Mic Switch=on）
# 先切走再切回，强制触发 DisableSequence + EnableSequence
pactl set-source-port "$SOURCE" '[In] Mic'
sleep 0.2
pactl set-source-port "$SOURCE" '[In] Headset'

# 3. 提升 PGA 录音增益到最大（UCM 默认值=3，调到8 = +24dB）
amixer -c $CARD cset numid=27 8   # Left Channel Capture Volume
amixer -c $CARD cset numid=28 8   # Right Channel Capture Volume

echo "ES8388 耳机模式初始化完成 (card $CARD)"
echo -n "  Headphone Switch: "; amixer -c $CARD cget numid=39 | grep ": values"
echo -n "  Headset Mic Switch: "; amixer -c $CARD cget numid=42 | grep ": values"
