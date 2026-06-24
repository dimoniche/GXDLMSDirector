#include "HdlcAddressScanner.h"
#include "HdlcAddressHelper.h"

#include <GXDLMSClient.h>
#include <GXDLMSConverter.h>
#include <GXDLMSSecureClient.h>
#include <GXReplyData.h>
#include <enums.h>
#include <errorcodes.h>

#include <errorcodes.h>

#include <QObject>

namespace {

int readPacket(CGXDLMSSecureClient &client, MediaConnection &media, CGXByteBuffer &data,
               CGXReplyData &reply, int waitTimeMs)
{
    if (data.GetSize() == 0 && !reply.IsStreaming())
        return DLMS_ERROR_CODE_OK;

    media.setWaitTimeMs(waitTimeMs);

    if (media.sendData(data.GetData(), static_cast<int>(data.GetSize())) != 0)
        return DLMS_ERROR_CODE_SEND_FAILED;

    CGXByteBuffer bb;
    CGXReplyData notify;
    int ret = 0;
    do {
        if (notify.GetData().GetSize() != 0) {
            if (!notify.IsMoreData())
                notify.Clear();
            continue;
        }

        bb.Clear();
        QByteArray buffer;
        if ((ret = media.readData(buffer, 0x7E)) != 0)
            return ret;
        bb.Set(buffer.constData(), static_cast<unsigned long>(buffer.size()));
    } while ((ret = client.GetData(bb, reply, notify)) == DLMS_ERROR_CODE_FALSE);

    return ret;
}

int readDataBlock(CGXDLMSSecureClient &client, MediaConnection &media, std::vector<CGXByteBuffer> &data,
                  CGXReplyData &reply, int waitTimeMs)
{
    for (auto &packet : data) {
        reply.Clear();
        int ret = readPacket(client, media, packet, reply, waitTimeMs);
        if (ret != 0)
            return ret;

        while (reply.IsMoreData()) {
            CGXByteBuffer rr;
            if ((ret = client.ReceiverReady(reply, rr)) != 0)
                return ret;
            if ((ret = readPacket(client, media, rr, reply, waitTimeMs)) != 0)
                return ret;
        }
    }
    return DLMS_ERROR_CODE_OK;
}

} // namespace

QList<HdlcScanResult> HdlcAddressScanner::scan(const HdlcScanSettings &settings,
                                               ProgressCallback progress, bool *cancelled)
{
    QList<HdlcScanResult> results;
    MediaConnection media;

    int ret = 0;
    if (settings.mediaType == MediaType::Serial) {
        ret = media.openSerial(settings.serialPort, settings.baudRate, settings.dataBits,
                               settings.parity, settings.stopBits);
    } else {
        ret = media.openNetwork(settings.hostName, settings.port);
    }
    if (ret != 0)
        return results;

    const QList<int> serverAddresses = settings.serverAddresses.isEmpty()
                                           ? QList<int>{1, 17, 18, 19, 20, 21, 22, 23, 24, 25}
                                           : settings.serverAddresses;
    const QList<int> clientAddresses = settings.clientAddresses.isEmpty() ? QList<int>{16}
                                                                        : settings.clientAddresses;

    const int total = serverAddresses.size() * clientAddresses.size();
    int current = 0;

    for (int server : serverAddresses) {
        for (int client : clientAddresses) {
            if (cancelled && *cancelled)
                break;

            ++current;
            if (progress) {
                progress(current, total,
                         QObject::tr("Trying server %1, client %2").arg(server).arg(client));
            }

            CGXDLMSSecureClient dlmsClient(true, static_cast<unsigned char>(client),
                                           static_cast<int>(server), DLMS_AUTHENTICATION_NONE, nullptr,
                                           DLMS_INTERFACE_TYPE_HDLC);

            std::vector<CGXByteBuffer> data;
            CGXReplyData reply;
            dlmsClient.SetClientAddress(client);
            dlmsClient.SetServerAddress(server);

            if (dlmsClient.SNRMRequest(data) != 0)
                continue;

            dlmsClient.SetClientAddress(0);
            dlmsClient.SetServerAddress(0x7F);

            ret = readDataBlock(dlmsClient, media, data, reply, settings.waitTimeMs);
            if (ret != 0)
                continue;

            if (dlmsClient.ParseUAResponse(reply.GetData()) != 0)
                continue;

            HdlcScanResult scanResult;
            scanResult.clientAddress = client;
            scanResult.serverAddress = reply.GetServerAddress() != 0 ? reply.GetServerAddress() : server;
            unsigned short logical = 0;
            unsigned short physical = 0;
            HdlcAddressHelper::decodeServerAddress(static_cast<unsigned long>(scanResult.serverAddress),
                                                   logical, physical);
            scanResult.logicalAddress = logical;
            scanResult.physicalAddress = physical;

            QString details = QObject::tr("SNRM succeeded");
            scanResult.aarqSucceeded = false;

            if (settings.tryAarq) {
                dlmsClient.SetClientAddress(client);
                dlmsClient.SetServerAddress(server);
                data.clear();
                reply.Clear();
                if (dlmsClient.AARQRequest(data) == 0 && !data.empty()) {
                    ret = readDataBlock(dlmsClient, media, data, reply, settings.waitTimeMs);
                    if (ret == 0) {
                        const int aarqRet = dlmsClient.ParseAAREResponse(reply.GetData());
                        scanResult.aarqSucceeded = aarqRet == 0;
                        details += scanResult.aarqSucceeded ? QObject::tr(", AARQ succeeded")
                                                            : QObject::tr(", AARQ failed (%1)").arg(aarqRet);
                    }
                }

                data.clear();
                reply.Clear();
                if (dlmsClient.DisconnectRequest(data) == 0 && !data.empty())
                    readDataBlock(dlmsClient, media, data, reply, settings.waitTimeMs);
            }

            scanResult.details = details;
            results.append(scanResult);
        }
        if (cancelled && *cancelled)
            break;
    }

    media.close();
    return results;
}
