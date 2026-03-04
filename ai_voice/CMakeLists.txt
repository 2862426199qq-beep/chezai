cmake_minimum_required(VERSION 3.16)
project(ai_voice VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------
# 构建选项
# -------------------------------------------------------
option(USE_REAL_MODELS
    "接入真实 whisper.cpp / llama.cpp 模型（需要预先编译好依赖库）" OFF)

option(WHISPER_DIR "whisper.cpp 源码根目录" "")
option(LLAMA_DIR   "llama.cpp 源码根目录"   "")

# ALSA 麦克风采集（USE_ALSA=ON 时启用真实麦克风录音，否则使用 stub）
option(USE_ALSA "启用 ALSA 麦克风采集（需要 libasound2-dev）" OFF)

# -------------------------------------------------------
# 头文件目录
# -------------------------------------------------------
include_directories(include)

# -------------------------------------------------------
# 源文件
# -------------------------------------------------------
set(AI_VOICE_SOURCES
    src/local_asr.cpp
    src/llm_engine.cpp
    src/tts_engine.cpp
    src/voice_pipeline.cpp
)

# -------------------------------------------------------
# 静态库（供 VehicleTerminal Qt 主程序链接）
# -------------------------------------------------------
add_library(ai_voice_lib STATIC ${AI_VOICE_SOURCES})

# -------------------------------------------------------
# 独立测试可执行文件
# -------------------------------------------------------
add_executable(ai_voice_test src/main.cpp ${AI_VOICE_SOURCES})

# -------------------------------------------------------
# ASR 单独调试工具（麦克风录音 → ASR 识别，终端测试）
# -------------------------------------------------------
add_executable(asr_test
    src/asr_test.cpp
    src/mic_capture.cpp
    src/local_asr.cpp
)

# -------------------------------------------------------
# 真实模型集成（USE_REAL_MODELS=ON 时激活）
# -------------------------------------------------------
if(USE_REAL_MODELS)
    message(STATUS "[ai_voice] 真实模型模式已启用")
    target_compile_definitions(ai_voice_lib  PUBLIC USE_REAL_MODELS)
    target_compile_definitions(ai_voice_test PUBLIC USE_REAL_MODELS)
    target_compile_definitions(asr_test      PUBLIC USE_REAL_MODELS)

    # whisper.cpp
    if(WHISPER_DIR)
        find_library(WHISPER_LIB whisper HINTS "${WHISPER_DIR}/build" "${WHISPER_DIR}")
        if(WHISPER_LIB)
            target_include_directories(ai_voice_lib PUBLIC "${WHISPER_DIR}")
            target_include_directories(asr_test     PUBLIC "${WHISPER_DIR}")
            target_link_libraries(ai_voice_lib ${WHISPER_LIB})
            target_link_libraries(asr_test     ${WHISPER_LIB})
        else()
            message(WARNING "未找到 libwhisper，请先编译 whisper.cpp")
        endif()
    endif()

    # llama.cpp
    if(LLAMA_DIR)
        find_library(LLAMA_LIB llama HINTS "${LLAMA_DIR}/build" "${LLAMA_DIR}")
        if(LLAMA_LIB)
            target_include_directories(ai_voice_lib PUBLIC "${LLAMA_DIR}/include" "${LLAMA_DIR}")
            target_link_libraries(ai_voice_lib ${LLAMA_LIB})
        else()
            message(WARNING "未找到 libllama，请先编译 llama.cpp")
        endif()
    endif()

    # espeak-ng（可选，用于真实 TTS）
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ESPEAK QUIET espeak-ng)
        if(ESPEAK_FOUND)
            target_include_directories(ai_voice_lib PUBLIC ${ESPEAK_INCLUDE_DIRS})
            target_link_libraries(ai_voice_lib ${ESPEAK_LIBRARIES})
        endif()
    endif()
else()
    message(STATUS "[ai_voice] Stub 模式（不需要真实模型）")
endif()

# -------------------------------------------------------
# ALSA 麦克风采集（USE_ALSA=ON 时启用真实录音）
# -------------------------------------------------------
if(USE_ALSA)
    message(STATUS "[ai_voice] ALSA 麦克风采集已启用")
    find_library(ASOUND_LIB asound)
    if(ASOUND_LIB)
        target_compile_definitions(asr_test PUBLIC USE_ALSA)
        target_link_libraries(asr_test ${ASOUND_LIB})
    else()
        message(WARNING "未找到 libasound，请安装 libasound2-dev")
    endif()
else()
    message(STATUS "[ai_voice] ALSA Stub 模式（录音输出静音 WAV）")
endif()

# -------------------------------------------------------
# pthread（llama.cpp 推理需要，以及 TtsEngine 工作线程）
# -------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(ai_voice_lib  Threads::Threads)
target_link_libraries(ai_voice_test Threads::Threads)
target_link_libraries(asr_test      Threads::Threads)

