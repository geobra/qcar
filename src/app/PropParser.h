#pragma once
#include <QByteArray>
#include <QVector>

class PropParser
{
public:
    QVector<QByteArray> extractPackets(const QByteArray &data);

private:
    QByteArray buffer;

    static constexpr char MAGIC[2] = {char(0x90), char(0x60)};
    static constexpr int HEADER_SIZE = 22;
};
