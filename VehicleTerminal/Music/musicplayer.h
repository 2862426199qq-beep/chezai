#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <Music/searchmusic.h>
#include <Music/music_worker.h>
#include <QMainWindow>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QVector>
#include <QNetworkReply>
#include <QMessageBox>
#include <QLocale>

#define MUSIC_COMMAND_CLOSE 0
#define MUSIC_COMMAND_SHOW 1
#define MUSIC_COMMAND_PRE 2
#define MUSIC_COMMAND_NEXT 3
#define MUSIC_COMMAND_PLAY 4
#define MUSIC_COMMAND_PAUSE 5

namespace Ui {
class MusicPlayer;
}

/* 媒体信息结构体 */
struct MediaObjectInfo {
    QString fileName;
    QString filePath;
};

class MusicPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer();
    QString LocalSongsPath = "/Music/myMusic";
    void ScanLocalSongs();

signals:
    /* UI → Worker 的跨线程信号 (QueuedConnection, 非阻塞) */
    void sigPlay(int index);
    void sigStop();
    void sigPause();
    void sigResume();
    void sigSetVolume(int vol);
    void sigSeek(qint64 positionMs);
    void sigSetPlaybackMode(int mode);

private slots:
    void on_pBtn_OpenSearchWin_clicked();
    void AddMusicFromUrl(QString name, QString UrlPath);
    void on_listWidget_currentRowChanged(int currentRow);
    void on_pBtn_Pre_clicked();
    void on_pBtn_Next_clicked();
    void on_pBtn_Pause_clicked();
    void on_pBtn_Loud_clicked();
    void on_pBtn_Low_clicked();
    void on_horizontalSlider_sliderReleased();
    void on_pBtn_loop_clicked();
    void on_GetSongTrueUrl(QNetworkReply *reply);
    void on_DownSong(QNetworkReply *reply);
    void on_pushButton_clicked();

    /* Worker → UI 的回调 (自动切回主线程) */
    void onWorkerDurationChanged(qint64 durationMs);
    void onWorkerPositionChanged(qint64 positionMs);
    void onWorkerIndexChanged(int index);

public slots:
    void on_handleCommand(int);

private:
    class SearchMusic *searchMusicWin;
    Ui::MusicPlayer *ui;

    /* 播放后端: worker 住在 m_thread 里 */
    MusicWorker *m_worker;
    QThread     *m_thread;

    int m_currentVolume;
    bool m_isPaused;

    QString CurrentSaveSongFileName;
    QVector<MediaObjectInfo> SongInfoVector;

    QNetworkAccessManager First_netManager;
    QNetworkRequest First_request;
    QNetworkAccessManager Second_netManager;
    QNetworkRequest Second_request;
};

#endif // MUSICPLAYER_H
