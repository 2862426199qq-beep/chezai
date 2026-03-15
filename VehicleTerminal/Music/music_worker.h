/*
 * music_worker.h — 音乐播放工作线程
 *
 * 设计思路:
 *   QMediaPlayer 的 stop()/play() 底层调用 GStreamer 状态切换,
 *   在 RK3588S 上耗时 200~500ms, 如果在 UI 线程执行会卡界面。
 *
 *   MusicWorker 通过 moveToThread() 搬到独立工作线程,
 *   UI 线程只通过信号槽与之通信 (QueuedConnection),
 *   stop()/play() 的阻塞只影响工作线程, UI 保持流畅。
 *
 *   类比内核 DHT11 驱动:
 *     - local_irq_save 把时序采集隔离在关中断区间, 不影响其他中断
 *     - moveToThread 把播放操作隔离在工作线程, 不影响 UI 线程
 */

#ifndef MUSIC_WORKER_H
#define MUSIC_WORKER_H

#include <QObject>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QUrl>
#include <QString>

class MusicWorker : public QObject
{
    Q_OBJECT

public:
    explicit MusicWorker(QObject *parent = nullptr);
    ~MusicWorker();

    /* 以下函数在 moveToThread 之前由主线程调用, 用于初始化播放列表 */
    bool addMedia(const QString &filePath);
    bool isEmpty() const;
    int  mediaCount() const;
    int  nextIndex(int steps = 1) const;
    int  previousIndex(int steps = 1) const;

public slots:
    /* ---- 以下槽函数由工作线程事件循环执行 ---- */
    void doPlay(int index);         /* 切到第 index 首并播放 */
    void doStop();
    void doPause();
    void doResume();
    void doSetVolume(int vol);
    void doSeek(qint64 positionMs);
    void doSetPlaybackMode(int mode);   /* QMediaPlaylist::PlaybackMode */

signals:
    /* ---- 以下信号发回 UI 线程 ---- */
    void durationChanged(qint64 durationMs);
    void positionChanged(qint64 positionMs);
    void currentIndexChanged(int index);

private:
    QMediaPlayer   *m_player;
    QMediaPlaylist *m_playlist;
};

#endif // MUSIC_WORKER_H
