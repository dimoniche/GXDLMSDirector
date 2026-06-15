#include "PlcDiscoverer.h"

#include <GXDLMSPlcMeterInfo.h>
#include <GXDLMSSecureClient.h>
#include <GXReplyData.h>
#include <enums.h>
#include <errorcodes.h>

#include <QDateTime>
#include <QObject>

namespace {

QString systemTitleToHex(CGXByteBuffer &title)
{
    return QByteArray(reinterpret_cast<const char *>(title.GetData()),
                      static_cast<int>(title.GetSize()))
        .toHex()
        .toUpper();
}

} // namespace

QList<PlcMeterInfo> PlcDiscoverer::discover(const PlcDiscoverSettings &settings,
                                            ProgressCallback progress, bool *cancelled)
{
    QList<PlcMeterInfo> meters;
    MediaConnection media;

    int ret = 0;
    if (settings.mediaType == MediaType::Serial) {
        ret = media.openSerial(settings.serialPort, settings.baudRate, settings.dataBits,
                               settings.parity, settings.stopBits);
    } else {
        ret = media.openNetwork(settings.hostName, settings.port);
    }
    if (ret != 0)
        return meters;

    media.setWaitTimeMs(settings.waitTimeMs);
    const auto interfaceType = static_cast<DLMS_INTERFACE_TYPE>(settings.interfaceType);
    CGXDLMSSecureClient client(true, 16, 1, DLMS_AUTHENTICATION_NONE, nullptr, interfaceType);

    CGXByteBuffer discoverData;
    if (client.GetPlcSettings().DiscoverRequest(discoverData) != 0) {
        media.close();
        return meters;
    }

    if (progress)
        progress(QObject::tr("Sending PLC discover request..."));
    media.sendData(discoverData.GetData(), static_cast<int>(discoverData.GetSize()));

    const QDateTime endTime = QDateTime::currentDateTime().addMSecs(settings.durationMs);
    CGXByteBuffer receiveBuffer;
    CGXReplyData reply;

    while (QDateTime::currentDateTime() < endTime) {
        if (cancelled && *cancelled)
            break;

        QByteArray chunk;
        const int readRet = media.readData(chunk, 0x00);
        if (readRet != 0)
            continue;

        receiveBuffer.Set(chunk.constData(), static_cast<unsigned long>(chunk.size()));
        CGXReplyData notify;
        if (client.GetData(receiveBuffer, reply, notify) != 0)
            continue;

        std::vector<CGXDLMSPlcMeterInfo> list;
        if (client.GetPlcSettings().ParseDiscover(reply.GetData(),
                                                  static_cast<uint16_t>(reply.GetClientAddress()),
                                                  static_cast<uint16_t>(reply.GetServerAddress()),
                                                  list) != 0) {
            continue;
        }

        for (auto &info : list) {
            PlcMeterInfo meter;
            meter.sourceAddress = info.GetSourceAddress();
            meter.destinationAddress = info.GetDestinationAddress();
            meter.systemTitleHex = systemTitleToHex(info.GetSystemTitle());
            meter.alarmDescriptor = info.GetAlarmDescriptor();
            meter.status = meter.sourceAddress == 1 ? QObject::tr("New meter")
                                                    : QObject::tr("Existing meter");
            meters.append(meter);
            if (progress) {
                progress(QObject::tr("Found meter: %1").arg(meter.systemTitleHex));
            }
        }
        reply.Clear();
        receiveBuffer.Clear();
    }

    media.close();
    return meters;
}
