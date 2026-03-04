cmake_minimum_required(VERSION 3.16)
project(ai_voice VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------
# 依赖说明：
#   - whisper.cpp: https://github.com/ggerganov/whisper.cpp
#   - llama.cpp:   https://github.com/ggerganov/llama.cpp
#   - espeak-ng:   apt install espeak-ng libespeak-ng-dev
#
# 使用前请先编译好 whisper.cpp 和 llama.cpp，并设置:
#   WHISPER_DIR: whisper.cpp 源码根目录
#   LLAMA_DIR:   llama.cpp 源码根目录
# -------------------------------------------------------

include_directories(include)

# --- 源文件 ---
set(AI_VOICE_SOURCES
    src/local_asr.cpp
    src/llm_engine.cpp
    src/tts_engine.cpp
)

# --- 静态库（供 VehicleTerminal Qt 主程序链接）---
add_library(ai_voice_lib STATIC ${AI_VOICE_SOURCES})

# --- 独立测试可执行文件 ---
add_executable(ai_voice_test src/main.cpp ${AI_VOICE_SOURCES})

# --- 链接 whisper.cpp（可用时取消注释）---
# find_library(WHISPER_LIB whisper HINTS ${WHISPER_DIR})
# target_include_directories(ai_voice_lib PUBLIC ${WHISPER_DIR})
# target_link_libraries(ai_voice_lib ${WHISPER_LIB})

# --- 链接 llama.cpp（可用时取消注释）---
# find_library(LLAMA_LIB llama HINTS ${LLAMA_DIR})
# target_include_directories(ai_voice_lib PUBLIC ${LLAMA_DIR})
# target_link_libraries(ai_voice_lib ${LLAMA_LIB})

# --- 链接 espeak-ng（可用时取消注释）---
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(ESPEAK REQUIRED espeak-ng)
# target_include_directories(ai_voice_lib PUBLIC ${ESPEAK_INCLUDE_DIRS})
# target_link_libraries(ai_voice_lib ${ESPEAK_LIBRARIES})

# pthread（llama.cpp 需要）
find_package(Threads REQUIRED)
target_link_libraries(ai_voice_lib Threads::Threads)
target_link_libraries(ai_voice_test Threads::Threads)
