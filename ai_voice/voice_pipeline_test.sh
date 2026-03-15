#!/bin/bash
# ============================================================
# 语音AI流水线测试
# 录音 → Whisper(STT) → 显示识别文字 → Qwen2.5(LLM) → JSON指令
# ============================================================

# ---------- 路径配置 ----------
WHISPER_DIR="/home/cat/chezai/ai_voice/whisper/rknn_model_zoo/install/rk3588_linux_aarch64/rknn_whisper_demo"
LLM_DIR="/home/cat/chezai/ai_voice/demo_Linux_aarch64"
LLM_BIN="${LLM_DIR}/llm_demo"
LLM_MODEL="/home/cat/chezai/ai_voice/Qwen2.5-0.5B_W8A8_RK3588.rkllm"

AUDIO_TMP="/tmp/voice_input.wav"
LANG="zh"
AUDIO_INIT_HP="/home/cat/chezai/ai_voice/voice/audio_init_hp.sh"
AUDIO_INIT_SPK="/home/cat/chezai/ai_voice/voice/audio_init_spk.sh"

# ---------- 颜色 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ---------- 检查依赖 ----------
check_deps() {
    local missing=0
    for f in "$WHISPER_DIR/rknn_whisper_demo" \
             "$WHISPER_DIR/model/whisper_encoder_base_20s.rknn" \
             "$WHISPER_DIR/model/whisper_decoder_base_20s.rknn" \
             "$LLM_BIN" "$LLM_MODEL"; do
        if [ ! -f "$f" ]; then
            echo -e "${RED}[错误] 文件不存在: $f${NC}"
            missing=1
        fi
    done
    if ! command -v arecord &>/dev/null; then
        echo -e "${RED}[错误] arecord 未安装${NC}"
        missing=1
    fi
    if [ $missing -eq 1 ]; then
        exit 1
    fi
    echo -e "${GREEN}[检查] 依赖文件全部就绪${NC}"
}

# ---------- 录音 ----------
AUDIO_NORM="/tmp/voice_input_norm.wav"

record_audio() {
    local duration=$1
    echo ""
    echo -e "${CYAN}[录音] 按 Enter 开始录音 (最长${duration}秒, 再按 Enter 提前结束)...${NC}"
    read -r

    echo -e "${YELLOW}>>> 录音中，请说话... <<<${NC}"

    # 用默认 44100Hz 录音（PulseAudio 原生），Whisper 内部会自动转 16000Hz 单声道
    parecord --file-format=wav "$AUDIO_TMP" &
    local rec_pid=$!

    read -r -t "$duration" 2>/dev/null || true

    if kill -0 "$rec_pid" 2>/dev/null; then
        kill "$rec_pid" 2>/dev/null
        wait "$rec_pid" 2>/dev/null || true
    fi
    sleep 0.3   # 等 parecord 刷新 WAV header

    if [ ! -f "$AUDIO_TMP" ] || [ ! -s "$AUDIO_TMP" ]; then
        echo -e "${RED}[错误] 录音文件为空${NC}"
        return 1
    fi

    local size
    size=$(stat -c%s "$AUDIO_TMP")
    local info
    info=$(sox --i "$AUDIO_TMP" 2>/dev/null | grep -E "Sample Rate|Channels|Duration" | tr '\n' ', ')
    echo -e "${GREEN}[录音完成] ${size} bytes  ${info}${NC}"

    # 检查音频是否有有效声音
    local max_amp
    max_amp=$(sox "$AUDIO_TMP" -n stat 2>&1 | grep "Maximum amplitude" | awk '{print $3}')
    echo -e "  音频峰值振幅: ${max_amp}"
    local is_silent
    is_silent=$(echo "$max_amp < 0.002" | bc -l 2>/dev/null || echo "0")
    if [ "$is_silent" = "1" ]; then
        echo -e "${YELLOW}[警告] 音量极低，可能是静音。请检查麦克风是否正常工作。${NC}"
        echo -e "${YELLOW}       尝试运行: bash ${AUDIO_INIT_HP}${NC}"
        return 1
    fi

    # 归一化音频到 -3dB，提升 Whisper 识别率
    sox "$AUDIO_TMP" "$AUDIO_NORM" norm -3 2>/dev/null
    if [ -f "$AUDIO_NORM" ] && [ -s "$AUDIO_NORM" ]; then
        mv "$AUDIO_NORM" "$AUDIO_TMP"
        echo -e "  已归一化音频到 -3dB"
    fi

    return 0
}

# ---------- Whisper 识别 ----------
WHISPER_TEXT=""

run_whisper() {
    WHISPER_TEXT=""
    echo -e "${CYAN}[Whisper] 正在识别语音...${NC}"

    local output
    output=$(cd "$WHISPER_DIR" && ./rknn_whisper_demo \
        ./model/whisper_encoder_base_20s.rknn \
        ./model/whisper_decoder_base_20s.rknn \
        "$LANG" "$AUDIO_TMP" 2>&1)

    local text
    text=$(echo "$output" | grep "Whisper output:" | sed 's/.*Whisper output://' | xargs)

    local rtf
    rtf=$(echo "$output" | grep "Real Time Factor" | head -1)

    if [ -z "$text" ]; then
        echo -e "${YELLOW}[Whisper] 未识别到有效语音（静音或噪音）${NC}"
        return 1
    fi

    echo ""
    echo -e "${BOLD}┌──────────────────────────────────────────┐${NC}"
    echo -e "${BOLD}│${GREEN}  语音识别结果: ${text}${NC}"
    echo -e "${BOLD}└──────────────────────────────────────────┘${NC}"
    [ -n "$rtf" ] && echo -e "${CYAN}  ${rtf}${NC}"

    WHISPER_TEXT="$text"
    return 0
}

# ---------- LLM 推理 ----------
LLM_JSON=""

run_llm() {
    local input_text="$1"
    LLM_JSON=""
    echo ""
    echo -e "${CYAN}[LLM] 正在加载 Qwen2.5-0.5B 生成 JSON 指令...${NC}"
    echo -e "${CYAN}      (首次加载模型约 10 秒)${NC}"

    local prompt="你是车载语音助手,根据用户语音指令输出JSON,只输出一行JSON不要其他内容。
格式:{\"action\":\"xxx\",\"param\":\"xxx\"}
支持的action: open_music,close_music,open_weather,open_map,open_camera,close_camera,open_radar,open_dht11,open_settings,unknown
用户说:${input_text}"

    local llm_output
    llm_output=$(cd "$LLM_DIR" && \
        export LD_LIBRARY_PATH=./lib && \
        ulimit -n 102400 2>/dev/null; \
        printf '%s\nexit\n' "$prompt" | \
        taskset f0 ./llm_demo "$LLM_MODEL" 256 512 2>&1)

    local json_result
    json_result=$(echo "$llm_output" | grep -o '{[^}]*}' | head -1)

    if [ -z "$json_result" ]; then
        json_result=$(echo "$llm_output" | grep -o 'robot:.*' | head -1 | sed 's/robot: *//')
    fi

    LLM_JSON="$json_result"

    echo ""
    echo -e "${BOLD}┌──────────────────────────────────────────┐${NC}"
    echo -e "${BOLD}│${GREEN}  JSON 指令: ${json_result}${NC}"
    echo -e "${BOLD}└──────────────────────────────────────────┘${NC}"
}

# ---------- 主流程 ----------
main() {
    echo ""
    echo -e "${BOLD}============================================${NC}"
    echo -e "${BOLD}${GREEN}    语音 AI 流水线测试${NC}"
    echo -e "${BOLD}  麦克风 → Whisper(STT) → Qwen2.5(LLM) → JSON${NC}"
    echo -e "${BOLD}============================================${NC}"
    echo ""

    check_deps

    # 自动检测耳机状态并初始化音频
    local hp_jack
    hp_jack=$(amixer -c 2 cget numid=37 2>/dev/null | grep "values=" | grep -o "on\|off")
    if [ "$hp_jack" = "on" ]; then
        echo -e "${CYAN}[音频] 检测到耳机已插入 → 耳麦模式${NC}"
        bash "$AUDIO_INIT_HP" 2>/dev/null | tail -1
    else
        echo -e "${CYAN}[音频] 未检测到耳机 → 主麦克风模式${NC}"
        bash "$AUDIO_INIT_SPK" 2>/dev/null | tail -1
    fi

    local mode="${1:-full}"
    local max_duration="${2:-10}"

    echo -e "模式: ${CYAN}${mode}${NC} | 最大录音: ${CYAN}${max_duration}秒${NC}"

    while true; do
        echo ""
        echo -e "${CYAN}────────── 新一轮 ──────────${NC}"

        if ! record_audio "$max_duration"; then
            echo -e "${YELLOW}录音失败，重试${NC}"
            continue
        fi

        if ! run_whisper; then
            echo ""
            echo -e "输入 ${RED}q${NC} 退出, 按 ${GREEN}Enter${NC} 继续..."
            read -r choice
            [ "$choice" = "q" ] && break
            continue
        fi

        if [ "$mode" = "full" ]; then
            run_llm "$WHISPER_TEXT"
        fi

        echo ""
        echo -e "输入 ${RED}q${NC} 退出, 按 ${GREEN}Enter${NC} 继续..."
        read -r choice
        [ "$choice" = "q" ] && break
    done

    echo -e "${GREEN}测试结束${NC}"
    rm -f "$AUDIO_TMP" "$AUDIO_NORM"
}

# ---------- 入口 ----------
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "用法: $0 [模式] [最大录音秒数]"
    echo ""
    echo "  模式:"
    echo "    stt   仅语音识别（录音 → Whisper → 显示文字）"
    echo "    full  完整流水线（录音 → Whisper → Qwen2.5 → JSON）(默认)"
    echo ""
    echo "  示例:"
    echo "    $0 stt 10      # 仅识别，最长录10秒"
    echo "    $0 full 15     # 完整流水线，最长录15秒"
    echo "    $0             # 默认: full 模式，10秒"
    exit 0
fi

main "$@"
