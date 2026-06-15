#include "TraceFormatter.h"

#include <GXBytebuffer.h>
#include <GXDLMSTranslator.h>
#include <enums.h>
#include <errorcodes.h>

#include <QDateTime>

namespace {

QString timestampPrefix()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")) + QStringLiteral("\n");
}

QString bytesToHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QString translateBytes(const QByteArray &data, bool pduOnly)
{
    if (data.isEmpty())
        return {};

    CGXByteBuffer buffer;
    buffer.Set(reinterpret_cast<const unsigned char *>(data.constData()),
               static_cast<unsigned long>(data.size()));

    CGXDLMSTranslator translator(DLMS_TRANSLATOR_OUTPUT_TYPE_SIMPLE_XML);
    translator.SetPduOnly(pduOnly);

    std::string xml;
    int ret = translator.PduToXml(buffer, xml);
    if (ret != DLMS_ERROR_CODE_OK)
        ret = translator.DataToXml(buffer, xml);

    if (ret != DLMS_ERROR_CODE_OK || xml.empty())
        return bytesToHex(data);
    return QString::fromStdString(xml);
}

} // namespace

namespace TraceFormatter {

QString formatPacket(const QString &direction, const QByteArray &data, TraceDisplayMode mode, bool withTimestamp)
{
    if (mode == TraceDisplayMode::None || data.isEmpty())
        return {};

    QString body;
    switch (mode) {
    case TraceDisplayMode::Hex:
        body = direction + QStringLiteral(": ") + bytesToHex(data);
        break;
    case TraceDisplayMode::Xml:
        body = direction + QStringLiteral(":\n") + translateBytes(data, false);
        break;
    case TraceDisplayMode::Pdu:
        body = direction + QStringLiteral(":\n") + translateBytes(data, true);
        break;
    default:
        return {};
    }

    if (withTimestamp)
        return timestampPrefix() + body;
    return body;
}

QString formatNotification(const QByteArray &data, NotificationDisplayMode mode, bool withTimestamp)
{
    if (data.isEmpty())
        return {};

    QString body;
    switch (mode) {
    case NotificationDisplayMode::Hex:
        body = bytesToHex(data);
        break;
    case NotificationDisplayMode::Xml:
        body = translateBytes(data, false);
        break;
    case NotificationDisplayMode::Pdu:
        body = translateBytes(data, true);
        break;
    }

    if (withTimestamp)
        return timestampPrefix() + body;
    return body;
}

} // namespace
