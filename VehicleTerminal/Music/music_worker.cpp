/*
 * music_worker.cpp — 音乐播放工作线程实现
 *
 * 所有 do* 槽函数都在工作线程中执行 (通过 moveToThread + QueuedConnection),
 * 因此 QMediaPlayer 的阻塞操作 (stop/play) 不会卡 UI。
 */

#include "Music/music_worker.h"
#include <QDebug>

MusicWorker::MusicWorker(QObject *parent)
    : QObject(parent)
    , m_player(nullptr)
    , m_playlist(nullptr)
{
    m_player = new QMediaPlayer(this);
    m_playlist = new QMediaPlaylist(this);
    m_playlist->clear();
    m_player->setPlaylist(m_playlist);
    m_playlist->setPlaybackMode(QMediaPlaylist::Random);

    /* 转发 QMediaPlayer 的信号给 UI 线程 */
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &MusicWorker::durationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &MusicWorker::positionChanged);

    /* 当播放列表自动切歌时 (如播完一首自动跳下一首), 通知 UI 更新高亮 */
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged,
            this, &MusicWorker::currentIndexChanged);
}

MusicWorker::~MusicWorker()
{
}

/* ---- 播放列表管理 (主线程初始化阶段调用, moveToThread 之前) ---- */

bool MusicWorker::addMedia(const QString &filePath)
{
    return m_playlist->addMedia(QUrl::fromLocalFile(filePath));
}

bool MusicWorker::isEmpty() const
{
    return m_playlist->isEmpty();
}

int MusicWorker::mediaCount() const
{
    return m_playlist->mediaCount();
}

int MusicWorker::nextIndex(int steps) const
{
    return m_playlist->nextIndex(steps);
}

int MusicWorker::previousIndex(int steps) const
{
    return m_playlist->previousIndex(steps);
}

/* ---- 以下在工作线程中执行 ---- */

void MusicWorker::doPlay(int index)
{
    m_player->stop();                       /* GStreamer 拆管线, 可能阻塞几百ms */
    m_playlist->setCurrentIndex(index);
    m_player->play();                       /* GStreamer 建管线, 可能阻塞几百ms */
    /* 但这些阻塞只发生在工作线程, UI 线程毫无感知 */
}

void MusicWorker::doStop()
{
    m_player->stop();
}

void MusicWorker::doPause()
{
    m_player->pause();
}

void MusicWorker::doResume()
{
    m_player->play();
}

void MusicWorker::doSetVolume(int vol)
{
    m_player->setVolume(vol);
}

void MusicWorker::doSeek(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

void MusicWorker::doSetPlaybackMode(int mode)
{
    m_playlist->setPlaybackMode(static_cast<QMediaPlaylist::PlaybackMode>(mode));
}
