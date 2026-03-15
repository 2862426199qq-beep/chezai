### camera模块说明
/camera ：用户态提取数据测试 bring up
/camera_qt ：单独 Qt 显示 bring up
/VehicleTerminal ：主应用 bring up，集成雷达 + 摄像头 + 系统状态显示


cd /home/cat/chezai/VehicleTerminal
make clean && qmake && make -j4
./VehicleTerminal 




 

Created 6 todos

Find CPU/memory display in Qt

搜索正则表达式 `cpu|memory|meminfo|proc/stat|sysinfo|performance|monitor` (`**/VehicleTerminal/**/*.{cpp,h,ui}`)，50 个结果

搜索正则表达式 `ebpf|bpf|perf`，30 个结果

搜索与 `**/VehicleTerminal/**/*monitor*` 匹配的文件，4 匹配项

搜索匹配 `**/VehicleTerminal/**/*system*` 的文件，无匹配项

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor)

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.cpp#380-380)，行 380 到 410

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.h#1-1)，行 1 到 110

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor/monitor.h)

搜索文本 `/proc` (`**/VehicleTerminal/Monitor/**`)，3 个结果

搜索匹配 `**/VehicleTerminal/**/*ap3216*` 的文件，无匹配项

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor/monitor.cpp)

搜索文本 `ap3216`，20 个结果

搜索正则表达式 `class|ap3216|readPsData` (`**/VehicleTerminal/**`)，20 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.cpp#100-100)，行 100 到 230

搜索文本 `lblNpu`，4 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal)

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.cpp#240-240)，行 240 到 270

搜索匹配 `**/ap3216.*` 的文件，无匹配项

搜索文本 `Ap3216c class Q_OBJECT` (`**/VehicleTerminal/**/*.cpp`)，无结果

搜索文本 `ap3216`，20 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal/moc_ap3216.cpp)

搜索文本 `cpu mem system monitor` (`**/VehicleTerminal/**/*.ui`)，无结果

读取 [](file:///home/cat/chezai/VehicleTerminal/ui_mainwindow.h#1-1)，行 1 到 200

读取 [](file:///home/cat/chezai/VehicleTerminal/ui_monitor.h)

搜索正则表达式 `timer.*CPU|on_timer_updateTime|QTimer`，40 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.cpp#1-1)，行 1 到 280

搜索文本 `/proc/meminfo`，无结果

搜索正则表达式 `readPsData|readAlsData|readIrData|Ap3216`，28 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.cpp#375-375)，行 375 到 425

搜索正则表达式 `MemAvailable|MemFree|MemTotal`，2 个结果

搜索文本 `lblNpu NPU` (`**/VehicleTerminal/**/*.{cpp,h}`)，无结果

读取 [](file:///home/cat/chezai/VehicleTerminal/settingwindow.cpp#50-50)，行 50 到 155

搜索文本 `NPU cpu memory lblNpu lblCpu` (`**/VehicleTerminal/mainwindow.cpp`)，无结果

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.h#50-50)，行 50 到 75

搜索文本 `lblCpu lblNpu lblStatus lblTemp lblHumi`，无结果

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.h#75-75)，行 75 到 110

搜索正则表达式 `/proc/stat|/proc/meminfo|sysinfo|top|free|ps aux` (`**/VehicleTerminal/**/*.cpp`)，20 个结果

Starting: *分析板上3个KEY可用性* (2/6)

现在来分析板子上3个按键的情况。从图片看是：**电源按键、Recovery按键、MaskRom按键**。让我查一下你板子的设备树和GPIO信息。 

Ran terminal command:  cat /proc/device-tree/model 2>/dev/null; echo "---"; ls /proc/device-tree/ | head -30; echo "---"; cat /sys/kernel/debug/gpio 2>/dev/null | head -40; echo "---"; find /proc/device-tree/ -name "*key*" -o -name "*button*" -o -name "*gpio-key*" 2>/dev/null | head -20

Ran terminal command:  # Check for adc-keys, gpio-keys nodes, and the RK806 power key
find /proc/device-tree/ -name "compatible" -exec sh -c 'val=$(cat "$1" 2>/dev/null | tr "\0" " "); case "$val" in *key*|*button*|*gpio-key*|*adc-key*) echo "$1: $val";; esac' _ {} \; 2>/dev/null
echo "=== RK806 pwrkey ==="
cat /proc/device-tree/spi@feb20000/rk806single@0/pwrkey/compatible 2>/dev/null | tr '\0' ' ; '; echo
echo "=== adc-keys ==="
find /proc/device-tree/ -path "*/adc*key*" -name "compatible" -exec sh -c 'echo "$1: $(cat "$1" | tr "\0" " ")"' _ {} \; 2>/dev/null
echo "=== gpio-keys ==="
find /proc/device-tree/ -path "*/gpio*key*" -name "compatible" -exec sh -c 'echo "$1: $(cat "$1" | tr "\0" " ")"' _ {} \; 2>/dev/null
echo "=== input devices ==="
cat /proc/bus/input/devices 2>/dev/null | head -50

Ran terminal command:  find /proc/device-tree/ -name "compatible" | xargs grep -l "key\|button" 2>/dev/null
Completed with input:  find /proc/device-tree/ -name "compatible" | xargs grep -l "key\|button" 2>/dev/null

Ran terminal command:  cat /proc/bus/input/devices

Ran terminal command:  # Check GPIO status and find available LEDs
echo "=== LEDs ==="
ls /sys/class/leds/ 2>/dev/null
echo "=== GPIO controllers ==="
ls /sys/class/gpio/ 2>/dev/null
echo "=== Recovery key GPIO ==="
# Typically Recovery is SARADC or GPIO on RK3588
find /proc/device-tree/ -name "recovery*" 2>/dev/null
echo "=== ADC keys ==="
find /proc/device-tree/ -path "*adc*" -not -path "*saradc*" 2>/dev/null | head -20
echo "=== Check for user LEDs ==="
for led in /sys/class/leds/*/; do echo "$led: $(cat ${led}trigger 2>/dev/null | head -c 200)"; echo; done

Ran terminal command:  ls /sys/class/leds/ 2>/dev/null; echo "---"; ls /sys/class/gpio/ 2>/dev/null

Ran terminal command:  cat /sys/class/leds/sys_led/trigger; echo "==="; cat /sys/class/leds/sys_led/brightness; echo "==="; cat /sys/class/leds/sys_led/max_brightness

Ran terminal command:  # Get the device tree info for sys_led
find /proc/device-tree/ -name "*led*" -o -name "*leds*" 2>/dev/null | grep -v __symbols__ | head -20

Ran terminal command:  find /proc/device-tree/ -name "*led*" 2>/dev/null | grep -v __symbols__

Ran terminal command:  # Get sys-led GPIO info
ls /proc/device-tree/leds/sys-led/
echo "=== gpios ==="
xxd /proc/device-tree/leds/sys-led/gpios 2>/dev/null
echo "=== label ==="
cat /proc/device-tree/leds/sys-led/label 2>/dev/null; echo
echo "=== default-state ==="
cat /proc/device-tree/leds/sys-led/default-state 2>/dev/null; echo
echo "=== compatible ==="
cat /proc/device-tree/leds/compatible 2>/dev/null | tr '\0' ' '; echo
echo "=== ls leds dir ==="
ls /proc/device-tree/leds/

Ran terminal command:  ls /proc/device-tree/leds/sys-led/ && cat /proc/device-tree/leds/sys-led/label 2>/dev/null && echo && xxd /proc/device-tree/leds/sys-led/gpios 2>/dev/null && echo "---" && cat /proc/device-tree/leds/compatible 2>/dev/null | tr '\0' '\n' && echo "---" && ls /proc/device-tree/leds/

Ran terminal command:  # Decode GPIO: phandle 0x010b, pin 0x0d (13), flags 0x01 (active low)
# Find which GPIO controller has phandle 0x010b
find /proc/device-tree/ -name phandle -exec sh -c 'v=$(xxd -p "$1" 2>/dev/null); if [ "$v" = "0000010b" ]; then echo "$1"; fi' _ {} \; 2>/dev/null

Ran terminal command:  find /proc/device-tree/ -maxdepth 2 -name phandle | while read f; do v=$(xxd -p "$f" 2>/dev/null); if [ "$v" = "0000010b" ]; then echo "$f"; fi; done
Completed with input:  find /proc/device-tree/ -maxdepth 2 -name phandle | while read f; do v=$(xxd -p "$f" 2>/dev/null); if [ "$v" = "0000010b" ]; then echo "$f"; fi; done

Ran terminal command:  # GPIO3 (fec50000), pin 13 = GPIO3_B5 (3*32+13=109)
# Check RK806 pwrkey details
echo "=== pwrkey ==="
ls /proc/device-tree/spi@feb20000/rk806single@0/pwrkey/
echo "=== board info ==="
cat /proc/device-tree/compatible | tr '\0' '\n'
echo "=== check if recovery is ADC key ==="
grep -r -l "recovery" /proc/device-tree/ 2>/dev/null | head -10

Ran terminal command:  ls /proc/device-tree/spi@feb20000/rk806single@0/pwrkey/ 2>/dev/null

Ran terminal command:  cat /proc/device-tree/spi@feb20000/rk806single@0/pwrkey/status | tr '\0' '\n'
echo "==="
cat /proc/device-tree/compatible | tr '\0' '\n'
echo "==="
# Check for any user-accessible GPIO keys (not power/recovery/maskrom)
# Check the LubanCat documentation or dts
find /proc/device-tree/ -name "status" -path "*key*" 2>/dev/null | while read f; do echo "$f: $(cat $f | tr '\0' ' ')"; done
echo "==="
# Check if there are any additional GPIO LEDs or user buttons in the DTS overlays
ls /boot/dtb/overlay/ 2>/dev/null | grep -i "led\|key\|button\|gpio" | head -20

Ran terminal command:  ls /boot/dtb/overlay/ 2>/dev/null | head -30

Ran terminal command:  # This is LubanCat-4 board (also called LubanCat-5?)
# On RK3588 LubanCat boards, the 3 buttons are:
# 1. Power key - managed by RK806 PMIC (system use - power on/off)
# 2. Recovery key - enters recovery/bootloader mode (system use)
# 3. MaskRom key - enters download mode (system use)
# These are ALL system buttons. No user-programmable buttons.
# Let's verify:
echo "=== dmesg keys ==="
dmesg | grep -i "key\|button\|input" | head -20
echo "=== GPIO banks ==="
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinmux-pins 2>/dev/null | head -10
echo "=== Available user GPIOs ==="
# Check which GPIO pins are free
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinmux-pins 2>/dev/null | grep "UNCLAIMED" | head -20

Ran terminal command:  dmesg | grep -i "key\|button\|input" 2>/dev/null | head -20

Ran terminal command:  # Check Recovery button - on LubanCat it's typically a SARADC button
# The Recovery button wired to SARADC_IN1 is checked by u-boot only, not Linux
# Check if there's an adc-keys node active in Linux
find /proc/device-tree/ -maxdepth 2 -name "adc-keys" 2>/dev/null
echo "==="
# Check if recovery GPIO is accessible 
# Also check the board's GPIO expansion header for available pins
cat /proc/device-tree/model | tr '\0' ' '; echo
echo "==="
# Check available GPIO pins on headers
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinmux-pins 2>/dev/null | grep "UNCLAIMED" | wc -l
echo "unclaimed GPIO pins"
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinmux-pins 2>/dev/null | grep "UNCLAIMED" | tail -20

Ran terminal command:  find /proc/device-tree/ -maxdepth 2 -name "adc-keys" 2>/dev/null; echo "---"; cat /proc/device-tree/model

Ran terminal command:  cat /proc/device-tree/model && echo

Ran terminal command:  # LubanCat-4 board. Check for recovery button (SARADC)
# On LubanCat-4, Recovery is connected to SARADC_IN1
# It's used by u-boot to enter recovery mode, but can be read in Linux via /dev/iio
ls /sys/bus/iio/devices/ 2>/dev/null
echo "==="
# Check available SARADC channels
for d in /sys/bus/iio/devices/iio:device*; do echo "$d:"; cat "$d/name" 2>/dev/null; ls "$d"/in_voltage*raw 2>/dev/null; done
echo "==="
# Check if recovery key has a kernel node
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinmux-pins 2>/dev/null | grep -c "UNCLAIMED"
echo " unclaimed pins"

Ran terminal command:  ls /sys/bus/iio/devices/ 2>/dev/null && echo "---" && for d in /sys/bus/iio/devices/iio:device*; do echo "$d"; cat "$d/name" 2>/dev/null; echo; done

Ran terminal command:  ls /sys/bus/iio/devices/iio:device0/in_voltage*raw 2>/dev/null && echo "---" && cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw 2>/dev/null && echo " (recovery button ADC value, ~0 when pressed, ~1023 when released)"

Recovery 按键ADC值是4090（未按下），说明它连在SARADC通道1上，可以在Linux中读取。现在我有足够信息来做完整分析了。 

Starting: *制定LED/KEY/eBPF/PHY开发计划* (3/3)

好的，完整分析结果出来了，给你梳理清楚：

---

## 1. Qt 里的 CPU 显示 — 不是 eBPF

你 Qt 里的 `lblCpu` 标签（mainwindow.cpp）是**直接读 stat 计算 CPU 占用率**的简单实现，和 eBPF 没有任何关系。这只是最基础的系统信息读取，**不能算 eBPF 系统监测**。

eBPF 的价值在于：**内核态插桩**，能做到函数级调用追踪、网络包分析、调度延迟分析等 proc 做不到的事情。

---

## 2. 板子上 3 个 KEY 的情况

你的 **Embedfire LubanCat-4** 板上 3 个按键全部是系统级用途：

| 按键 | 硬件接口 | Linux 中状态 | 能用吗？ |
|------|---------|-------------|---------|
| **电源键** | RK806 PMIC (SPI) | 已注册为 `rk805 pwrkey` input 设备 (event1) | **系统占用** — 长按关机/短按休眠 |
| **Recovery键** | SARADC 通道1 | 可通过 IIO 读取 (`in_voltage1_raw`, 当前值=4090) | **可复用** — Linux 启动后无人使用 |
| **MaskRom键** | 直连 SoC eFuse | 仅在断电时按住有用 | **不可用** — 纯硬件级，Linux 无法感知 |

**结论：只有 Recovery 键可以在 Linux 中作为用户按键使用**，通过 SARADC 读 ADC 值（按下≈0，松开≈4090）。

---

## 3. LED 情况

板上有一个 **sys_led**（GPIO3_B5, pin 109），当前被 `gpio-leds` 驱动管理，trigger 设为 `heartbeat`（心跳闪烁）。你可以：
- 直接写一个**自定义 LED 字符设备驱动**替代默认的 gpio-leds
- 或者用空闲 GPIO 外接 LED

---

## 4. 接下来的计划（P0 + P1）

根据你的情况调整后的优先执行顺序：

### 第一阶段：字符设备驱动补全（~4天）

| 序号 | 任务 | 方式 | 产出 |
|------|------|------|------|
| **1** | **LED 驱动**（platform driver） | 写 misc 字符设备驱动控制 GPIO3_B5，覆盖默认 gpio-leds | `/dev/my_led`，支持 ioctl 亮/灭/闪烁 |
| **2** | **KEY 驱动**（ADC 按键） | 写内核驱动读 SARADC 通道1，注册为 input_dev，上报 KEY 事件 | `/dev/input/eventX`，按键事件上报 |
| **3** | **Qt 集成** | 在 VehicleTerminal 中加 LED 控制 + KEY 响应 | UI 上显示/控制 |

### 第二阶段：eBPF 系统监测（~4天）

| 序号 | 任务 | 产出 |
|------|------|------|
| **4** | 安装 bpftrace/libbpf 工具链 | 编译环境就绪 |
| **5** | 写 3 个 eBPF 监测程序 | CPU调度延迟、内存分配追踪、网络包统计 |
| **6** | Qt 集成展示面板 | 替代现有的简单 stat 读取 |

### 第三阶段：PHY 驱动移植（~5天）

| 序号 | 任务 | 产出 |
|------|------|------|
| **7** | 分析现有 PHY（你板上已有 `jl2xxx` PHY） | 确认硬件方案 |
| **8** | 写/适配 PHY 驱动 + 设备树 | 千兆网络打通 |

### 第四阶段：RTSP 补全（~2天）

| 序号 | 任务 | 产出 |
|------|------|------|
| **9** | 基于 FFmpeg 加 RTSP server | 完善 Media 系统描述 |

---

**要开始第一步 LED 驱动吗？** 我可以直接带你写代码，从内核驱动到设备树 overlay 到用户空间测试，一步步来。




让我先全面了解你项目的现状，再对照升级路线图做差距分析。 

Full project inventory analysis

读取 [](file:///home/cat/chezai/driver)

读取 [](file:///home/cat/chezai/ai_voice)

读取 [](file:///home/cat/chezai/driver/fmcw_radar)

读取 [](file:///home/cat/chezai/driver/dht)

读取 [](file:///home/cat/chezai/README.md#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/readCodeSys.md)

读取 [](file:///home/cat/chezai/VehicleTerminal/README.md)

读取 [](file:///home/cat/chezai/ai_voice/whisper_qwen_unite.md#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/middleware/radar_processor.h)

读取 [](file:///home/cat/chezai/middleware/radar_processor.cpp#1-1)，行 1 到 150

读取 [](file:///home/cat/chezai/ai_voice/RK3588S_LLM_%E9%83%A8%E7%BD%B2%E6%B5%81%E7%A8%8B.md#1-1)，行 1 到 150

读取 [](file:///home/cat/chezai/ai_voice/voice_pipeline_test.sh#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/camera/v4l2_capture.c#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/driver/fmcw_radar/fmcw_radar.c#1-1)，行 1 到 80

读取 [](file:///home/cat/chezai/VehicleTerminal/mainwindow.h#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/VehicleTerminal/rtmp_streamer.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/mjpeg_server.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/camera_thread.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/speechrecognition.h)

读取 [](file:///home/cat/chezai/VehicleTerminal)

读取 [](file:///home/cat/chezai/driver/dht/dht11_drv.c#1-1)，行 1 到 120

读取 [](file:///home/cat/chezai/docs/DRM_%E9%A9%B1%E5%8A%A8%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0.md#1-1)，行 1 到 150

读取 [](file:///home/cat/chezai/VehicleTerminal/dht11.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/main.cpp)

读取 [](file:///home/cat/chezai/docs/V4L2_%E9%A9%B1%E5%8A%A8%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0.md#1-1)，行 1 到 150

读取 [](file:///home/cat/chezai/driver/spi)

读取 [](file:///home/cat/chezai/ai_voice/voice)

读取 [](file:///home/cat/chezai/driver/fmcw_radar/radar_bringup.md#1-1)，行 1 到 100

读取 [](file:///home/cat/chezai/VehicleTerminal/voice_assistant.h)

读取 [](file:///home/cat/chezai/qt_app/radar_reader.h)

读取 [](file:///home/cat/chezai/qt_app/main.cpp)

搜索正则表达式 `ebpf|eBPF|RTOS|RPMSG|MPU6050|network|PHY`，40 个结果

读取 [](file:///home/cat/chezai/camera)

读取 [](file:///home/cat/chezai/qt_app/qt_app.pro)

读取 [](file:///home/cat/chezai/VehicleTerminal/VehicleTerminal.pro)

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor)

搜索文本 `TODO|FIXME|missing|incomplete|不完整`，50 个结果

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor/monitor.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/Music/musicplayer.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/Map/baidumap.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/Weather/weather.h)

读取 [](file:///home/cat/chezai/VehicleTerminal/camera_thread.cpp#1-1)，行 1 到 200

读取 [](file:///home/cat/chezai/VehicleTerminal/voice_assistant.cpp#1-1)，行 1 到 100

读取内存 repo

读取 [](file:///home/cat/.vscode-server/data/User/workspaceStorage/3a3957e9c71ef842a6d3fbb4943ffa37/GitHub.copilot-chat/chat-session-resources/b8cd51c1-da9f-40a6-9961-8329079e7245/toolu_vrtx_01WrVVSmcGbe7BXBgqywxY47__vscode-1773556906251/content.txt#1-1)，行 1 到 300

Check AMP/Net/eBPF/RTSP status

搜索正则表达式 `RTOS|RPMSG|AMP|heterogeneous`，20 个结果

搜索正则表达式 `ethernet|PHY|MAC driver|RGMII|RMII|eth_phy`，20 个结果

搜索正则表达式 `eBPF|bpftrace|bpf_program`，3 个结果

搜索正则表达式 `RTSP|rtsp`，20 个结果

搜索正则表达式 `dual.*camera|second.*camera|camera.*1|/dev/video1`，20 个结果

搜索正则表达式 `MPU6050|mpu6050|IMU`，20 个结果

搜索正则表达式 `LED driver|LED_GPIO|key driver|button driver|GPIO_KEY`，无结果

搜索正则表达式 `#include.*opencv2|opencv`，20 个结果

搜索正则表达式 `LVGL|lvgl`，1 个结果

搜索匹配 `**/*.bpf.c` 的文件，无匹配项

读取 [](file:///home/cat/chezai/docs/DRM_%E9%A9%B1%E5%8A%A8%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0.md#1-1)，行 1 到 100

搜索正则表达式 `AP3216|light sensor|proximity|als`，20 个结果

搜索正则表达式 `GPIO|gpio_key|button|LED_|LED|input_device`，50 个结果

搜索正则表达式 `FreeRTOS|STM32`，20 个结果

读取 [](file:///home/cat/chezai/driver)

读取 [](file:///home/cat/chezai/middleware)

读取内存 repo

搜索正则表达式 `MPU-6050|mpu6050|IMU|6-axis|accelerometer`，20 个结果

搜索正则表达式 `LVGL|lv_|ui library`，1 个结果

搜索正则表达式 `#include.*opencv`，20 个结果

读取 [](file:///home/cat/chezai/README.md#1-1)，行 1 到 50

读取 [](file:///home/cat/chezai/README.md#60-60)，行 60 到 150

读取 [](file:///home/cat/chezai/camera/imx415_driver)

搜索正则表达式 `ethernet|eth|internet|network driver` (`**/*.dts`)，无结果

读取 [](file:///home/cat/chezai/VehicleTerminal/Monitor/capture_thread.h#1-1)，行 1 到 60

读取内存 fmcw_radar_spi_mode_debug_note.md

已创建记忆文件 [](file:///home/cat/.vscode-server/data/User/workspaceStorage/3a3957e9c71ef842a6d3fbb4943ffa37/GitHub.copilot-chat/memory-tool/memories/YjhjZDUxYzEtZGE5Zi00MGE2LTk5NjEtODMyOTA3OWU3MjQ1/roadmap_analysis_results.md)

## 项目现状 vs 升级路线图 差距分析

以下是你当前项目对照图片中各子系统的完成度，以及面向**实习求职**的优先级建议：

---

### 各子系统完成度一览

| 子系统 | 路线图要求 | 当前状态 | 完成度 |
|--------|-----------|---------|--------|
| **AI系统** | NPU部署 + 语音交互 | Whisper STT + Qwen2.5 LLM + espeak TTS，端侧全链路跑通 | **90%** |
| **Media系统** | 双路摄像头 + V4L2 + FFmpeg/OpenCV + RTSP | 单路IMX415 V4L2 + MPP硬编码 + RTMP推流 + MJPEG，缺RTSP和双路 | **60%** |
| **Monitor系统** | 字符设备(MPU6050/DHT11/LED/key) + eBPF | DHT11驱动 + AP3216C，缺MPU6050/LED/Key驱动，eBPF为零 | **30%** |
| **AMP系统** | 异构RTOS + Linux + RPMSG | 完全缺失（STM32走SPI，非RPMSG框架） | **0%** |
| **GUI** | QT / LVGL | Qt 5.15 完整应用（三栏布局、多模块集成） | **85%** |
| **Display系统** | 屏幕 + DRM驱动 | 仅有学习笔记，无驱动代码 | **10%** |
| **Net系统** | MAC + ETH PHY驱动移植 | TODO，完全缺失 | **0%** |

---

### 面向实习的优先级排序

> 核心原则：**面试官看的是深度 > 广度，能讲清原理 + 踩坑经验的1个模块 > 浮于表面的5个模块。**

#### P0 — 立刻做（投简历前必须有）

1. **eBPF 系统监测**
   - 理由：这是当前**最热门的Linux内核技术方向**，大厂（字节/阿里/华为）嵌入式/系统岗面试高频考点。实现一个简单的 CPU/内存/网络监测 + 可视化即可，成本低但简历亮点极高。
   - 工作量：~3-5天（bpftrace脚本 + libbpf C程序 + Qt展示面板）

2. **补齐 Monitor 驱动（MPU6050 + LED + Key）**
   - 理由：字符设备驱动是嵌入式面试**最基础的考核点**。你已有DHT11驱动的经验，MPU6050(I2C)、LED(gpio-leds)、Key(gpio-keys)都是标准Linux驱动子系统，能证明你**系统性掌握了字符设备框架**而非只会一个。
   - 工作量：~3-5天

#### P1 — 优先做（明显提升简历竞争力）

3. **千兆网络 PHY 驱动移植**
   - 理由：这是整个项目**标题关键词**（"基于千兆网络PHY驱动的…"），面试官一定会问。涉及 MDIO/PHY 子系统、设备树配置、RGMII 时序调优，属于驱动岗核心能力。
   - 工作量：~5-7天（设备树修改 + PHY驱动适配 + 网络联通测试）

4. **RTSP 替代/补充 RTMP**
   - 理由：Media系统差一个RTSP就完整了。用 live555 或 FFmpeg 的 RTSP server 封装，工作量不大，但能补全"双路摄像头 + RTSP"的完整描述。
   - 工作量：~2-3天

#### P2 — 有余力再做（锦上添花）

5. **DRM 显示驱动**
   - 理由：你已有笔记基础，写一个 Rockchip VOP2 + MIPI DSI 的显示pipeline初始化代码，能证明驱动全栈能力（输入→处理→输出都覆盖）。
   - 工作量：~5-7天

6. **AMP 异构通信（RPMSG）**
   - 理由：RK3588本身支持 MCU 核(Cortex-M0)跑RTOS + Linux大核的AMP模式，但调试门槛高、坑多。如果目标岗位不是专门做异构的，可以先跳过。
   - 工作量：~7-14天

#### P3 — 可暂缓

7. **LVGL** — 你已有完整Qt应用，LVGL画蛇添足
8. **双路摄像头** — 设备树加一路CSI即可，面试时口述方案足够

---

### 总结建议

**你的项目已经有很好的基础**，AI语音链路 + FMCW雷达 + V4L2 + Qt整合是亮点。但对标路线图，关键短板是：

| 短板 | 影响 |
|------|------|
| eBPF = 0 | 错失最热门技术加分项 |
| PHY驱动 = 0 | 项目标题核心词无法自圆其说 |
| 字符设备只有1个 | 无法证明驱动能力系统性 |

**如果只有2周准备时间**，建议按顺序做：

> **MPU6050驱动 → LED/Key驱动 → eBPF基础监测 → PHY驱动移植**

完成这4项后，你的项目在嵌入式Linux实习市场上会非常有竞争力。