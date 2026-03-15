# Whisper RKNN 部署记录 (RK3588S)

> 参考教程：https://doc.embedfire.com/linux/rk356x/Ai/zh/latest/lubancat_ai/example/whisper.html  
> 记录日期：2026-03-09  
> 工作目录：`/home/cat/chezai/ai_voice/whisper`

---

## 一、方案说明

### 1.1 整体架构

```
麦克风(es8388) → 录音(arecord) → [Whisper STT] → 文本 → [Qwen2.5-0.5B LLM] → JSON指令 → Qt操作
```

Qwen2.5-0.5B 是纯文本模型，无法直接处理音频，需要 Whisper 作为 STT（语音转文字）前端。

### 1.2 模型选择

| 项目 | 配置 |
|------|------|
| 模型 | Whisper base (20秒版) |
| 框架 | RKNN (NPU 加速) |
| 输入 | 16kHz WAV，≤20秒 |
| 输出 | 识别文本 |
| RTF | ~0.46 (5秒音频约2.6秒处理) |
| 内存 | ~200-300MB |

---

## 二、PC 端模型转换 (Ubuntu x86 VM)

### 2.1 环境准备

```bash
conda create -n whisper python=3.10
conda activate whisper

# 安装 PyTorch
conda install pytorch torchvision pytorch-cuda=12.1 -c pytorch -c nvidia

# 克隆 whisper v20231117（必须此版本，新版不兼容 rknn 转换）
git clone https://github.com/openai/whisper -b v20231117
cd whisper
pip install -r requirements.txt

# 固定兼容版本
pip install onnx==1.16.1 numpy==1.26.4 protobuf==4.25.4
```

### 2.2 修改 Whisper 源码（20秒版）

**whisper/audio.py**：
```python
# 第16行
CHUNK_LENGTH = 20  # 原为 30
```

**whisper/model.py**：
```python
# 注释掉 assert
# assert x.shape[1:] == self.positional_embedding.shape, "incorrect audio shape"

# 位置编码改为切片尾部
x = (x + self.positional_embedding[-x.shape[1]:,:]).to(x.dtype)
```

### 2.3 导出 ONNX

从 rknn_model_zoo 获取 `export_onnx.py`：
```bash
python export_onnx.py --model_type base
# 生成：whisper_encoder_base.onnx, whisper_decoder_base.onnx
```

### 2.4 转换为 RKNN

安装 rknn-toolkit2：
```bash
pip install rknn-toolkit2  # 版本 2.3.x
```

转换（使用 rknn_model_zoo 的 convert.py）：
```bash
python convert.py whisper_encoder_base_20s.onnx rk3588 fp whisper_encoder_base_20s.rknn
python convert.py whisper_decoder_base_20s.onnx rk3588 fp whisper_decoder_base_20s.rknn
```

### 2.5 产出文件

```
whisper_encoder_base_20s.rknn  43M  sha256:b202e48cba99ccca06c9e3327996e4625a1debdb61990bc3ea2ff1f7234d336a
whisper_decoder_base_20s.rknn 153M  sha256:a9e69b6290e42bcc1326517ae024316d0ed79ec1ed200549b532f88349eb4235
```

传输到板卡：
```bash
scp whisper_encoder_base_20s.rknn whisper_decoder_base_20s.rknn cat@<板卡IP>:~/chezai/ai_voice/whisper/
```

---

## 三、板卡端编译部署

### 3.1 安装依赖

```bash
sudo apt update
sudo apt install -y libfftw3-dev libsndfile1-dev libsdl2-dev cmake g++
```

### 3.2 克隆 rknn_model_zoo

```bash
cd /home/cat/chezai/ai_voice/whisper
git clone https://github.com/airockchip/rknn_model_zoo.git
```

### 3.3 修改编译配置（本地编译适配）

rknn_model_zoo 默认使用预编译的交叉编译静态库，板卡本地编译需改为系统库。

修改 `rknn_model_zoo/3rdparty/CMakeLists.txt`：

**fftw 部分**：
```cmake
# fftw - use system library for native compilation
set(LIBFFTW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/fftw)
set(LIBFFTW_INCLUDES /usr/include PARENT_SCOPE)
set(LIBFFTW fftw3f PARENT_SCOPE)
```

**libsndfile 部分**：
```cmake
# libsndfile - use system library for native compilation
set(LIBSNDFILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libsndfile)
set(LIBSNDFILE_INCLUDES /usr/include PARENT_SCOPE)
set(LIBSNDFILE sndfile PARENT_SCOPE)
```

### 3.4 编译

```bash
cd /home/cat/chezai/ai_voice/whisper/rknn_model_zoo
export CC=gcc && export CXX=g++
export GCC_COMPILER=/usr/bin/aarch64-linux-gnu
bash build-linux.sh -t rk3588 -a aarch64 -d whisper -b Release
```

编译产物在：`install/rk3588_linux_aarch64/rknn_whisper_demo/`

### 3.5 部署模型

```bash
cp ~/chezai/ai_voice/whisper/whisper_encoder_base_20s.rknn \
   ~/chezai/ai_voice/whisper/rknn_model_zoo/install/rk3588_linux_aarch64/rknn_whisper_demo/model/
cp ~/chezai/ai_voice/whisper/whisper_decoder_base_20s.rknn \
   ~/chezai/ai_voice/whisper/rknn_model_zoo/install/rk3588_linux_aarch64/rknn_whisper_demo/model/
```

---

## 四、测试验证

### 4.1 英文识别

```bash
cd ~/chezai/ai_voice/whisper/rknn_model_zoo/install/rk3588_linux_aarch64/rknn_whisper_demo
./rknn_whisper_demo ./model/whisper_encoder_base_20s.rknn ./model/whisper_decoder_base_20s.rknn en ./model/test_en.wav
```

**输出结果：**
```
Whisper output:  Mr. Quilter is the apostle of the middle classes and we are glad to welcome his gospel.
Real Time Factor (RTF): 3.199 / 5.855 = 0.546
```

### 4.2 中文识别

```bash
./rknn_whisper_demo ./model/whisper_encoder_base_20s.rknn ./model/whisper_decoder_base_20s.rknn zh ./model/test_zh.wav
```

**输出结果：**
```
Whisper output: 对我做了介绍,我想说的是大家如果对我的研究感兴趣
Real Time Factor (RTF): 2.602 / 5.611 = 0.464
```

### 4.3 板载麦克风实录测试

```bash
# 录音（es8388 需双声道，whisper demo 自动转单声道）
arecord -D hw:2,0 -f S16_LE -r 16000 -c 2 -d 5 test_mic.wav

# 识别
./rknn_whisper_demo ./model/whisper_encoder_base_20s.rknn ./model/whisper_decoder_base_20s.rknn zh test_mic.wav
```

**输出：** 自动完成 `convert_channels: 2 -> 1`，链路正常。

---

## 五、使用方法

```bash
# 用法
./rknn_whisper_demo <encoder_path> <decoder_path> <task> <audio_path>

# task: en（英文）、zh（中文）
# audio_path: 16kHz WAV 文件，≤20秒
```

---

## 六、性能数据

| 指标 | 数值 |
|------|------|
| Encoder 加载 | ~125ms |
| Decoder 加载 | ~190ms |
| 推理（5秒音频） | ~2.6-3.2秒 |
| RTF | 0.46~0.55 |
| 静音推理 | ~0.4秒 |

---

## 七、目录结构

```
/home/cat/chezai/ai_voice/whisper/
├── whisper_bringup.md              # 本文档
├── whisper_encoder_base_20s.rknn   # Encoder 模型 (43M)
├── whisper_decoder_base_20s.rknn   # Decoder 模型 (153M)
└── rknn_model_zoo/                 # 编译工程
    └── install/rk3588_linux_aarch64/rknn_whisper_demo/
        ├── rknn_whisper_demo       # 可执行文件
        ├── lib/                    # 运行库
        └── model/                  # 模型和资源文件
```

---

## 八、遇到的问题

| 问题 | 解决方案 |
|------|----------|
| 磁盘空间不足（29G 用了 26G） | /var/log 日志堆积 12G，清理后释放 15G |
| rknn-toolkit2 仅 x86 | 模型转换移到 PC 端 VM 完成 |
| 本地编译 fftw 静态库 -fPIC 报错 | 改用系统 `libfftw3-dev` 动态库 |
| es8388 不支持单声道录音 | 录双声道，whisper demo 自动转单声道 |
| whisper v20240930 与 rknn 不兼容 | 使用 v20231117 版本 |
| rknn-toolkit2 与新版 onnx 冲突 | 固定 onnx==1.16.1, numpy==1.26.4, protobuf==4.25.4 |
