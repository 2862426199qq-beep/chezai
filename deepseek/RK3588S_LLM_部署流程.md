# 鲁班猫4 (RK3588S) LLM 边缘部署流程

> Qwen2.5-0.5B W8A8 量化部署指南

---

## 环境说明

| 项目 | 配置 |
|------|------|
| PC 系统 | Ubuntu (x86, VMware VM) |
| 目标板 | 鲁班猫4 RK3588S，4GB RAM，32GB 存储 |
| NPU 驱动 | rknpu v0.9.8 |
| RKLLM 版本 | 1.2.3 |
| 部署模型 | Qwen2.5-0.5B-Instruct，W8A8 量化 |
| Python 环境 | Miniforge3 + conda rkllm (Python 3.10) |

---

## 一、PC 端环境准备

### Step 1：安装 Miniforge 和创建 conda 环境

```bash
wget https://github.com/conda-forge/miniforge/releases/download/25.11.0-0/Miniforge3-25.11.0-0-Linux-x86_64.sh
bash Miniforge3-25.11.0-0-Linux-x86_64.sh
source ~/miniforge3/bin/activate
conda create -n rkllm python=3.10
conda activate rkllm
```

### Step 2：克隆 rknn-llm 并安装 Toolkit

```bash
git clone https://github.com/airockchip/rknn-llm.git
cd rknn-llm/rkllm-toolkit/packages
pip install rkllm_toolkit-1.2.3-cp310-cp310-linux_x86_64.whl -r requirements.txt \
  -i https://pypi.tuna.tsinghua.edu.cn/simple
```

> ⚠️ 注意：setuptools 需降级到 <70 版本，否则报 `pkg_resources` 错误
> ```bash
> pip install "setuptools<70" -i https://pypi.tuna.tsinghua.edu.cn/simple
> ```

### Step 3：下载模型

```bash
pip install modelscope -i https://pypi.tuna.tsinghua.edu.cn/simple
python -c "
from modelscope import snapshot_download
snapshot_download('Qwen/Qwen2.5-0.5B-Instruct', local_dir='./Qwen2.5-0.5B')
"
```

---

## 二、模型量化转换

### Step 4：生成量化校准数据

```bash
cd rknn-llm/examples/rkllm_api_demo/export
python generate_data_quant.py -m ~/chezai_pc/deepseek/Qwen2.5-0.5B
```

> 耗时约 10~15 分钟，VM 内存需 ≥ 8GB，否则会被 OOM Kill

### Step 5：修改并执行转换脚本

修改 `export_rkllm.py`：

```python
modelpath = '/home/xxy/chezai_pc/deepseek/Qwen2.5-0.5B'
# device 改为 cpu
ret = llm.load_huggingface(model=modelpath, model_lora=None, device='cpu', dtype="float16", ...)
```

执行转换：

```bash
python export_rkllm.py
```

> 耗时约 10~20 分钟，生成 `Qwen2.5-0.5B_W8A8_RK3588.rkllm`（约 0.5GB）

---

## 三、交叉编译推理程序

### Step 6：安装交叉编译工具链

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake -y
```

### Step 7：修改编译脚本并编译

修改 `build-linux.sh` 中编译器路径：

```bash
GCC_COMPILER_PATH=/usr/bin/aarch64-linux-gnu
```

执行编译：

```bash
cd rknn-llm/examples/rkllm_api_demo/deploy
chmod +x build-linux.sh
./build-linux.sh
```

> 生成文件：`install/demo_Linux_aarch64/llm_demo`

### Step 8：修改 callback 过滤异常 token

在 `src/llm_demo.cpp` 中将 callback 函数替换为：

```cpp
int callback(RKLLMResult *result, void *userdata, LLMCallState state)
{
    if (state == RKLLM_RUN_FINISH) {
        printf("\n");
    } else if (state == RKLLM_RUN_ERROR) {
        printf("run error\n");
    } else if (state == RKLLM_RUN_NORMAL) {
        std::string text = result->text;
        if (text.find("[PAD") == std::string::npos) {
            printf("%s", text.c_str());
            fflush(stdout);
        }
    }
    return 0;
}
```

修改后重新编译：

```bash
./build-linux.sh
```

---

## 四、部署到板子并运行

### Step 9：传输文件到板子

```bash
# 传推理程序和库
scp -r install/demo_Linux_aarch64 cat@192.168.137.215:~/chezai/deepseek/

# 传模型文件
scp Qwen2.5-0.5B_W8A8_RK3588.rkllm cat@192.168.137.215:~/chezai/deepseek/
```

### Step 10：板子上运行

```bash
cd ~/chezai/deepseek/demo_Linux_aarch64
export LD_LIBRARY_PATH=./lib
ulimit -n 102400
taskset f0 ./llm_demo ~/chezai/deepseek/Qwen2.5-0.5B_W8A8_RK3588.rkllm 1024 2048
```

---

## 五、常见问题

| 问题 | 解决方案 |
|------|----------|
| 安装被 OOM Killed | VM 内存调至 12GB |
| `pkg_resources` 报错 | `pip install "setuptools<70"` |
| `cmake` 未找到 | `sudo apt install cmake -y` |
| 输出 `[PAD151935]` | callback 中过滤 `[PAD` 开头的 token |
| 推理 OOM Killed | 换小模型（0.5B），减小 context 参数 |
| scp 找不到板子 | VM 网络改为桥接模式 |
| 下载速度慢 | pip 加 `-i https://pypi.tuna.tsinghua.edu.cn/simple` |

---

## 六、性能参考（RK3588S 4GB）

| 指标 | Qwen2.5-0.5B W8A8 |
|------|-------------------|
| 模型内存占用 | ~0.5GB |
| 总内存占用 | ~1.3GB |
| 推理速度 | 15~25 token/秒 |
| 响应延迟 | 1~3 秒 |
| 适合场景 | 意图识别、简单问答、结构化指令输出 |

---

## 七、架构说明

```
x86 Ubuntu (VM)                    鲁班猫4 (RK3588S)
───────────────────                ─────────────────────
1. 下载原始模型                     4. 运行 llm_demo
2. 量化转换 → .rkllm   ──scp──→       加载 .rkllm
3. 交叉编译 → llm_demo ──scp──→       NPU 推理
```

**下一步：接入 LLM_Voice_Flow**

```
麦克风 → ASR（语音转文字）→ LLM（Qwen2.5-0.5B）→ TTS（文字转语音）→ 扬声器
                                    ↓
                              Qt 指令执行
```
