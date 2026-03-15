#include "mjpeg_server.h"
#include <QBuffer>
#include <QDebug>

// MJPEG over HTTP 的标准边界字符串
static const char *BOUNDARY = "mjpegframe";

// 新客户端连接后发送的 HTTP 响应头（长连接，multipart 流）
static const QByteArray HTTP_HEADER =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace;boundary=" + QByteArray(BOUNDARY) + "\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

MjpegServer::MjpegServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_jpegQuality(70)   // JPEG 压缩质量，720p 下约 30-50KB/帧
{
    connect(m_server, &QTcpServer::newConnection,
            this, &MjpegServer::onNewConnection);

    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "MjpegServer: listening on port" << port;
    } else {
        qWarning() << "MjpegServer: failed to listen on port" << port
                    << m_server->errorString();
    }
}

MjpegServer::~MjpegServer()
{
    QMutexLocker lock(&m_mutex);
    for (QTcpSocket *client : m_clients) {
        client->disconnectFromHost();
    }
    m_clients.clear();
    m_server->close();
}

bool MjpegServer::isListening() const
{
    return m_server->isListening();
}

quint16 MjpegServer::serverPort() const
{
    return m_server->serverPort();
}

void MjpegServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *client = m_server->nextPendingConnection();
        if (!client) continue;

        // 读取并丢弃客户端发来的 HTTP GET 请求头（浏览器访问时会发 GET /）
        // 我们不解析具体路径，任何连接都推 MJPEG 流
        connect(client, &QTcpSocket::readyRead, client, [client]() {
            client->readAll();  // 丢弃请求内容
        });

        connect(client, &QTcpSocket::disconnected,
                this, &MjpegServer::onClientDisconnected);

        // 发送 HTTP 响应头，建立 multipart 长连接
        client->write(HTTP_HEADER);
        client->flush();

        QMutexLocker lock(&m_mutex);
        m_clients.append(client);

        qDebug() << "MjpegServer: client connected from"
                 << client->peerAddress().toString()
                 << "total:" << m_clients.size();
    }
}

void MjpegServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    QMutexLocker lock(&m_mutex);
    m_clients.removeAll(client);
    qDebug() << "MjpegServer: client disconnected, remaining:" << m_clients.size();

    client->deleteLater();
}

void MjpegServer::onNewFrame(const QImage &img)
{
    QMutexLocker lock(&m_mutex);
    if (m_clients.isEmpty()) return;   // 没客户端就不编码，省 CPU

    // JPEG 编码（720p quality=70 约 30-50KB，耗时 ~5-10ms）
    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", m_jpegQuality);
    buf.close();

    // 构造 multipart 帧头
    QByteArray header;
    header.append("--");
    header.append(BOUNDARY);
    header.append("\r\nContent-Type: image/jpeg\r\n");
    header.append("Content-Length: ");
    header.append(QByteArray::number(jpeg.size()));
    header.append("\r\n\r\n");

    // 推送给所有客户端
    QMutableListIterator<QTcpSocket*> it(m_clients);
    while (it.hasNext()) {
        QTcpSocket *client = it.next();
        // 如果客户端写缓冲区积压超过 1MB，说明网速跟不上，断开
        if (client->bytesToWrite() > 1024 * 1024) {
            qDebug() << "MjpegServer: dropping slow client"
                     << client->peerAddress().toString();
            client->disconnectFromHost();
            it.remove();
            client->deleteLater();
            continue;
        }
        client->write(header);
        client->write(jpeg);
        client->write("\r\n");
    }
}
