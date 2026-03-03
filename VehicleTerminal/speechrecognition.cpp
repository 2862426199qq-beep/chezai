#include "speechrecognition.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <linux/ioctl.h>

SpeechRecognition::SpeechRecognition()
{
    printf("SpeechRecognition: constructor starting...\n");
    fflush(stdout);
    
    timer = new QTimer();
    initAudioRecord();
    initKeyRead();
    connect(timer,SIGNAL(timeout()),this,SLOT(timer_timerout()));
    
    printf("SpeechRecognition: constructor complete\n");
    fflush(stdout);
}

void SpeechRecognition::initAudioRecord()
{
    m_audioRecorder = new QAudioRecorder(this);
    /* 扫描本地声卡设备 */
    devicesVar.append(QVariant(QString()));
    for (auto &device: m_audioRecorder->audioInputs()) {
        devicesVar.append(QVariant(device));
        //qDebug()<<"本地声卡设备："<<device<<endl;
    }

    /* 音频编码 */
    codecsVar.append(QVariant(QString()));
    for (auto &codecName: m_audioRecorder->supportedAudioCodecs()) {
        codecsVar.append(QVariant(codecName));
        //qDebug()<<"音频编码："<<codecName<<endl;
    }

    /* 容器/支持的格式 */
    containersVar.append(QVariant(QString()));
    for (auto &containerName: m_audioRecorder->supportedContainers()) {
        containersVar.append(QVariant(containerName));
        //qDebug()<<"支持的格式："<<containerName<<endl;
    }

    /* 采样率 */
    sampleRateVar.append(QVariant(0));
    sampleRateVar.append(QVariant(8000));
    sampleRateVar.append(QVariant(16000));
    for (int sampleRate: m_audioRecorder->supportedAudioSampleRates()) {
        sampleRateVar.append(QVariant(sampleRate));
        //qDebug()<<"采样率："<<sampleRate<<endl;
    }

    /* 通道 */
    channelsVar.append(QVariant(-1));
    channelsVar.append(QVariant(1));
    channelsVar.append(QVariant(2));
    channelsVar.append(QVariant(4));

    /* 质量 */
    qualityVar.append(QVariant(int(QMultimedia::LowQuality)));
    qualityVar.append(QVariant(int(QMultimedia::NormalQuality)));
    qualityVar.append(QVariant(int(QMultimedia::HighQuality)));

    /* 比特率 */
    bitratesVar.append(QVariant(0));
    bitratesVar.append(QVariant(32000));
    bitratesVar.append(QVariant(64000));
    bitratesVar.append(QVariant(96000));
    bitratesVar.append(QVariant(128000));
}

void SpeechRecognition::initKeyRead()
{
    key_fd = open("/dev/key",O_RDWR);
    if(key_fd<0)
    {
        qDebug("file /dev/key open failed, voice control disabled");
        key_fd = -1;  // 确保为-1
    }
}

/* 开启录音  */
void SpeechRecognition::startRecord()
{
    qDebug()<<"start record";
    
    // 安全检查：确保有可用的输入设备和格式
    if(devicesVar.isEmpty() || codecsVar.isEmpty() || containersVar.isEmpty()) {
        qDebug() << "音频录制设备或格式不可用";
        return;
    }
    
    m_audioRecorder->setAudioInput(devicesVar.at(0).toString());
    QAudioEncoderSettings settings;
    settings.setCodec(codecsVar.size() > 0 ? codecsVar.at(0).toString() : "");
    settings.setSampleRate(sampleRateVar.size() > 2 ? sampleRateVar[2].toInt() : 16000);
    settings.setBitRate(bitratesVar.size() > 0 ? bitratesVar[0].toInt() : 0);
    settings.setChannelCount(channelsVar.size() > 1 ? channelsVar[1].toInt() : 1);
    settings.setQuality(QMultimedia::EncodingQuality(
        qualityVar.size() > 0 ? qualityVar[0].toInt() : 0));
    /* 以恒定的质量录制，可选恒定的比特率 */
    settings.setEncodingMode(QMultimedia::ConstantQualityEncoding);
    // 使用第一个可用的容器格式，而不是固定索引
    QString container = containersVar.size() > 1 ? containersVar.at(1).toString() : "";
    m_audioRecorder->setEncodingSettings(settings,QVideoEncoderSettings(),container);
    m_audioRecorder->setOutputLocation(QUrl::fromLocalFile(tr("./record.wav")));
    m_audioRecorder->record();
}


/*  录音停止 并发送信号，调用对应槽函数处理  */
void SpeechRecognition::stopRecord()
{
    m_audioRecorder->stop();
    emit RecordFinished();
}

/* 获取按键值，如果是按下 按键值为240，否则为0  */
int SpeechRecognition::readKeyValue()
{
    if(key_fd < 0) {
        return 0;  // 设备未打开，返回0表示未按下
    }
    char keyValue = 0;
    ssize_t ret = read(key_fd,&keyValue,sizeof(keyValue));
    if(ret <= 0) {
        return 0;
    }
    return keyValue;
}


/* 定时器槽函数，用来实时检测按键是否按下松开，按下时录音，松开后结束录音并进行识别并控制其他功能 */
void SpeechRecognition::timer_timerout()
{
    char keyValue = readKeyValue();
    if(keyValue==240)
    {
       if(hasRecord==0) startRecord();
        hasRecord =1;
    }
    else
    {
        if(hasRecord==1)stopRecord();
        hasRecord = 0;
    }
}


/*得到网络请求的槽函数，对得到的网络请求结果进行处理*/
void SpeechRecognition::getSpeechResult(QNetworkReply *reply)
{
    reply->waitForReadyRead(500);
    QByteArray content = reply->readAll();
    content.remove(content.lastIndexOf('\n'),1);
    QJsonDocument doc = QJsonDocument::fromJson(content);
    if(!doc.isObject()){
        qDebug()<<"Netjson not an jsonObject!";
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray resutls = obj.value("result").toArray();
    QString AsrResult = resutls.at(0).toString();
    qDebug()<<"识别结果:"<<AsrResult;
}


/* 开启语音控制功能 ，也就是开启定时器进行实时按键检测  */
void SpeechRecognition::startSpeechRecognition()
{
    timer->start(500);
}
