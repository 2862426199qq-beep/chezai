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
# 真实模型集成（USE_REAL_MODELS=ON 时激活）
# -------------------------------------------------------
if(USE_REAL_MODELS)
    message(STATUS "[ai_voice] 真实模型模式已启用")
    target_compile_definitions(ai_voice_lib  PUBLIC USE_REAL_MODELS)
    target_compile_definitions(ai_voice_test PUBLIC USE_REAL_MODELS)

    # whisper.cpp
    if(WHISPER_DIR)
        find_library(WHISPER_LIB whisper HINTS "${WHISPER_DIR}/build" "${WHISPER_DIR}")
        if(WHISPER_LIB)
            target_include_directories(ai_voice_lib PUBLIC "${WHISPER_DIR}")
            target_link_libraries(ai_voice_lib ${WHISPER_LIB})
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
# pthread（llama.cpp 推理需要，以及 TtsEngine 工作线程）
# -------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(ai_voice_lib  Threads::Threads)
target_link_libraries(ai_voice_test Threads::Threads)

