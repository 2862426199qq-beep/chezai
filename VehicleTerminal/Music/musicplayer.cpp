/*
 * musicplayer.cpp — 音乐播放器 UI 层
 *
 * 架构: UI (本文件, 主线程) ←信号槽→ MusicWorker (工作线程)
 *
 * UI 线程只做:
 *   - 按钮响应 → emit sigPlay/sigStop/... → 投递到工作线程
 *   - 接收 Worker 的 durationChanged/positionChanged → 刷新进度条
 *
 * 工作线程做:
 *   - QMediaPlayer 的 stop()/play() (GStreamer 管线操作, 可能阻塞几百ms)
 *
 * 这样 UI 线程永远不会被 GStreamer 阻塞, 切歌时界面保持流畅。
 */

#include "Music/musicplayer.h"
#include "ui_musicplayer.h"

#include <QMouseEvent>
#include <stdio.h>

MusicPlayer::MusicPlayer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MusicPlayer),
    m_worker(nullptr),
    m_thread(nullptr),
    m_currentVolume(50),
    m_isPaused(false)
{
    printf("MusicPlayer: constructor starting...\n");
    fflush(stdout);
    ui->setupUi(this);
    this->setMinimumSize(1024,600);
    searchMusicWin = new SearchMusic(this);

    /*
     * ========== 创建工作线程 ==========
     *
     * 1. new MusicWorker() — 此时 worker 还在主线程, 可以安全调用 addMedia()
     * 2. new QThread() — 创建线程对象 (还未启动)
     * 3. moveToThread() — 把 worker 的事件循环绑定到工作线程
     * 4. m_thread->start() — 启动工作线程的事件循环
     *
     * moveToThread 之后, 所有通过 QueuedConnection 连接的信号
     * 都会在工作线程的事件循环中执行, 而非主线程。
     */
    m_worker = new MusicWorker();       /* 注意: 不设 parent, 因为要 moveToThread */
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    /* 线程结束时自动清理 worker */
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    /* UI → Worker 信号 (跨线程, QueuedConnection, 非阻塞) */
    connect(this, &MusicPlayer::sigPlay,            m_worker, &MusicWorker::doPlay);
    connect(this, &MusicPlayer::sigStop,            m_worker, &MusicWorker::doStop);
    connect(this, &MusicPlayer::sigPause,           m_worker, &MusicWorker::doPause);
    connect(this, &MusicPlayer::sigResume,          m_worker, &MusicWorker::doResume);
    connect(this, &MusicPlayer::sigSetVolume,       m_worker, &MusicWorker::doSetVolume);
    connect(this, &MusicPlayer::sigSeek,            m_worker, &MusicWorker::doSeek);
    connect(this, &MusicPlayer::sigSetPlaybackMode, m_worker, &MusicWorker::doSetPlaybackMode);

    /* Worker → UI 信号 (跨线程, 自动回到主线程执行, 安全刷新 UI) */
    connect(m_worker, &MusicWorker::durationChanged,     this, &MusicPlayer::onWorkerDurationChanged);
    connect(m_worker, &MusicWorker::positionChanged,     this, &MusicPlayer::onWorkerPositionChanged);
    connect(m_worker, &MusicWorker::currentIndexChanged,  this, &MusicPlayer::onWorkerIndexChanged);

    m_thread->start();                  /* 启动工作线程事件循环 */

    /* 扫描本地歌曲 (addMedia 在 moveToThread 之后也安全, 因为此时工作线程还没处理信号) */
    ScanLocalSongs();

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    Second_request.setSslConfiguration(config);
    QFile style_file(":/music/Music/style.qss");
    if(style_file.exists())
    {
        style_file.open(QFile::ReadOnly);
        QString styleStr = QLatin1String(style_file.readAll());
        this->setStyleSheet(styleStr);
    }

    connect(searchMusicWin, &SearchMusic::AddUrlMusic, this, &MusicPlayer::AddMusicFromUrl);
    connect(&First_netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(on_GetSongTrueUrl(QNetworkReply*)));
    connect(&Second_netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(on_DownSong(QNetworkReply*)));
}

MusicPlayer::~MusicPlayer()
{
    /* 安全关闭工作线程: 退出事件循环 → 等待线程结束 → deleteLater 清理 worker */
    m_thread->quit();
    m_thread->wait();
    delete ui;
}

/*  扫描本地音乐 — 直接调用 worker 的 addMedia (此时线程刚启动, 不冲突)  */
void MusicPlayer::ScanLocalSongs()
{
    QString searchPath = QCoreApplication::applicationDirPath() + LocalSongsPath;
    printf("MusicPlayer: Scanning songs in: %s\n", searchPath.toStdString().c_str());
    fflush(stdout);

    QDir dir(searchPath);
    QDir dirAbsolutePath(dir.absolutePath());

    printf("MusicPlayer: Absolute path: %s\n", dirAbsolutePath.absolutePath().toStdString().c_str());
    printf("MusicPlayer: Directory exists: %s\n", dirAbsolutePath.exists() ? "yes" : "no");
    fflush(stdout);

    if(dirAbsolutePath.exists())
    {
        QStringList filter;
        filter << "*.mp3";
        QFileInfoList files = dirAbsolutePath.entryInfoList(filter, QDir::Files);

        printf("MusicPlayer: Found %d mp3 files\n", files.count());
        fflush(stdout);

        for(int i = 0; i < files.count(); i++)
        {
            MediaObjectInfo info;
            QString fileName = QString::fromUtf8(files.at(i).fileName().replace(".mp3","").toUtf8().data());
            info.fileName = fileName;
            info.filePath = QString::fromUtf8(files.at(i).filePath().toUtf8().data());

            printf("MusicPlayer: Adding song: %s\n", fileName.toStdString().c_str());
            fflush(stdout);

            if(m_worker->addMedia(info.filePath))
            {
                SongInfoVector.append(info);
                ui->listWidget->addItem(info.fileName);
            }
            else
            {
                qDebug() << "addMedia failed for:" << info.filePath;
            }
        }
    }
    else
    {
        printf("MusicPlayer: Music directory not found!\n");
        fflush(stdout);
    }
}

/*  打开在线搜索音乐界面  */
void MusicPlayer::on_pBtn_OpenSearchWin_clicked()
{
    searchMusicWin->show();
}

/*  根据音乐url地址下载音乐 添加到本地  */
void MusicPlayer::AddMusicFromUrl(QString name, QString UrlPath)
{
    qDebug() << UrlPath;
    First_request.setUrl(QUrl(UrlPath));
    First_netManager.get(First_request);
    CurrentSaveSongFileName = name;
}

/*
 * 用户点击列表中的歌曲 — 发信号让 worker 切歌
 * emit sigPlay() 是非阻塞的, UI 线程立即返回
 */
void MusicPlayer::on_listWidget_currentRowChanged(int currentRow)
{
    emit sigPlay(currentRow);
}

/*
 * 点击上一首 — 算出索引, 更新列表高亮, 发信号
 * blockSignals 防止 setCurrentRow 触发 currentRowChanged 导致重复 sigPlay
 */
void MusicPlayer::on_pBtn_Pre_clicked()
{
    int prevIdx = m_worker->previousIndex(1);
    ui->listWidget->blockSignals(true);
    ui->listWidget->setCurrentRow(prevIdx);
    ui->listWidget->blockSignals(false);
    emit sigPlay(prevIdx);              /* 非阻塞, 立即返回 */
}

/* 点击下一首 — 同上 */
void MusicPlayer::on_pBtn_Next_clicked()
{
    int nextIdx = m_worker->nextIndex(1);
    ui->listWidget->blockSignals(true);
    ui->listWidget->setCurrentRow(nextIdx);
    ui->listWidget->blockSignals(false);
    emit sigPlay(nextIdx);              /* 非阻塞, 立即返回 */
}

/* 暂停/恢复 */
void MusicPlayer::on_pBtn_Pause_clicked()
{
    if(ui->pBtn_Pause->isChecked())
    {
        m_isPaused = true;
        emit sigPause();
    }
    else
    {
        m_isPaused = false;
        emit sigResume();
    }
}

/*  增加音量 — 本地记录音量值, 发信号给 worker  */
void MusicPlayer::on_pBtn_Loud_clicked()
{
    if(m_currentVolume > 99)
        return;
    m_currentVolume += 10;
    emit sigSetVolume(m_currentVolume);
    qDebug() << "Curr Volume" << m_currentVolume;
}

/* 降低音量 */
void MusicPlayer::on_pBtn_Low_clicked()
{
    if(m_currentVolume < 1)
        return;
    m_currentVolume -= 10;
    emit sigSetVolume(m_currentVolume);
    qDebug() << "Curr Volume" << m_currentVolume;
}

/* 拖动进度条 */
void MusicPlayer::on_horizontalSlider_sliderReleased()
{
    emit sigSeek(ui->horizontalSlider->value() * 1000);
}

/*
 * ========== Worker → UI 回调 (在主线程执行, 安全刷新 UI) ==========
 */

/* 切歌时更新总时长 */
void MusicPlayer::onWorkerDurationChanged(qint64 duration)
{
    ui->horizontalSlider->setMaximum(duration / 1000);
    int min = duration / 1000 / 60;
    int sec = (duration / 1000) % 60;
    QString minStr = QString::number(min);
    QString secStr = QString::number(sec);
    if(min < 10) minStr = "0" + minStr;
    if(sec < 10) secStr = "0" + secStr;
    ui->label_TotalTime->setText(QString("%1:%2").arg(minStr, secStr));
}

/* 播放进度更新 */
void MusicPlayer::onWorkerPositionChanged(qint64 value)
{
    ui->horizontalSlider->setValue(value / 1000);
    int min = value / 1000 / 60;
    int sec = (value / 1000) % 60;
    QString minStr = QString::number(min);
    QString secStr = QString::number(sec);
    if(min < 10) minStr = "0" + minStr;
    if(sec < 10) secStr = "0" + secStr;
    ui->label_CurTime->setText(QString("%1:%2").arg(minStr, secStr));
}

/* 播放列表自动切歌时, 更新列表高亮 (如播完一首自动跳下一首) */
void MusicPlayer::onWorkerIndexChanged(int index)
{
    if(index >= 0 && index < ui->listWidget->count())
    {
        ui->listWidget->blockSignals(true);
        ui->listWidget->setCurrentRow(index);
        ui->listWidget->blockSignals(false);
    }
}

/* 点击循环播放按钮 */
void MusicPlayer::on_pBtn_loop_clicked()
{
    if(ui->pBtn_SetLove->isChecked())
        emit sigSetPlaybackMode(QMediaPlaylist::CurrentItemInLoop);
    else
        emit sigSetPlaybackMode(QMediaPlaylist::Random);
}

/* 根据音乐搜索网络请求，获取音乐的真实下载地址 */
void MusicPlayer::on_GetSongTrueUrl(QNetworkReply *reply)
{
    qDebug() << "on_GetSongTrueUrl";
    qDebug() << reply->rawHeaderList();
    qDebug() << reply->rawHeaderPairs();
    qDebug() << reply->readAll();
    int total = reply->rawHeaderPairs().length();
    qDebug() << total;
    for(int i = 0; i < total; i++)
    {
        QString first = QString(reply->rawHeaderPairs().at(i).first);
        if(first.compare(QString("Location")) == 0)
        {
            QString urlDownload = QString(reply->rawHeaderPairs().at(i).second);
            if(urlDownload.endsWith("404"))
            {
                QMessageBox::warning(this, "warning", "该歌曲无法下载，请换一首");
                return;
            }
            qDebug() << urlDownload;
            Second_request.setUrl(QUrl(urlDownload));
            Second_netManager.get(Second_request);
        }
    }
}

/* 下载音乐到本地文件 */
void MusicPlayer::on_DownSong(QNetworkReply *reply)
{
    QFile file;
    QString FilePath = QCoreApplication::applicationDirPath() + LocalSongsPath
                       + QString("/") + CurrentSaveSongFileName + QString(".mp3");
    file.setFileName(FilePath);
    file.open(QIODevice::Append | QIODevice::Truncate);
    file.write(reply->readAll());
    file.close();
    qDebug() << "Save Song <" << CurrentSaveSongFileName << "> Finished";
    MediaObjectInfo info;
    info.fileName = CurrentSaveSongFileName;
    info.filePath = FilePath;
    if(m_worker->addMedia(info.filePath))
    {
        SongInfoVector.append(info);
        ui->listWidget->addItem(info.fileName);
    }
    else
    {
        qDebug() << "addMedia failed for downloaded song";
    }
}

/* 退出按钮  */
void MusicPlayer::on_pushButton_clicked()
{
    this->hide();
}

/* 处理语音控制传来的指令 */
void MusicPlayer::on_handleCommand(int command)
{
    switch (command) {
    case MUSIC_COMMAND_SHOW:
        this->show();
        break;
    case MUSIC_COMMAND_CLOSE:
        this->hide();
        break;
    case MUSIC_COMMAND_PAUSE:
        emit sigPause();
        break;
    case MUSIC_COMMAND_PLAY: {
        if(m_worker->isEmpty()) return;
        int idx = m_worker->previousIndex(1);
        ui->listWidget->blockSignals(true);
        ui->listWidget->setCurrentRow(idx);
        ui->listWidget->blockSignals(false);
        emit sigPlay(idx);
        break;
    }
    case MUSIC_COMMAND_PRE: {
        if(m_worker->isEmpty()) return;
        int prevIdx = m_worker->previousIndex(1);
        ui->listWidget->blockSignals(true);
        ui->listWidget->setCurrentRow(prevIdx);
        ui->listWidget->blockSignals(false);
        emit sigPlay(prevIdx);
        break;
    }
    case MUSIC_COMMAND_NEXT: {
        if(m_worker->isEmpty()) return;
        int nextIdx = m_worker->nextIndex(1);
        ui->listWidget->blockSignals(true);
        ui->listWidget->setCurrentRow(nextIdx);
        ui->listWidget->blockSignals(false);
        emit sigPlay(nextIdx);
        break;
    }
    default:
        break;
    }
}
