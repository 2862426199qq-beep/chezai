#ifndef MJPEG_SERVER_H
#define MJPEG_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QImage>
#include <QList>
#include <QMutex>

class MjpegServer : public QObject
{
    Q_OBJECT

public:
    explicit MjpegServer(quint16 port = 8080, QObject *parent = nullptr);
    ~MjpegServer();

    bool isListening() const;
    quint16 serverPort() const;

public slots:
    void onNewFrame(const QImage &img);

private slots:
    void onNewConnection();
    void onClientDisconnected();

private:
    QTcpServer *m_server;
    QList<QTcpSocket*> m_clients;
    QMutex m_mutex;
    int m_jpegQuality;
};

#endif
