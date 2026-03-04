#include "tts_engine.h"
#include <iostream>
#include <cstdlib>

// 过滤 shell 危险字符，保留 UTF-8 中文和安全 ASCII
static std::string sanitizeForShell(const std::string &text)
{
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text) {
        if (c > 0x7F || std::isalnum(c) || c == ' ' || c == ',' ||
            c == '.' || c == '!' || c == '?' || c == '\'') {
            result += static_cast<char>(c);
        }
        // 丢弃 $  `  "  \  ;  &  |  ( ) 等 shell 元字符
    }
    return result;
}

// 调用 espeak-ng 命令行（TODO: 改为 libespeak-ng API 可消除 fork 开销）
static void espeak(const std::string &safe_text)
{
    if (safe_text.empty()) {
        return;
    }
    std::string cmd = "espeak-ng -v zh -s 150 -a 80 -- \"" + safe_text + "\" 2>/dev/null";
    if (std::system(cmd.c_str()) != 0) {
        std::cerr << "[TtsEngine] espeak-ng 执行失败（apt install espeak-ng）" << std::endl;
    }
}

// ── 工作线程：从队列中取句子逐个播报 ─────────────────────────────────────
void TtsEngine::workerLoop()
{
    while (true) {
        std::string sentence;
        {
            std::unique_lock<std::mutex> lk(m_mtx);
            m_cv.wait(lk, [this] {
                return !m_queue.empty() || !m_running.load();
            });

            if (!m_running.load() && m_queue.empty()) {
                break;
            }
            if (m_queue.empty()) {
                continue;
            }
            sentence = std::move(m_queue.front());
            m_queue.pop();
        }

        // 在锁外播报，不阻塞生产者
        espeak(sanitizeForShell(sentence));

        // 检查队列是否已清空，通知 waitDone()
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (m_queue.empty()) {
                m_cv_done.notify_all();
            }
        }
    }
}

TtsEngine::TtsEngine()
{
    m_running.store(true);
    m_worker = std::thread(&TtsEngine::workerLoop, this);
}

TtsEngine::~TtsEngine()
{
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_running.store(false);
    }
    m_cv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void TtsEngine::speak(const std::string &text)
{
    if (text.empty()) {
        return;
    }
    std::cout << "[TtsEngine] 同步播报: " << text << std::endl;
    espeak(sanitizeForShell(text));
}

void TtsEngine::speakAsync(const std::string &text)
{
    enqueue(text);  // 直接复用队列，无需额外线程
}

void TtsEngine::enqueue(const std::string &sentence)
{
    if (sentence.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_queue.push(sentence);
    }
    m_cv.notify_one();
}

void TtsEngine::waitDone()
{
    std::unique_lock<std::mutex> lk(m_mtx);
    m_cv_done.wait(lk, [this] { return m_queue.empty(); });
}
