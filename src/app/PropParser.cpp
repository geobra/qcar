#include "PropParser.h"

QVector<QByteArray> PropParser::extractPackets(const QByteArray &data)
{
    buffer += data;
    QVector<QByteArray> out;

    while (true) {
        int start = buffer.indexOf(QByteArray(MAGIC, 2));
        if (start < 0) {
            buffer = buffer.right(2);
            break;
        }

        if (buffer.size() < start + HEADER_SIZE)
            break;

        int next = buffer.indexOf(QByteArray(MAGIC, 2), start + 2);
        if (next < 0)
            break;

        QByteArray payload = buffer.mid(start + HEADER_SIZE,
                                        next - (start + HEADER_SIZE));
        out.append(payload);

        buffer = buffer.mid(next);
    }

    return out;
}
