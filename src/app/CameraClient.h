#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QProcess>
#include <QTimer>
#include "PropParser.h"
#include "MpvItem.h"


class CameraClient : public QObject
{
    Q_OBJECT

public:
    explicit CameraClient(QObject *parent = nullptr);
    Q_INVOKABLE void start(MpvItem* player);

signals:
    void videoSinkChanged();

private:
    void sendTcpPacket(const QByteArray &packet, const QString &name);
    QByteArray buildHeartbeat(quint8 counter);

    QTcpSocket tcp;
    QUdpSocket udp;
    QProcess mpv;

    QTimer heartbeatTimer;
    quint8 hbCounter = 0xC2;

    PropParser parser;

    QString camIp = "192.179.8.1";
    int camPort = 6320;
    int udpLocalPort = 14766;
    int udpTargetPort = 8989;

    int pipeFd[2];
};
