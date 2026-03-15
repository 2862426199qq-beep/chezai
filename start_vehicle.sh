#!/bin/bash
# ============================================================
# start_vehicle.sh — VehicleTerminal cpuset 隔离启动脚本
# RK3588S: CPU 0-3 = A55 (小核), CPU 4-7 = A76 (大核)
#
# 分区策略:
#   ui  组 (CPU 0-3, A55): Qt UI 主线程 + 传感器线程(DHT11等)
#   ai  组 (CPU 4-7, A76): Whisper / LLM 子进程 (taskset f0)
#
# 用法: sudo bash start_vehicle.sh
# ============================================================

set -e

CPUSET_ROOT="/sys/fs/cgroup/cpuset"

# ---------- 1. 创建 cpuset 分区 ----------
echo "[cpuset] 创建 CPU 分区..."

# UI 组: 小核 A55 (CPU 0-3)
mkdir -p "$CPUSET_ROOT/ui"
echo 0-3 > "$CPUSET_ROOT/ui/cpuset.cpus"
echo 0   > "$CPUSET_ROOT/ui/cpuset.mems"

# AI 组: 大核 A76 (CPU 4-7)
mkdir -p "$CPUSET_ROOT/ai"
echo 4-7 > "$CPUSET_ROOT/ai/cpuset.cpus"
echo 0   > "$CPUSET_ROOT/ai/cpuset.mems"

echo "[cpuset] ui 组: CPU 0-3 (A55 小核)"
echo "[cpuset] ai 组: CPU 4-7 (A76 大核)"

# ---------- 2. 设置大核性能模式 ----------
echo "[governor] 大核 CPU 4-7 设为 performance..."
for cpu in 4 5 6 7; do
    gov_path="/sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor"
    if [ -f "$gov_path" ]; then
        echo performance > "$gov_path"
    fi
done

# ---------- 3. 启动 VehicleTerminal ----------
echo "[launch] 启动 VehicleTerminal (UI 绑定小核 0-3)..."

cd /home/cat/chezai/VehicleTerminal

# 将 Qt 主进程放入 ui cpuset 组
# 注: Whisper/LLM 子进程通过 taskset f0 自行绑定大核, 不受 ui 组限制
#     因为 taskset 会覆盖 cpuset 的亲和性设置(子进程 fork 后 exec 会重设)
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
./VehicleTerminal &
VT_PID=$!

# 等待进程启动
sleep 0.5
if kill -0 "$VT_PID" 2>/dev/null; then
    echo "$VT_PID" > "$CPUSET_ROOT/ui/tasks"
    echo "[cpuset] VehicleTerminal (PID=$VT_PID) → ui 组 (CPU 0-3)"
else
    echo "[error] VehicleTerminal 启动失败"
    exit 1
fi

echo ""
echo "============================================"
echo "  VehicleTerminal 已启动 (PID=$VT_PID)"
echo "  UI 线程  → CPU 0-3 (A55 小核)"
echo "  AI 推理  → CPU 4-7 (A76 大核, taskset)"
echo "============================================"

# 等待主进程退出
wait "$VT_PID" 2>/dev/null || true
echo "[exit] VehicleTerminal 已退出"
