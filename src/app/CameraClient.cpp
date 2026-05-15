#include "CameraClient.h"
#include <QDebug>
#include <QThread>

#include <unistd.h>

static const QByteArray LOGIN_PACKET = QByteArray::fromHex(
    "7e0f10110006686f6e67626f083230313530313031100d");

static const QByteArray START_PACKET = QByteArray::fromHex(
    "7e0f110400000000310d");

static const QByteArray FORCE_IFRAME_PACKET = QByteArray::fromHex(
    "7e0f720400000000d00d");

CameraClient::CameraClient(QObject *parent) : QObject(parent)
{
    connect(&udp, &QUdpSocket::readyRead, this, [this]() {
        while (udp.hasPendingDatagrams()) {
            QByteArray d;
            d.resize(udp.pendingDatagramSize());
            udp.readDatagram(d.data(), d.size());

            auto chunks = parser.extractPackets(d);

            for (auto &c : chunks) {
                write(pipeFd[1], c.data(), c.size());
                //qDebug() << "chunk" << c.size();
            }
        }
    });

    connect(&heartbeatTimer, &QTimer::timeout, this, [this]() {
        auto pkt = buildHeartbeat(hbCounter++);
        tcp.write(pkt);
        qDebug() << "[HB]" << hbCounter;
    });
}

void CameraClient::start(MpvItem* player)
{
    tcp.connectToHost(camIp, camPort);
    tcp.waitForConnected();

    sendTcpPacket(LOGIN_PACKET, "LOGIN");
    QThread::msleep(200);

    sendTcpPacket(FORCE_IFRAME_PACKET, "IFRAME");
    QThread::msleep(200);

    udp.bind(QHostAddress::Any, udpLocalPort);
    udp.writeDatagram(QByteArray("\x01", 1),
                      QHostAddress(camIp), udpTargetPort);

    qDebug() << "[UDP] Trigger sent";

    QThread::msleep(200);


    tcp.write(buildHeartbeat(0xC1));

    sendTcpPacket(START_PACKET, "START");

    pipe(pipeFd);

    mpv_handle *mpv = player->mpv();

    // force raw h264
    mpv_set_option_string(mpv, "demuxer-lavf-format", "h264");
    mpv_set_option_string(mpv, "untimed", "yes");
    mpv_set_option_string(mpv, "no-cache", "yes");
    mpv_set_option_string(mpv, "wid", "0");         // no window
    mpv_set_option_string(mpv, "osc", "no");        // no controls
    mpv_set_option_string(mpv, "input-default-bindings", "no");
    mpv_set_option_string(mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv, "msg-level", "all=v");

    // load from pipe
    QString fdStr = QString("fd://%1").arg(pipeFd[0]);
    QByteArray cmd = fdStr.toUtf8();

    const char *args[] = {"loadfile", cmd.constData(), nullptr};
    mpv_command(mpv, args);

    // start UDP
    if (udp.state() != QAbstractSocket::UnconnectedState) {
		udp.close();
	}
    udp.bind(14766);
    udp.writeDatagram(QByteArray("\x01",1),
                      QHostAddress("192.179.8.1"), 8989);

    qDebug() << "Streaming started";

    heartbeatTimer.start(1000);
}

void CameraClient::sendTcpPacket(const QByteArray &packet, const QString &name)
{
    qDebug() << "[TCP]" << name << packet.toHex();
    tcp.write(packet);
    tcp.waitForBytesWritten();
}

QByteArray CameraClient::buildHeartbeat(quint8 counter)
{
    QByteArray payload;
    payload.append('\x00');
    payload.append("\x69\xe3\xda", 3);
    payload.append(counter);
    payload.append((0x104 - counter) & 0xFF);

    QByteArray pkt = "\x7e\x0f\x17\x05";
    pkt += payload;
    pkt += '\x0d';
    return pkt;
}

