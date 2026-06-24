#include "GXDLMSCommunicator.h"
#include "GXDLMSDevice.h"

#include "AssociationViewParser.h"
#include "VariantConverter.h"
#include "ConformanceHelper.h"

#include <GXDLMSAccessItem.h>
#include <GXDLMSAssociationLogicalName.h>
#include <GXDLMSConverter.h>
#include <GXDLMSData.h>
#include <GXDLMSProfileGeneric.h>
#include <GXDLMSTranslator.h>
#include <GXDLMSObject.h>
#include <GXDateTime.h>
#include <enums.h>
#include <errorcodes.h>

#include <QList>

#include <QDateTime>
#include <QThread>
#include <QTime>
#include <QTimer>

namespace {

void prepareApduBuffer(CGXByteBuffer &data)
{
    if (data.GetSize() < 3)
        return;

    const unsigned long saved = data.GetPosition();
    data.SetPosition(0);
    unsigned char header[3];
    if (data.GetUInt8(&header[0]) != 0 || data.GetUInt8(&header[1]) != 0
        || data.GetUInt8(&header[2]) != 0) {
        data.SetPosition(saved);
        return;
    }

    const bool replyLlc = header[0] == 0xE6 && header[1] == 0xE7 && header[2] == 0x00;
    const bool sendLlc = header[0] == 0xE6 && header[1] == 0xE6 && header[2] == 0x00;
    if (!replyLlc && !sendLlc)
        data.SetPosition(saved);
}

bool isCommunicationError(int errorCode)
{
    return errorCode == DLMS_ERROR_CODE_RECEIVE_FAILED
           || errorCode == DLMS_ERROR_CODE_SEND_FAILED
           || (errorCode & DLMS_ERROR_TYPE_COMMUNICATION_ERROR) != 0;
}

int normalizeIoError(int errorCode)
{
    if (errorCode == 0)
        return DLMS_ERROR_CODE_OK;
    if (isCommunicationError(errorCode))
        return DLMS_ERROR_CODE_RECEIVE_FAILED;
    return errorCode;
}

class NotificationPauseGuard
{
public:
    explicit NotificationPauseGuard(GXDLMSCommunicator *communicator)
        : m_communicator(communicator)
        , m_resume(communicator && communicator->notificationTimerActive())
    {
        if (m_resume)
            communicator->setNotificationPolling(false);
    }

    ~NotificationPauseGuard()
    {
        if (m_resume && m_communicator && m_communicator->deviceNotificationsEnabled())
            m_communicator->setNotificationPolling(true);
    }

private:
    GXDLMSCommunicator *m_communicator;
    bool m_resume = false;
};

class MeterOperationGuard
{
public:
    explicit MeterOperationGuard(GXDLMSCommunicator *communicator)
        : m_communicator(communicator)
        , m_active(communicator && communicator->tryBeginMeterOperation())
    {
    }

    ~MeterOperationGuard()
    {
        if (m_active)
            m_communicator->endMeterOperation();
    }

    explicit operator bool() const { return m_active; }

private:
    GXDLMSCommunicator *m_communicator;
    bool m_active = false;
};

bool isCommunicationError(int errorCode)
{
    return errorCode == DLMS_ERROR_CODE_RECEIVE_FAILED
           || (errorCode & DLMS_ERROR_TYPE_COMMUNICATION_ERROR) != 0;
}

class NotificationPauseGuard
{
public:
    explicit NotificationPauseGuard(GXDLMSCommunicator *communicator)
        : m_communicator(communicator)
        , m_resume(communicator && communicator->notificationTimerActive())
    {
        if (m_resume)
            communicator->setNotificationPolling(false);
    }

    ~NotificationPauseGuard()
    {
        if (m_resume && m_communicator && m_communicator->deviceNotificationsEnabled())
            m_communicator->setNotificationPolling(true);
    }

private:
    GXDLMSCommunicator *m_communicator;
    bool m_resume = false;
};

class MeterOperationGuard
{
public:
    explicit MeterOperationGuard(GXDLMSCommunicator *communicator)
        : m_communicator(communicator)
        , m_active(communicator && communicator->tryBeginMeterOperation())
    {
    }

    ~MeterOperationGuard()
    {
        if (m_active)
            m_communicator->endMeterOperation();
    }

    explicit operator bool() const { return m_active; }

private:
    GXDLMSCommunicator *m_communicator;
    bool m_active = false;
};

} // namespace

GXDLMSCommunicator::GXDLMSCommunicator(GXDLMSDevice *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{
}

void GXDLMSCommunicator::initIo()
{
    if (m_media)
        return;

    m_media = std::make_unique<MediaConnection>();
    QObject::connect(m_media.get(), &MediaConnection::traceData, this,
                     &GXDLMSCommunicator::traceData);

    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setInterval(200);
    connect(m_notificationTimer, &QTimer::timeout, this, &GXDLMSCommunicator::pollNotifications);

    applyClientSettings();
}

GXDLMSCommunicator::~GXDLMSCommunicator()
{
    close();
}

bool GXDLMSCommunicator::usesAccessService() const
{
    if (!m_client || !m_client->GetCiphering())
        return false;
    if (m_client->GetCiphering()->GetSecurity() == DLMS_SECURITY_NONE)
        return false;
    const DLMS_CONFORMANCE negotiated = m_client->GetNegotiatedConformance();
    // Access-Request with ciphering requires General Protection; without it Gurux
    // cannot map ACCESS_REQUEST to a GLO command and message building fails.
    return (negotiated & DLMS_CONFORMANCE_ACCESS) != 0
           && (negotiated & DLMS_CONFORMANCE_GENERAL_PROTECTION) != 0;
}

void GXDLMSCommunicator::applyClientSettings()
{
    if (!m_device)
        return;

    m_passwordBuffer = m_device->password().toStdString();
    const char *password = m_passwordBuffer.empty() ? nullptr : m_passwordBuffer.c_str();

    m_client = std::make_unique<CGXDLMSSecureClient>(
        m_device->useLogicalNameReferencing(),
        m_device->clientAddress(),
        static_cast<int>(m_device->serverAddress()),
        static_cast<DLMS_AUTHENTICATION>(m_device->authentication()),
        password,
        static_cast<DLMS_INTERFACE_TYPE>(m_device->interfaceType()));

    if (!m_device->manufacturer().isEmpty()) {
        char manufacturerId[3] = {' ', ' ', ' '};
        const QByteArray bytes = m_device->manufacturer().left(3).toUpper().toLatin1();
        for (int i = 0; i < qMin(3, bytes.size()); ++i)
            manufacturerId[i] = bytes.at(i);
        m_client->SetManufacturerId(manufacturerId);
    }

    if (m_client->GetCiphering()) {
        m_client->GetCiphering()->SetSecurity(static_cast<DLMS_SECURITY>(m_device->security()));
        CGXByteBuffer bb;
        std::string authKey = m_device->authenticationKey().toStdString();
        if (!authKey.empty()) {
            bb.Clear();
            bb.SetHexString(authKey);
            m_client->GetCiphering()->SetAuthenticationKey(bb);
        }
        std::string blockKey = m_device->blockCipherKey().toStdString();
        if (!blockKey.empty()) {
            bb.Clear();
            bb.SetHexString(blockKey);
            m_client->GetCiphering()->SetBlockCipherKey(bb);
        }
        if (m_client->GetCiphering()->GetSecurity() != DLMS_SECURITY_NONE) {
            m_client->SetProposedConformance(static_cast<DLMS_CONFORMANCE>(
                m_client->GetProposedConformance() | DLMS_CONFORMANCE_GENERAL_PROTECTION));
        }
    }

    if (m_client->GetInterfaceType() == DLMS_INTERFACE_TYPE_PLC
        || m_client->GetInterfaceType() == DLMS_INTERFACE_TYPE_PLC_HDLC) {
        m_client->GetPlcSettings().SetMacSourceAddress(m_device->macSourceAddress());
        m_client->GetPlcSettings().SetMacDestinationAddress(m_device->macDestinationAddress());
    }
}

int GXDLMSCommunicator::sendData(CGXByteBuffer &data)
{
    return m_media->sendData(data.GetData(), static_cast<int>(data.GetSize()));
}

int GXDLMSCommunicator::readBytes(CGXByteBuffer &reply, unsigned char /*eop*/)
{
    QByteArray buffer;
    const int ret = m_media->readHdlcFrame(buffer);
    if (ret != 0)
        return ret;
    reply.Set(buffer.constData(), static_cast<unsigned long>(buffer.size()));
    return 0;
}

int GXDLMSCommunicator::readNetworkBytes(CGXByteBuffer &reply)
{
    QByteArray buffer;
    const int ret = m_media->readNetworkChunk(buffer);
    if (ret != 0)
        return ret;
    reply.Set(buffer.constData(), static_cast<unsigned long>(buffer.size()));
    return 0;
}

bool GXDLMSCommunicator::usesHdlcFrameDelimiter() const
{
    if (!m_client)
        return false;

    const DLMS_INTERFACE_TYPE iface = m_client->GetInterfaceType();
    return iface == DLMS_INTERFACE_TYPE_HDLC || iface == DLMS_INTERFACE_TYPE_HDLC_WITH_MODE_E
           || iface == DLMS_INTERFACE_TYPE_PLC_HDLC;
}

bool GXDLMSCommunicator::notificationTimerActive() const
{
    return m_notificationTimer && m_notificationTimer->isActive();
}

bool GXDLMSCommunicator::deviceNotificationsEnabled() const
{
    return m_device && m_device->notificationsEnabled();
}

int GXDLMSCommunicator::openMedia()
{
    if (!m_device || !m_media)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    m_media->clearReadStash();
    m_media->setWaitTimeMs(m_device->waitTimeMs());

    if (m_device->mediaType() == MediaType::Serial) {
        return m_media->openSerial(m_device->serialPort(), m_device->baudRate(),
                                   m_device->dataBits(), m_device->parity(), m_device->stopBits());
    }
    return m_media->openNetwork(m_device->hostName(), m_device->port());
}

int GXDLMSCommunicator::restoreApplicationAssociation()
{
    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = 0;

    if ((ret = m_client->SNRMRequest(data)) != 0
        || (ret = readDataBlock(data, reply, false)) != 0
        || (ret = parseUaResponse(reply.GetData())) != 0) {
        return ret;
    }

    reply.Clear();
    data.clear();
    if ((ret = m_client->AARQRequest(data)) != 0
        || (ret = readDataBlock(data, reply, false)) != 0) {
        return ret;
    }
    prepareApduBuffer(reply.GetData());
    if ((ret = m_client->ParseAAREResponse(reply.GetData())) != 0)
        return ret;

    reply.Clear();
    data.clear();
    if (m_client->GetAuthentication() > DLMS_AUTHENTICATION_LOW || m_client->IsAuthenticationRequired()) {
        if ((ret = m_client->GetApplicationAssociationRequest(data)) != 0
            || (ret = readDataBlock(data, reply, false)) != 0) {
            return ret;
        }
        prepareApduBuffer(reply.GetData());
        if ((ret = m_client->ParseApplicationAssociationResponse(reply.GetData())) != 0)
            return ret;
    }

    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::restoreConnection()
{
    if (!m_device || !m_media || !m_client)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    m_rxBuffer.Clear();
    m_media->close();

    int ret = openMedia();
    if (ret != 0) {
        notifyConnectionLost();
        return ret;
    }

    emit traceMessage(tr("Restoring DLMS session..."));
    ret = restoreApplicationAssociation();
    if (ret != 0) {
        notifyConnectionLost();
        return ret;
    }

    return DLMS_ERROR_CODE_OK;
}

void GXDLMSCommunicator::notifyConnectionLost()
{
    m_rxBuffer.Clear();
    if (m_media)
        m_media->close();
    emit connectionLost();
}

int GXDLMSCommunicator::readDLMSPacket(CGXByteBuffer &data, CGXReplyData &reply, bool /*allowReconnect*/)
{
    NotificationPauseGuard guard(this);

    if (data.GetSize() == 0 && !reply.IsStreaming())
        return DLMS_ERROR_CODE_OK;

    int ret = 0;
    if (data.GetSize() != 0) {
        m_rxBuffer.Clear();
        if ((ret = sendData(data)) != 0)
            return ret;
    }

    CGXReplyData notify;

    do {
        if (notify.GetData().GetSize() != 0) {
            if (!notify.IsMoreData()) {
                CGXByteBuffer notifyData = notify.GetData();
                const QByteArray payload(reinterpret_cast<const char *>(notifyData.GetData()),
                                         static_cast<int>(notifyData.GetSize()));
                emit notificationReceived(payload);
                notify.Clear();
            }
            continue;
        }

        if (usesHdlcFrameDelimiter()) {
            CGXByteBuffer chunk;
            if ((ret = readBytes(chunk, 0x7E)) != 0)
                return ret;
            if ((ret = m_rxBuffer.Set(&chunk)) != 0)
                return ret;
            ret = m_client->GetData(m_rxBuffer, reply, notify);
            if (ret == 0)
                prepareApduBuffer(reply.GetData());
        } else {
            CGXByteBuffer chunk;
            if ((ret = readNetworkBytes(chunk)) != 0)
                return ret;
            if ((ret = m_rxBuffer.Set(&chunk)) != 0)
                return ret;
            ret = m_client->GetData(m_rxBuffer, reply, notify);
        }
    } while (ret == DLMS_ERROR_CODE_FALSE);

    if (ret != DLMS_ERROR_CODE_FALSE) {
        if (m_rxBuffer.GetPosition() >= m_rxBuffer.GetSize())
            m_rxBuffer.Clear();
        else
            m_rxBuffer.Trim();
    }

    return ret;
}

int GXDLMSCommunicator::readDataBlock(CGXByteBuffer &data, CGXReplyData &reply, bool allowReconnect)
{
    if (data.GetSize() == 0)
        return DLMS_ERROR_CODE_OK;

    const auto runBlock = [&]() -> int {
        m_rxBuffer.Clear();
        if (m_media)
            m_media->clearReadStash();

        int ret = readDLMSPacket(data, reply, false);
        if (ret != 0)
            return ret;

        while (reply.IsMoreData()) {
            CGXByteBuffer rr;
            if (!reply.IsStreaming()) {
                if ((ret = m_client->ReceiverReady(reply.GetMoreData(), rr)) != 0)
                    return ret;
            }
            if ((ret = readDLMSPacket(rr, reply, false)) != 0)
                return ret;
        }

        return DLMS_ERROR_CODE_OK;
    };

    if (allowReconnect && m_media && !m_media->isOpen()) {
        if (restoreConnection() != 0)
            return DLMS_ERROR_CODE_RECEIVE_FAILED;
    }

    int ret = runBlock();
    if (ret != 0 && allowReconnect && isCommunicationError(ret)) {
        reply.Clear();
        if (restoreConnection() == 0)
            ret = runBlock();
    }

    return ret;
}

int GXDLMSCommunicator::readDataBlock(std::vector<CGXByteBuffer> &data, CGXReplyData &reply,
                                      bool allowReconnect)
{
    if (data.empty())
        return DLMS_ERROR_CODE_OK;

    int ret = 0;
    for (auto &packet : data) {
        reply.Clear();
        if ((ret = readDLMSPacket(packet, reply, false)) != 0)
            return ret;
        while (reply.IsMoreData()) {
            CGXByteBuffer rr;
            if (!reply.IsStreaming()) {
                if ((ret = m_client->ReceiverReady(reply.GetMoreData(), rr)) != 0)
                    return ret;
            }
            if ((ret = readDLMSPacket(rr, reply, false)) != 0)
                return ret;
        }
    }
    return DLMS_ERROR_CODE_OK;
}

namespace {

int extractHdlcXidParameters(CGXByteBuffer &in, unsigned long endPos, CGXByteBuffer &out)
{
    unsigned char b = 0;
    int ret = 0;
    while (in.GetPosition() < endPos && in.GetPosition() < in.GetSize()) {
        if ((ret = in.GetUInt8(in.GetPosition(), &b)) != 0)
            return ret;

        if (b == 0x00 && in.GetSize() - in.GetPosition() >= 2) {
            unsigned char next = 0;
            if (in.GetUInt8(in.GetPosition() + 1, &next) == 0 && next == 0x80) {
                in.SetPosition(in.GetPosition() + 2);
                continue;
            }
        }

        if (b == 0x81 && in.GetSize() - in.GetPosition() >= 3) {
            unsigned char group = 0;
            unsigned char groupLen = 0;
            if (in.GetUInt8(in.GetPosition() + 1, &group) == 0 && group == 0x80
                && in.GetUInt8(in.GetPosition() + 2, &groupLen) == 0) {
                const unsigned long groupStart = in.GetPosition() + 3;
                const unsigned long groupEnd = qMin(groupStart + groupLen, in.GetSize());
                in.SetPosition(groupStart);
                if ((ret = extractHdlcXidParameters(in, groupEnd, out)) != 0)
                    return ret;
                if (in.GetPosition() < groupEnd)
                    in.SetPosition(groupEnd);
                continue;
            }
        }

        if (b >= HDLC_INFO_MAX_INFO_TX && b <= HDLC_INFO_WINDOW_SIZE_RX) {
            unsigned char id = 0;
            unsigned char len = 0;
            if ((ret = in.GetUInt8(&id)) != 0 || (ret = in.GetUInt8(&len)) != 0)
                return ret;
            if (len != 1 && len != 2 && len != 4)
                return DLMS_ERROR_CODE_INVALID_PARAMETER;
            if ((ret = out.SetUInt8(id)) != 0 || (ret = out.SetUInt8(len)) != 0)
                return ret;
            for (unsigned char i = 0; i < len; ++i) {
                if ((ret = in.GetUInt8(&b)) != 0)
                    return ret;
                if ((ret = out.SetUInt8(b)) != 0)
                    return ret;
            }
            continue;
        }

        in.SetPosition(in.GetPosition() + 1);
    }
    return DLMS_ERROR_CODE_OK;
}

int flattenHdlcXidParameters(CGXByteBuffer &data, CGXByteBuffer &flat)
{
    flat.Clear();
    const unsigned long savedPos = data.GetPosition();
    data.SetPosition(0);

    int ret = extractHdlcXidParameters(data, data.GetSize(), flat);
    data.SetPosition(savedPos);
    if (ret != 0 || flat.GetSize() == 0)
        return ret != 0 ? ret : DLMS_ERROR_CODE_INVALID_PARAMETER;

    CGXByteBuffer wrapped;
    if ((ret = wrapped.SetUInt8(0x81)) != 0
        || (ret = wrapped.SetUInt8(0x80)) != 0
        || (ret = wrapped.SetUInt8(static_cast<unsigned char>(flat.GetSize()))) != 0
        || (ret = wrapped.Set(&flat)) != 0) {
        return ret;
    }

    flat = wrapped;
    flat.SetPosition(0);
    return DLMS_ERROR_CODE_OK;
}

} // namespace

int GXDLMSCommunicator::parseUaResponse(CGXByteBuffer &data)
{
    int ret = m_client->ParseUAResponse(data);
    if (ret != DLMS_ERROR_CODE_INVALID_PARAMETER)
        return ret;

    CGXByteBuffer flat;
    if (flattenHdlcXidParameters(data, flat) == 0) {
        ret = m_client->ParseUAResponse(flat);
        if (ret == DLMS_ERROR_CODE_OK)
            return ret;
    }

    // Some meters answer SNRM with nested user-info blocks. If parsing still fails,
    // keep Gurux defaults when the client sent SNRM without negotiation data.
    data.Clear();
    return m_client->ParseUAResponse(data);
}

int GXDLMSCommunicator::updateFrameCounter()
{
    if (!m_client || !m_client->GetCiphering()
        || m_client->GetCiphering()->GetSecurity() == DLMS_SECURITY_NONE) {
        return DLMS_ERROR_CODE_OK;
    }

    const unsigned long savedClient = m_client->GetClientAddress();
    const DLMS_AUTHENTICATION savedAuth = m_client->GetAuthentication();
    const DLMS_SECURITY savedSecurity = m_client->GetCiphering()->GetSecurity();
    CGXByteBuffer challenge = m_client->GetCtoSChallenge();

    m_client->SetProposedConformance(static_cast<DLMS_CONFORMANCE>(
        m_client->GetProposedConformance() | DLMS_CONFORMANCE_GENERAL_PROTECTION));
    m_client->SetClientAddress(16);
    m_client->SetAuthentication(DLMS_AUTHENTICATION_NONE);
    m_client->GetCiphering()->SetSecurity(DLMS_SECURITY_NONE);

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = 0;

    if ((ret = m_client->SNRMRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = parseUaResponse(reply.GetData())) != 0) {
        m_client->SetClientAddress(savedClient);
        m_client->SetAuthentication(savedAuth);
        m_client->GetCiphering()->SetSecurity(savedSecurity);
        m_client->SetCtoSChallenge(challenge);
        return ret;
    }

    reply.Clear();
    if ((ret = m_client->AARQRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0) {
        m_client->SetClientAddress(savedClient);
        m_client->SetAuthentication(savedAuth);
        m_client->GetCiphering()->SetSecurity(savedSecurity);
        m_client->SetCtoSChallenge(challenge);
        return ret;
    }
    prepareApduBuffer(reply.GetData());
    if ((ret = m_client->ParseAAREResponse(reply.GetData())) != 0) {
        m_client->SetClientAddress(savedClient);
        m_client->SetAuthentication(savedAuth);
        m_client->GetCiphering()->SetSecurity(savedSecurity);
        m_client->SetCtoSChallenge(challenge);
        return ret;
    }

    reply.Clear();
    CGXDLMSData counter(QStringLiteral("0.0.96.11.255.2.0").toStdString());
    if ((ret = m_client->Read(&counter, 2, data)) == 0
        && (ret = readDataBlock(data, reply)) == 0
        && (ret = m_client->UpdateValue(counter, 2, reply.GetValue())) == 0) {
        m_client->GetCiphering()->SetInvocationCounter(
            1 + static_cast<unsigned long>(counter.GetValue().ToInteger()));
    }

    disconnect();

    m_client->SetClientAddress(savedClient);
    m_client->SetAuthentication(savedAuth);
    m_client->GetCiphering()->SetSecurity(savedSecurity);
    m_client->SetCtoSChallenge(challenge);

    return ret;
}

int GXDLMSCommunicator::initializeConnection()
{
    if (!m_media)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    applyClientSettings();

    int ret = openMedia();
    if (ret != 0) {
        close();
        return ret;
    }

    if ((ret = updateFrameCounter()) != 0) {
        close();
        return ret;
    }

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;

    emit progressChanged(tr("Sending SNRM request..."), 1, 4);
    if ((ret = m_client->SNRMRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = parseUaResponse(reply.GetData())) != 0) {
        close();
        return ret;
    }

    reply.Clear();
    emit progressChanged(tr("Sending AARQ request..."), 2, 4);
    if ((ret = m_client->AARQRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0) {
        close();
        return ret;
    }
    prepareApduBuffer(reply.GetData());
    if ((ret = m_client->ParseAAREResponse(reply.GetData())) != 0) {
        close();
        return ret;
    }

    reply.Clear();
    if (m_client->GetAuthentication() > DLMS_AUTHENTICATION_LOW || m_client->IsAuthenticationRequired()) {
        emit progressChanged(tr("Authenticating..."), 3, 4);
        if ((ret = m_client->GetApplicationAssociationRequest(data)) != 0
            || (ret = readDataBlock(data, reply)) != 0) {
            close();
            return ret;
        }
        prepareApduBuffer(reply.GetData());
        if ((ret = m_client->ParseApplicationAssociationResponse(reply.GetData())) != 0) {
            close();
            return ret;
        }
    }

    emit progressChanged(tr("Connected."), 4, 4);
    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::disconnect()
{
    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    if (!m_media->isOpen())
        return 0;

    if (m_client->DisconnectRequest(data) == 0)
        readDataBlock(data, reply, false);
    return 0;
}

int GXDLMSCommunicator::close()
{
    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;

    if (m_media->isOpen()) {
        if (m_client->GetInterfaceType() == DLMS_INTERFACE_TYPE_WRAPPER
            || (m_client->GetCiphering()
                && m_client->GetCiphering()->GetSecurity() != DLMS_SECURITY_NONE)) {
            if (m_client->ReleaseRequest(data) == 0)
                readDataBlock(data, reply);
        }
        disconnect();
    }
    m_media->close();
    return 0;
}

int GXDLMSCommunicator::read(CGXDLMSObject *object, int attributeIndex, QString &value)
{
    if (m_linkDead.load() || !m_media || !m_media->isOpen())
        return DLMS_ERROR_CODE_RECEIVE_FAILED;

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = 0;

    if (usesAccessService()) {
        std::vector<CGXDLMSAccessItem> list;
        list.emplace_back(DLMS_ACCESS_SERVICE_COMMAND_TYPE_GET, object,
                          static_cast<unsigned char>(attributeIndex));
        if ((ret = m_client->AccessRequest(nullptr, list, data)) != 0)
            return ret;
        if ((ret = readDataBlock(data, reply)) != 0)
            return ret;
        prepareApduBuffer(reply.GetData());
        if ((ret = m_client->ParseAccessResponse(list, reply.GetData())) != 0)
            return ret;
        if (list.front().GetError() != DLMS_ERROR_CODE_OK)
            return list.front().GetError();
    } else {
        if ((ret = m_client->Read(object, attributeIndex, data)) != 0)
            return ret;
        if ((ret = readDataBlock(data, reply)) != 0)
            return ret;
        prepareApduBuffer(reply.GetData());
        if ((ret = m_client->UpdateValue(*object, attributeIndex, reply.GetValue())) != 0)
            return ret;
    }

    DLMS_DATA_TYPE type;
    if ((ret = object->GetDataType(attributeIndex, type)) != 0)
        return ret;
    if (type == DLMS_DATA_TYPE_NONE) {
        type = reply.GetValue().vt;
        if ((ret = object->SetDataType(attributeIndex, type)) != 0)
            return ret;
    }

    std::vector<std::string> values;
    object->GetValues(values);
    value = QString::fromStdString(values.at(static_cast<size_t>(attributeIndex - 1)));
    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::write(CGXDLMSObject *object, int attributeIndex, const QString &value, QString *error)
{
    CGXDLMSVariant variant;
    if (!VariantConverter::fromString(object, attributeIndex, value, variant, error))
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = m_client->Write(object, attributeIndex, variant, data);
    if (ret != 0)
        return ret;
    ret = readDataBlock(data, reply);
    if (ret != 0)
        return ret;
    return m_client->UpdateValue(*object, attributeIndex, reply.GetValue());
}

int GXDLMSCommunicator::method(CGXDLMSObject *object, int methodIndex, CGXDLMSVariant &variant)
{
    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = m_client->Method(object, methodIndex, variant, data);
    if (ret != 0)
        return ret;
    return readDataBlock(data, reply);
}

int GXDLMSCommunicator::readList(std::vector<std::pair<CGXDLMSObject *, unsigned char>> &list)
{
    if (list.empty())
        return DLMS_ERROR_CODE_OK;

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = m_client->ReadList(list, data);
    if (ret != 0)
        return ret;

    std::vector<CGXDLMSVariant> values;
    for (auto &packet : data) {
        if ((ret = readDataBlock(packet, reply)) != 0)
            return ret;
        if (reply.GetValue().vt == DLMS_DATA_TYPE_ARRAY) {
            values.insert(values.end(), reply.GetValue().Arr.begin(), reply.GetValue().Arr.end());
        }
        reply.Clear();
    }

    if (values.size() != list.size())
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    return m_client->UpdateValues(list, values);
}

int GXDLMSCommunicator::getAssociationView()
{
    emit progressChanged(tr("Reading association view..."), 1, 2);

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = 0;
    CGXDLMSVariant associationValue;

    if (usesAccessService()) {
        CGXDLMSAssociationLogicalName association(QStringLiteral("0.0.40.0.0.255").toStdString());
        std::vector<CGXDLMSAccessItem> list;
        list.emplace_back(DLMS_ACCESS_SERVICE_COMMAND_TYPE_GET, &association,
                          static_cast<unsigned char>(2));
        if ((ret = m_client->AccessRequest(nullptr, list, data)) != 0)
            return ret;
        if ((ret = readDataBlock(data, reply)) != 0)
            return ret;
        prepareApduBuffer(reply.GetData());
        if ((ret = m_client->ParseAccessResponse(list, reply.GetData())) != 0)
            return ret;
        if (list.front().GetError() != DLMS_ERROR_CODE_OK)
            return list.front().GetError();
        associationValue = list.front().GetValue();
    } else {
        if ((ret = m_client->GetObjectsRequest(data)) != 0)
            return ret;
        if ((ret = readDataBlock(data, reply)) != 0)
            return ret;
        associationValue = reply.GetValue();
    }

    emit progressChanged(tr("Parsing COSEM objects..."), 2, 2);
    if (associationValue.vt == DLMS_DATA_TYPE_ARRAY && !associationValue.Arr.empty()) {
        std::vector<CGXDLMSVariant> objects = associationValue.Arr;
        AssociationViewParser::normalizeObjects(objects);
        ret = m_client->ParseObjects(objects, true);
    } else if (!usesAccessService()) {
        prepareApduBuffer(reply.GetData());
        ret = m_client->ParseObjects(reply.GetData(), true);
    } else {
        ret = DLMS_ERROR_CODE_INVALID_RESPONSE;
    }
    if (ret != 0)
        return ret;

    CGXDLMSConverter converter;
    converter.UpdateOBISCodeInformation(m_client->GetObjects());
    return DLMS_ERROR_CODE_OK;
}

namespace {

int dlmsAccessErrorCode(int errorCode)
{
    if (errorCode >= DLMS_ERROR_CODE_OK && errorCode <= DLMS_ERROR_CODE_OTHER_REASON)
        return errorCode;
    return errorCode & 0xFF;
}

bool isSuppressibleReadError(int errorCode)
{
    switch (dlmsAccessErrorCode(errorCode)) {
    case DLMS_ERROR_CODE_HARDWARE_FAULT:
    case DLMS_ERROR_CODE_TEMPORARY_FAILURE:
    case DLMS_ERROR_CODE_READ_WRITE_DENIED:
    case DLMS_ERROR_CODE_UNDEFINED_OBJECT:
    case DLMS_ERROR_CODE_INCONSISTENT_CLASS_OR_OBJECT:
    case DLMS_ERROR_CODE_UNAVAILABLE_OBJECT:
    case DLMS_ERROR_CODE_UNMATCH_TYPE:
    case DLMS_ERROR_CODE_ACCESS_VIOLATED:
    case DLMS_ERROR_CODE_OTHER_REASON:
        return true;
    default:
        return false;
    }
}

bool shouldMarkAttributeNoAccess(CGXDLMSObject *object, int attributeIndex, int errorCode)
{
    if (object && object->GetObjectType() == DLMS_OBJECT_TYPE_PROFILE_GENERIC && attributeIndex == 2
        && dlmsAccessErrorCode(errorCode) == DLMS_ERROR_CODE_OTHER_REASON) {
        return false;
    }
    return isSuppressibleReadError(errorCode);
}

bool shouldSkipAttributeRead(CGXDLMSObject *object, int attributeIndex, bool forceAll)
{
    if (attributeIndex == 1)
        return true;

    if (object && object->GetObjectType() == DLMS_OBJECT_TYPE_PROFILE_GENERIC) {
        // Buffer must be read with selective access (Profile Generic dialog).
        if (attributeIndex == 2)
            return true;
        if (forceAll && attributeIndex == 3)
            return true;
    }

    return false;
}

void fillProfileGenericResult(CGXDLMSProfileGeneric *pg, ProfileGenericResult &result)
{
    result.columnHeaders.clear();
    for (auto &capture : pg->GetCaptureObjects()) {
        std::string logicalName;
        capture.first->GetLogicalName(logicalName);
        result.columnHeaders.append(QString::fromStdString(logicalName));
    }

    result.rows.clear();
    for (auto &row : pg->GetBuffer()) {
        ProfileGenericRow profileRow;
        for (auto &cell : row)
            profileRow.columns.append(QString::fromStdString(cell.ToString()));
        result.rows.append(profileRow);
    }
}

struct tm qDateTimeToProfileRangeTm(const QDateTime &dateTime)
{
    QDateTime utc = dateTime.toUTC();
    utc.setTime(QTime(utc.time().hour(), utc.time().minute(), 0));
    struct tm value {};
    value.tm_year = utc.date().year() - 1900;
    value.tm_mon = utc.date().month() - 1;
    value.tm_mday = utc.date().day();
    value.tm_hour = utc.time().hour();
    value.tm_min = utc.time().minute();
    value.tm_sec = 0;
    value.tm_isdst = -1;
    return value;
}

} // namespace

QString profileGenericErrorText(int errorCode)
{
    const char *message = CGXDLMSConverter::GetErrorMessage(errorCode);
    if (!message || !message[0])
        return QString::number(errorCode);
    return QString::fromUtf8(message);
}

int GXDLMSCommunicator::readProfileGenericColumns(CGXDLMSObject *object)
{
    if (!object || object->GetObjectType() != DLMS_OBJECT_TYPE_PROFILE_GENERIC)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    QString value;
    return read(object, 3, value);
}

int GXDLMSCommunicator::readProfileGenericByEntry(CGXDLMSObject *object, int index, int count,
                                                  ProfileGenericResult &result)
{
    result = ProfileGenericResult{};
    if (!object || object->GetObjectType() != DLMS_OBJECT_TYPE_PROFILE_GENERIC)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    auto *pg = static_cast<CGXDLMSProfileGeneric *>(object);
    int ret = readProfileGenericColumns(object);
    if (ret != 0)
        return ret;

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    if ((ret = m_client->ReadRowsByEntry(pg, index, count, data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = m_client->UpdateValue(*pg, 2, reply.GetValue())) != 0) {
        return ret;
    }

    fillProfileGenericResult(pg, result);
    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::readProfileGenericByRange(CGXDLMSObject *object, const QDateTime &start,
                                                  const QDateTime &end, ProfileGenericResult &result)
{
    result = ProfileGenericResult{};
    if (!object || object->GetObjectType() != DLMS_OBJECT_TYPE_PROFILE_GENERIC)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    auto *pg = static_cast<CGXDLMSProfileGeneric *>(object);
    int ret = readProfileGenericColumns(object);
    if (ret != 0)
        return ret;

    struct tm startTm = qDateTimeToProfileRangeTm(start);
    struct tm endTm = qDateTimeToProfileRangeTm(end);

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    if ((ret = m_client->ReadRowsByRange(pg, &startTm, &endTm, data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = m_client->UpdateValue(*pg, 2, reply.GetValue())) != 0) {
        return ret;
    }

    fillProfileGenericResult(pg, result);
    return DLMS_ERROR_CODE_OK;
}

QString GXDLMSCommunicator::proposedConformanceString() const
{
    if (!m_client)
        return {};
    return ConformanceHelper::conformanceToString(m_client->GetProposedConformance());
}

QString GXDLMSCommunicator::negotiatedConformanceString() const
{
    if (!m_client)
        return {};
    return ConformanceHelper::conformanceToString(m_client->GetNegotiatedConformance());
}

QList<ReadResult> GXDLMSCommunicator::readAll(bool forceAll, std::atomic<bool> *cancelFlag)
{
    QList<ReadResult> results;
    for (auto *obj : m_client->GetObjects()) {
        if (cancelFlag && cancelFlag->load())
            break;

        for (const ReadResult &partial : readObjectAttributes(obj, forceAll, cancelFlag)) {
            results.append(partial);
            if (isCommunicationError(partial.errorCode))
                return results;
        }
    }
    return results;
}

QList<ReadResult> GXDLMSCommunicator::readObjectAttributes(CGXDLMSObject *object, bool forceAll,
                                                           std::atomic<bool> *cancelFlag)
{
    QList<ReadResult> results;
    if (!object)
        return results;

    std::vector<int> indexes;
    object->GetAttributeIndexToRead(forceAll, indexes);
    for (int index : indexes) {
        if (cancelFlag && cancelFlag->load())
            break;

        if (m_linkDead.load()) {
            ReadResult result;
            result.object = object;
            result.attributeIndex = index;
            result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
            results.append(result);
            break;
        }

        if (shouldSkipAttributeRead(object, index, forceAll))
            continue;

        const DLMS_ACCESS_MODE access = object->GetAccess(index);
        if (access != DLMS_ACCESS_MODE_READ && access != DLMS_ACCESS_MODE_READ_WRITE)
            continue;

        ReadResult result;
        result.object = object;
        result.attributeIndex = index;
        result.errorCode = normalizeIoError(read(object, index, result.value));
        if (result.errorCode != 0) {
            if (shouldMarkAttributeNoAccess(object, index, result.errorCode))
                object->SetAccess(index, DLMS_ACCESS_MODE_NONE);
            if (isSuppressibleReadError(result.errorCode))
                continue;
            results.append(result);
            if (isCommunicationError(result.errorCode))
                break;
            continue;
        }
        results.append(result);

        if (m_device && m_device->mediaType() == MediaType::Network && result.errorCode == 0
            && indexes.size() > 1) {
            QThread::msleep(25);
        }
    }
    return results;
}

QList<ReadResult> GXDLMSCommunicator::readObjects(const QList<CGXDLMSObject *> &objects, bool forceAll,
                                                  std::atomic<bool> *cancelFlag)
{
    QList<ReadResult> results;
    int current = 0;
    const int total = objects.size();

    for (CGXDLMSObject *object : objects) {
        if (cancelFlag && cancelFlag->load())
            break;

        if (total > 1)
            emit progressChanged(tr("Reading..."), current, total);
        ++current;

        for (const ReadResult &partial : readObjectAttributes(object, forceAll, cancelFlag)) {
            results.append(partial);
            if (isCommunicationError(partial.errorCode))
                return results;
        }
    }

    return results;
}

bool GXDLMSCommunicator::tryBeginMeterOperation()
{
    bool expected = false;
    return m_meterOperationActive.compare_exchange_strong(expected, true);
}

void GXDLMSCommunicator::endMeterOperation()
{
    m_meterOperationActive = false;
}

void GXDLMSCommunicator::connectToMeter()
{
    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit connectFinished(DLMS_ERROR_CODE_RECEIVE_FAILED);
        return;
    }

    const int ret = syncConnect();
    emit connectFinished(ret);
}

int GXDLMSCommunicator::syncConnect()
{
    close();
    m_linkDead = false;

    int ret = initializeConnection();
    if (ret == 0) {
        m_client->GetObjects().Free();
        ret = getAssociationView();
    }
    if (ret != 0)
        close();
    else
        m_linkDead = false;
    return ret;
}

void GXDLMSCommunicator::disconnectFromMeter()
{
    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit disconnectFinished();
        return;
    }

    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit disconnectFinished();
        return;
    }

    syncDisconnect();
    emit disconnectFinished();
}

int GXDLMSCommunicator::syncDisconnect()
{
    return close();
}

void GXDLMSCommunicator::readAllFromMeter(bool forceRead)
{
    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readAllCompleted({});
        return;
    }

    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readAllCompleted({});
        return;
    }

    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    emit readAllCompleted(readAll(forceRead, m_cancelFlag));

    if (!m_linkDead.load() && m_media && !m_media->isOpen())
        notifyConnectionLost();
}

void GXDLMSCommunicator::readSelectedFromMeter(quintptr objectPtr, bool forceAll)
{
    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readSelectedCompleted({});
        return;
    }

    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readSelectedCompleted({});
        return;
    }

    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    emit readSelectedCompleted(readObjectAttributes(object, forceAll, m_cancelFlag));
}

void GXDLMSCommunicator::readObjectsFromMeter(const QList<quintptr> &objectPtrs, bool forceAll)
{
    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readSelectedCompleted({});
        return;
    }

    MeterOperationGuard guard(this);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        emit readSelectedCompleted({});
        return;
    }

    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    QList<CGXDLMSObject *> objects;
    objects.reserve(objectPtrs.size());
    for (quintptr objectPtr : objectPtrs)
        objects.append(reinterpret_cast<CGXDLMSObject *>(objectPtr));

    emit readSelectedCompleted(readObjects(objects, forceAll, m_cancelFlag));

    if (!m_linkDead.load() && m_media && !m_media->isOpen())
        notifyConnectionLost();
}

void GXDLMSCommunicator::readAttributeFromMeter(quintptr objectPtr, int attributeIndex)
{
    MeterOperationGuard guard(this);
    MeterOperationGuard guard(this);
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = attributeIndex;
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit readAttributeCompleted(result);
        return;
    }

    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit readAttributeCompleted(result);
        return;
    }

    result.errorCode = read(result.object, attributeIndex, result.value);
    emit readAttributeCompleted(result);
}

void GXDLMSCommunicator::writeAttributeToMeter(quintptr objectPtr, int attributeIndex,
                                               const QString &value)
{
    MeterOperationGuard guard(this);
    MeterOperationGuard guard(this);
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = attributeIndex;
    result.value = value;
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit writeCompleted(result);
        return;
    }

    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit writeCompleted(result);
        return;
    }

    result.errorCode = write(result.object, attributeIndex, value);
    emit writeCompleted(result);
}

void GXDLMSCommunicator::invokeMethodOnMeter(quintptr objectPtr, int methodIndex,
                                             const QString &parameter)
{
    MeterOperationGuard guard(this);
    MeterOperationGuard guard(this);
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = methodIndex;
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit methodInvokeCompleted(result);
        return;
    }

    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        result.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        emit methodInvokeCompleted(result);
        return;
    }

    CGXDLMSVariant variant;
    QString error;
    if (!parameter.trimmed().isEmpty()) {
        if (!VariantConverter::fromString(result.object, methodIndex, parameter, variant, &error)) {
            result.errorCode = DLMS_ERROR_CODE_INVALID_PARAMETER;
            result.value = error;
            emit methodInvokeCompleted(result);
            return;
        }
    }
    result.errorCode = method(result.object, methodIndex, variant);
    emit methodInvokeCompleted(result);
}

void GXDLMSCommunicator::readProfileGenericByEntryFromMeter(quintptr objectPtr, int index, int count)
{
    MeterOperationGuard guard(this);
    ProfileGenericResult pgResult;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        pgResult.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        pgResult.errorMessage = tr("Previous meter operation still running.");
        emit profileGenericCompleted(objectPtr, pgResult);
        return;
    }

    pgResult.errorCode = readProfileGenericByEntry(object, index, count, pgResult);
    if (pgResult.errorCode != 0) {
        pgResult.errorMessage =
            tr("Profile Generic read failed: %1").arg(profileGenericErrorText(pgResult.errorCode));
    }
    emit profileGenericCompleted(objectPtr, pgResult);
}

void GXDLMSCommunicator::readProfileGenericByRangeFromMeter(quintptr objectPtr, const QDateTime &start,
                                                            const QDateTime &end)
{
    MeterOperationGuard guard(this);
    ProfileGenericResult pgResult;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    if (!guard) {
        emit operationSkipped(tr("Previous meter operation still running."));
        pgResult.errorCode = DLMS_ERROR_CODE_RECEIVE_FAILED;
        pgResult.errorMessage = tr("Previous meter operation still running.");
        emit profileGenericCompleted(objectPtr, pgResult);
        return;
    }

    pgResult.errorCode = readProfileGenericByRange(object, start, end, pgResult);
    if (pgResult.errorCode != 0) {
        pgResult.errorMessage =
            tr("Profile Generic read failed: %1").arg(profileGenericErrorText(pgResult.errorCode));
        if (pgResult.errorCode == DLMS_ERROR_CODE_READ_WRITE_DENIED)
            pgResult.errorMessage += QLatin1Char('\n')
                                     + tr("This meter may not allow reading by date range. "
                                          "Try mode \"By entry\" instead.");
    }
    emit profileGenericCompleted(objectPtr, pgResult);
}

void GXDLMSCommunicator::setNotificationPolling(bool enabled)
{
    if (!m_notificationTimer)
        return;

    if (enabled) {
        m_notificationBuffer.clear();
        m_notificationTimer->start();
    } else {
        m_notificationTimer->stop();
        m_notificationBuffer.clear();
    }
}

void GXDLMSCommunicator::pollNotifications()
{
    if (!m_media || !m_media->isOpen() || !m_client)
        return;

    QByteArray chunk;
    if (m_media->readAvailable(chunk, 100) != 0 || chunk.isEmpty())
        return;

    m_notificationBuffer.append(chunk);
    const unsigned char eop = m_client->GetInterfaceType() == DLMS_INTERFACE_TYPE_HDLC ? 0x7E : 0x00;
    if (eop == 0x7E) {
        int start = 0;
        for (int i = 0; i < m_notificationBuffer.size(); ++i) {
            if (static_cast<unsigned char>(m_notificationBuffer.at(i)) == eop) {
                const QByteArray frame = m_notificationBuffer.mid(start, i - start + 1);
                if (frame.size() > 2)
                    emit notificationReceived(frame);
                start = i + 1;
            }
        }
        if (start > 0)
            m_notificationBuffer.remove(0, start);
    } else {
        emit notificationReceived(m_notificationBuffer);
        m_notificationBuffer.clear();
    }
}

void GXDLMSCommunicator::shutdown()
{
    if (m_notificationTimer)
        m_notificationTimer->stop();
    m_notificationBuffer.clear();
    close();
}
