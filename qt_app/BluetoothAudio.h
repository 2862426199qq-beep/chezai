#ifndef BLUETOOTHAUDIO_H
#define BLUETOOTHAUDIO_H

#include <QObject>
#include <QProcess>
#include <QLocalSocket>
#include <QString>

class BluetoothAudio : public QObject {
    Q_OBJECT
public:
    explicit BluetoothAudio(QObject *parent = nullptr);
    ~BluetoothAudio();

    void start();
    void stop();
    QString getStatus();

signals:
    void deviceConnected(QString deviceName);
    void deviceDisconnected();

private slots:
    void onSocketReadyRead();
    void onServiceError(QProcess::ProcessError err);

private:
    bool ensureServiceRunning();
    bool ensureSocketConnected(int timeoutMs = 800);
    void sendRequest(const QString &method);
    void handleJsonLine(const QByteArray &line);

private:
    QString m_socketPath;
    QString m_lastStatus;
    QString m_lastDeviceName;

    QLocalSocket *m_socket;
    QProcess *m_serviceProcess;
    QByteArray m_rxBuffer;
};

#endif
