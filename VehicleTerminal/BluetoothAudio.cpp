#include "BluetoothAudio.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>

BluetoothAudio::BluetoothAudio(QObject *parent)
    : QObject(parent)
    , m_socketPath("/tmp/bt_audio_service.sock")
    , m_lastStatus("未连接")
    , m_socket(new QLocalSocket(this))
    , m_serviceProcess(new QProcess(this)) {
    connect(m_socket, &QLocalSocket::readyRead, this, &BluetoothAudio::onSocketReadyRead);
    connect(m_serviceProcess, &QProcess::errorOccurred, this, &BluetoothAudio::onServiceError);
}

BluetoothAudio::~BluetoothAudio() {
    stop();
    if (m_serviceProcess->state() != QProcess::NotRunning) {
        m_serviceProcess->terminate();
        m_serviceProcess->waitForFinished(500);
    }
}

bool BluetoothAudio::ensureServiceRunning() {
    QLocalSocket probe;
    probe.connectToServer(m_socketPath);
    if (probe.waitForConnected(200)) {
        probe.disconnectFromServer();
        return true;
    }

    QFileInfo script(QCoreApplication::applicationDirPath() + "/bt_audio_service.py");
    if (!script.exists()) {
        script.setFile(QCoreApplication::applicationDirPath() + "/../qt_app/bt_audio_service.py");
    }
    if (!script.exists()) {
        return false;
    }

    if (m_serviceProcess->state() == QProcess::NotRunning) {
        QStringList args;
        args << script.absoluteFilePath();
        args << "--socket" << m_socketPath;
        m_serviceProcess->start("python3", args);
        if (!m_serviceProcess->waitForStarted(1200)) {
            return false;
        }
        QThread::msleep(200);
    }
    return true;
}

bool BluetoothAudio::ensureSocketConnected(int timeoutMs) {
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        return true;
    }

    m_socket->abort();
    m_socket->connectToServer(m_socketPath);
    return m_socket->waitForConnected(timeoutMs);
}

void BluetoothAudio::sendRequest(const QString &method) {
    QJsonObject obj;
    obj.insert("method", method);

    QJsonDocument doc(obj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');

    m_socket->write(payload);
    m_socket->flush();
}

void BluetoothAudio::start() {
    if (!ensureServiceRunning()) {
        return;
    }
    if (!ensureSocketConnected()) {
        return;
    }
    sendRequest("start");
}

void BluetoothAudio::stop() {
    if (ensureSocketConnected()) {
        sendRequest("stop");
    }
}

QString BluetoothAudio::getStatus() {
    if (!ensureServiceRunning()) {
        m_lastStatus = "服务未运行";
        return m_lastStatus;
    }
    if (!ensureSocketConnected()) {
        m_lastStatus = "服务未连接";
        return m_lastStatus;
    }

    sendRequest("getStatus");

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
    timer.start(800);
    loop.exec();

    if (m_lastStatus.isEmpty()) {
        return "未连接";
    }
    return m_lastStatus;
}

void BluetoothAudio::onSocketReadyRead() {
    m_rxBuffer.append(m_socket->readAll());

    while (true) {
        int idx = m_rxBuffer.indexOf('\n');
        if (idx < 0) {
            break;
        }

        QByteArray line = m_rxBuffer.left(idx).trimmed();
        m_rxBuffer.remove(0, idx + 1);

        if (!line.isEmpty()) {
            handleJsonLine(line);
        }
    }
}

void BluetoothAudio::handleJsonLine(const QByteArray &line) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("status") && obj.value("status").isObject()) {
        QJsonObject statusObj = obj.value("status").toObject();
        QString state = statusObj.value("state").toString();
        QString name = statusObj.value("deviceName").toString();

        if (state == "connected") {
            m_lastStatus = QString("已连接: %1").arg(name);
            if (name != m_lastDeviceName) {
                m_lastDeviceName = name;
                emit deviceConnected(name);
            }
        } else {
            m_lastStatus = "未连接";
            if (!m_lastDeviceName.isEmpty()) {
                m_lastDeviceName.clear();
                emit deviceDisconnected();
            }
        }
        return;
    }

    QString eventName = obj.value("event").toString();
    if (eventName == "statusChanged") {
        QJsonObject payload = obj.value("payload").toObject();
        QString state = payload.value("state").toString();
        QString name = payload.value("deviceName").toString();

        if (state == "connected") {
            m_lastStatus = QString("已连接: %1").arg(name);
            if (name != m_lastDeviceName) {
                m_lastDeviceName = name;
                emit deviceConnected(name);
            }
        } else {
            m_lastStatus = "未连接";
            if (!m_lastDeviceName.isEmpty()) {
                m_lastDeviceName.clear();
                emit deviceDisconnected();
            }
        }
    }
}

void BluetoothAudio::onServiceError(QProcess::ProcessError) {
    m_lastStatus = "服务异常";
}
