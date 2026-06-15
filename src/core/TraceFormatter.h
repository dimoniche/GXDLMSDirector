#pragma once

#include <QByteArray>
#include <QString>

enum class TraceDisplayMode {
    None = 0,
    Hex = 1,
    Xml = 2,
    Pdu = 3
};

enum class NotificationDisplayMode {
    Hex = 0,
    Xml = 1,
    Pdu = 2
};

namespace TraceFormatter {
QString formatPacket(const QString &direction, const QByteArray &data, TraceDisplayMode mode, bool withTimestamp);
QString formatNotification(const QByteArray &data, NotificationDisplayMode mode, bool withTimestamp);
}
