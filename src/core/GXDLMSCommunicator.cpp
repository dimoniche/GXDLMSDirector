#include "GXDLMSCommunicator.h"
#include "GXDLMSDevice.h"

#include "AssociationViewParser.h"
#include "VariantConverter.h"
#include "ConformanceHelper.h"

#include <GXDLMSConverter.h>
#include <GXDLMSProfileGeneric.h>
#include <GXDLMSTranslator.h>
#include <GXDLMSObject.h>
#include <GXDateTime.h>
#include <enums.h>
#include <errorcodes.h>

#include <QList>

#include <QDateTime>
#include <QTime>
#include <QTimer>

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

int GXDLMSCommunicator::readBytes(CGXByteBuffer &reply, unsigned char eop)
{
    QByteArray buffer;
    const int ret = m_media->readData(buffer, eop);
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

bool GXDLMSCommunicator::usesSerialFrameDelimiter() const
{
    if (m_media->type() != MediaType::Serial)
        return false;

    const DLMS_INTERFACE_TYPE iface = m_client->GetInterfaceType();
    return iface == DLMS_INTERFACE_TYPE_HDLC || iface == DLMS_INTERFACE_TYPE_HDLC_WITH_MODE_E
           || iface == DLMS_INTERFACE_TYPE_PLC_HDLC;
}

int GXDLMSCommunicator::readDLMSPacket(CGXByteBuffer &data, CGXReplyData &reply)
{
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

        if (usesSerialFrameDelimiter()) {
            CGXByteBuffer bb;
            if ((ret = readBytes(bb, 0x7E)) != 0)
                return ret;
            ret = m_client->GetData(bb, reply, notify);
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

int GXDLMSCommunicator::readDataBlock(CGXByteBuffer &data, CGXReplyData &reply)
{
    if (data.GetSize() == 0)
        return DLMS_ERROR_CODE_OK;

    int ret = readDLMSPacket(data, reply);
    if (ret != 0)
        return ret;

    while (reply.IsMoreData()) {
        CGXByteBuffer rr;
        if (!reply.IsStreaming()) {
            if ((ret = m_client->ReceiverReady(reply.GetMoreData(), rr)) != 0)
                return ret;
        }
        if ((ret = readDLMSPacket(rr, reply)) != 0)
            return ret;
    }

    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::readDataBlock(std::vector<CGXByteBuffer> &data, CGXReplyData &reply)
{
    if (data.empty())
        return DLMS_ERROR_CODE_OK;

    int ret = 0;
    for (auto &packet : data) {
        reply.Clear();
        if ((ret = readDLMSPacket(packet, reply)) != 0)
            return ret;
        while (reply.IsMoreData()) {
            CGXByteBuffer rr;
            if (!reply.IsStreaming()) {
                if ((ret = m_client->ReceiverReady(reply.GetMoreData(), rr)) != 0)
                    return ret;
            }
            if ((ret = readDLMSPacket(rr, reply)) != 0)
                return ret;
        }
    }
    return DLMS_ERROR_CODE_OK;
}

int GXDLMSCommunicator::initializeConnection()
{
    if (!m_media)
        return DLMS_ERROR_CODE_INVALID_PARAMETER;

    applyClientSettings();
    m_media->setWaitTimeMs(m_device->waitTimeMs());

    int ret = 0;
    if (m_device->mediaType() == MediaType::Serial) {
        ret = m_media->openSerial(m_device->serialPort(), m_device->baudRate(),
                                  m_device->dataBits(), m_device->parity(), m_device->stopBits());
    } else {
        ret = m_media->openNetwork(m_device->hostName(), m_device->port());
    }
    if (ret != 0) {
        close();
        return ret;
    }

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;

    emit progressChanged(tr("Sending SNRM request..."), 1, 4);
    if ((ret = m_client->SNRMRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = m_client->ParseUAResponse(reply.GetData())) != 0) {
        close();
        return ret;
    }

    reply.Clear();
    emit progressChanged(tr("Sending AARQ request..."), 2, 4);
    if ((ret = m_client->AARQRequest(data)) != 0
        || (ret = readDataBlock(data, reply)) != 0
        || (ret = m_client->ParseAAREResponse(reply.GetData())) != 0) {
        close();
        return ret;
    }

    reply.Clear();
    if (m_client->IsAuthenticationRequired()) {
        emit progressChanged(tr("Authenticating..."), 3, 4);
        if ((ret = m_client->GetApplicationAssociationRequest(data)) != 0
            || (ret = readDataBlock(data, reply)) != 0
            || (ret = m_client->ParseApplicationAssociationResponse(reply.GetData())) != 0) {
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
        readDataBlock(data, reply);
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
    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = m_client->Read(object, attributeIndex, data);
    if (ret != 0)
        return ret;
    if ((ret = readDataBlock(data, reply)) != 0)
        return ret;
    if ((ret = m_client->UpdateValue(*object, attributeIndex, reply.GetValue())) != 0)
        return ret;

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
    return 0;
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
    int ret = m_client->GetObjectsRequest(data);
    if (ret != 0)
        return ret;
    if ((ret = readDataBlock(data, reply)) != 0)
        return ret;

    emit progressChanged(tr("Parsing COSEM objects..."), 2, 2);
    if (reply.GetValue().vt == DLMS_DATA_TYPE_ARRAY && !reply.GetValue().Arr.empty()) {
        std::vector<CGXDLMSVariant> objects = reply.GetValue().Arr;
        AssociationViewParser::normalizeObjects(objects);
        ret = m_client->ParseObjects(objects, true);
    } else {
        reply.GetData().SetPosition(0);
        ret = m_client->ParseObjects(reply.GetData(), true);
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

        for (const ReadResult &partial : readObjectAttributes(obj, forceAll, cancelFlag))
            results.append(partial);
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

        if (shouldSkipAttributeRead(object, index, forceAll))
            continue;

        const DLMS_ACCESS_MODE access = object->GetAccess(index);
        if (access != DLMS_ACCESS_MODE_READ && access != DLMS_ACCESS_MODE_READ_WRITE)
            continue;

        ReadResult result;
        result.object = object;
        result.attributeIndex = index;
        result.errorCode = read(object, index, result.value);
        if (result.errorCode != 0) {
            if (shouldMarkAttributeNoAccess(object, index, result.errorCode))
                object->SetAccess(index, DLMS_ACCESS_MODE_NONE);
            if (isSuppressibleReadError(result.errorCode))
                continue;
        }
        results.append(result);
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

        for (const ReadResult &partial : readObjectAttributes(object, forceAll, cancelFlag))
            results.append(partial);
    }

    return results;
}

void GXDLMSCommunicator::connectToMeter()
{
    int ret = syncConnect();
    emit connectFinished(ret);
}

int GXDLMSCommunicator::syncConnect()
{
    int ret = initializeConnection();
    if (ret == 0) {
        m_client->GetObjects().Free();
        ret = getAssociationView();
    }
    if (ret != 0)
        close();
    return ret;
}

void GXDLMSCommunicator::disconnectFromMeter()
{
    syncDisconnect();
    emit disconnectFinished();
}

int GXDLMSCommunicator::syncDisconnect()
{
    return close();
}

void GXDLMSCommunicator::readAllFromMeter(bool forceRead)
{
    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    emit readAllCompleted(readAll(forceRead, m_cancelFlag));
}

void GXDLMSCommunicator::readSelectedFromMeter(quintptr objectPtr, bool forceAll)
{
    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    emit readSelectedCompleted(readObjectAttributes(object, forceAll, m_cancelFlag));
}

void GXDLMSCommunicator::readObjectsFromMeter(const QList<quintptr> &objectPtrs, bool forceAll)
{
    m_cancelFlag = m_device ? &m_device->cancelFlag() : nullptr;
    QList<CGXDLMSObject *> objects;
    objects.reserve(objectPtrs.size());
    for (quintptr objectPtr : objectPtrs)
        objects.append(reinterpret_cast<CGXDLMSObject *>(objectPtr));

    emit readSelectedCompleted(readObjects(objects, forceAll, m_cancelFlag));
}

void GXDLMSCommunicator::readAttributeFromMeter(quintptr objectPtr, int attributeIndex)
{
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = attributeIndex;
    result.errorCode = read(result.object, attributeIndex, result.value);
    emit readAttributeCompleted(result);
}

void GXDLMSCommunicator::writeAttributeToMeter(quintptr objectPtr, int attributeIndex,
                                               const QString &value)
{
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = attributeIndex;
    result.value = value;
    result.errorCode = write(result.object, attributeIndex, value);
    emit writeCompleted(result);
}

void GXDLMSCommunicator::invokeMethodOnMeter(quintptr objectPtr, int methodIndex,
                                             const QString &parameter)
{
    ReadResult result;
    result.object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    result.attributeIndex = methodIndex;
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
    ProfileGenericResult pgResult;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
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
    ProfileGenericResult pgResult;
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
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
