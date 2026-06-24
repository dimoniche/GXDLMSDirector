#include "GXDLMSDevice.h"
#include "GXDLMSCommunicator.h"
#include "ConformanceHelper.h"
#include "HdlcAddressHelper.h"
#include "VariantConverter.h"

#include <GXDLMSConverter.h>
#include <errorcodes.h>
#include <enums.h>

#include <QMetaObject>
#include <QThread>

namespace {

QString formatDlmsError(int errorCode)
{
    const char *message = CGXDLMSConverter::GetErrorMessage(errorCode);
    if (!message || !message[0])
        return QString::number(errorCode);
    return QString::fromUtf8(message);
}

} // namespace

unsigned long GXDLMSDevice::serverAddress() const
{
    return HdlcAddressHelper::encodeServerAddress(m_serverLogicalAddress, m_serverPhysicalAddress);
}

void GXDLMSDevice::setServerAddress(unsigned long address)
{
    HdlcAddressHelper::decodeServerAddress(address, m_serverLogicalAddress, m_serverPhysicalAddress);
}

GXDLMSDevice::GXDLMSDevice(QObject *parent)
    : QObject(parent)
    , m_communicator(std::make_unique<GXDLMSCommunicator>(this, nullptr))
    , m_ioThread(new QThread(this))
{
    m_communicator->moveToThread(m_ioThread);

    connect(m_communicator.get(), &GXDLMSCommunicator::traceMessage,
            this, &GXDLMSDevice::traceMessage);
    connect(m_communicator.get(), &GXDLMSCommunicator::traceData,
            this, &GXDLMSDevice::traceData);
    connect(m_communicator.get(), &GXDLMSCommunicator::notificationReceived,
            this, &GXDLMSDevice::notificationReceived);
    connect(m_communicator.get(), &GXDLMSCommunicator::progressChanged,
            this, &GXDLMSDevice::progressChanged);
    connect(m_communicator.get(), &GXDLMSCommunicator::errorOccurred,
            this, &GXDLMSDevice::errorOccurred);

    connect(m_communicator.get(), &GXDLMSCommunicator::connectFinished,
            this, &GXDLMSDevice::handleConnectionFinished);
    connect(m_communicator.get(), &GXDLMSCommunicator::disconnectFinished,
            this, &GXDLMSDevice::handleDisconnectionFinished);
    connect(m_communicator.get(), &GXDLMSCommunicator::readAllCompleted,
            this, &GXDLMSDevice::handleReadAllFinished);
    connect(m_communicator.get(), &GXDLMSCommunicator::readSelectedCompleted, this,
            [this](const QList<ReadResult> &results) {
                for (const ReadResult &result : results) {
                    if (result.errorCode == 0)
                        emit objectRead(result.object, result.attributeIndex, result.value);
                    else
                        emit errorOccurred(tr("Read failed: %1").arg(formatDlmsError(result.errorCode)));
                }
                setState(m_state & ~DeviceStates(DeviceState::Reading));
            });
    connect(m_communicator.get(), &GXDLMSCommunicator::readAttributeCompleted, this,
            [this](const ReadResult &result) {
                handleObjectReadFinished(result.object, result.attributeIndex, result.value,
                                         result.errorCode);
            });
    connect(m_communicator.get(), &GXDLMSCommunicator::writeCompleted, this,
            [this](const ReadResult &result) {
                handleObjectWriteFinished(result.object, result.attributeIndex, result.value,
                                          result.errorCode);
            });
    connect(m_communicator.get(), &GXDLMSCommunicator::methodInvokeCompleted, this,
            [this](const ReadResult &result) {
                handleMethodInvokeFinished(result.object, result.attributeIndex, result.errorCode);
            });
    connect(m_communicator.get(), &GXDLMSCommunicator::profileGenericCompleted, this,
            &GXDLMSDevice::handleProfileGenericFinished);

    m_ioThread->start();
    QMetaObject::invokeMethod(m_communicator.get(), "initIo", Qt::BlockingQueuedConnection);
}

GXDLMSDevice::~GXDLMSDevice()
{
    if (m_communicator && m_ioThread) {
        QMetaObject::invokeMethod(m_communicator.get(), "shutdown", Qt::BlockingQueuedConnection);
        m_ioThread->quit();
        m_ioThread->wait(5000);
    }
}

void GXDLMSDevice::setState(DeviceStates state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void GXDLMSDevice::connectAsync()
{
    setState(DeviceState::Connecting);
    QMetaObject::invokeMethod(m_communicator.get(), "connectToMeter", Qt::QueuedConnection);
}

void GXDLMSDevice::disconnectAsync()
{
    setState(DeviceState::Disconnecting);
    QMetaObject::invokeMethod(m_communicator.get(), "disconnectFromMeter", Qt::QueuedConnection);
}

void GXDLMSDevice::readAllAsync()
{
    if (!(m_state & DeviceState::Connected))
        return;

    m_cancelRequested = false;
    setState(m_state | DeviceState::Reading);
    QMetaObject::invokeMethod(m_communicator.get(), "readAllFromMeter", Qt::QueuedConnection,
                              Q_ARG(bool, m_forceRead));
}

void GXDLMSDevice::readSelectedObjectAsync(CGXDLMSObject *object, bool forceAll)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    m_cancelRequested = false;
    setState(m_state | DeviceState::Reading);
    QMetaObject::invokeMethod(m_communicator.get(), "readSelectedFromMeter", Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(bool, forceAll));
}

void GXDLMSDevice::readObjectsAsync(const QList<CGXDLMSObject *> &objects, bool forceAll)
{
    if (!(m_state & DeviceState::Connected) || objects.isEmpty())
        return;

    m_cancelRequested = false;
    setState(m_state | DeviceState::Reading);

    QList<quintptr> objectPtrs;
    objectPtrs.reserve(objects.size());
    for (CGXDLMSObject *object : objects)
        objectPtrs.append(reinterpret_cast<quintptr>(object));

    QMetaObject::invokeMethod(m_communicator.get(), "readObjectsFromMeter", Qt::QueuedConnection,
                              Q_ARG(QList<quintptr>, objectPtrs),
                              Q_ARG(bool, forceAll));
}

void GXDLMSDevice::readObjectAsync(CGXDLMSObject *object, int attributeIndex)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Reading);
    QMetaObject::invokeMethod(m_communicator.get(), "readAttributeFromMeter", Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(int, attributeIndex));
}

void GXDLMSDevice::applyConnectionSettings()
{
    QMetaObject::invokeMethod(m_communicator.get(), "applyClientSettings", Qt::QueuedConnection);
}

void GXDLMSDevice::writeObjectAsync(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Writing);
    QMetaObject::invokeMethod(m_communicator.get(), "writeAttributeToMeter", Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(int, attributeIndex),
                              Q_ARG(QString, value));
}

void GXDLMSDevice::invokeMethodAsync(CGXDLMSObject *object, int methodIndex, const QString &parameter)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Writing);
    QMetaObject::invokeMethod(m_communicator.get(), "invokeMethodOnMeter", Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(int, methodIndex),
                              Q_ARG(QString, parameter));
}

void GXDLMSDevice::setConformanceInfo(const QString &proposed, const QString &negotiated)
{
    m_proposedConformance = proposed;
    m_negotiatedConformance = negotiated;
}

void GXDLMSDevice::handleConnectionFinished(int ret)
{
    if (ret == 0) {
        setConformanceInfo(m_communicator->proposedConformanceString(),
                           m_communicator->negotiatedConformanceString());
        setState(DeviceState::Connected | DeviceState::Initialized);
        if (m_notificationsEnabled) {
            QMetaObject::invokeMethod(m_communicator.get(), "setNotificationPolling",
                                      Qt::QueuedConnection, Q_ARG(bool, true));
        }
        emit traceMessage(tr("Association view loaded: %1 objects")
                              .arg(static_cast<int>(m_communicator->client()->GetObjects().size())));
    } else {
        setState(DeviceState::None);
        emit errorOccurred(QString::fromUtf8(CGXDLMSConverter::GetErrorMessage(ret)));
    }
}

void GXDLMSDevice::handleDisconnectionFinished()
{
    QMetaObject::invokeMethod(m_communicator.get(), "setNotificationPolling", Qt::QueuedConnection,
                              Q_ARG(bool, false));
    setState(DeviceState::None);
}

void GXDLMSDevice::handleObjectReadFinished(CGXDLMSObject *object, int attributeIndex,
                                            const QString &value, int ret)
{
    setState(m_state & ~DeviceStates(DeviceState::Reading));
    if (ret == 0)
        emit objectRead(object, attributeIndex, value);
    else
        emit errorOccurred(tr("Read failed: %1").arg(formatDlmsError(ret)));
}

void GXDLMSDevice::handleObjectWriteFinished(CGXDLMSObject *object, int attributeIndex,
                                             const QString &value, int ret)
{
    setState(m_state & ~DeviceStates(DeviceState::Writing));
    if (ret == 0)
        emit objectWritten(object, attributeIndex, value);
    else
        emit errorOccurred(tr("Write failed: %1").arg(ret));
}

void GXDLMSDevice::handleMethodInvokeFinished(CGXDLMSObject *object, int methodIndex, int ret)
{
    setState(m_state & ~DeviceStates(DeviceState::Writing));
    if (ret == 0)
        emit methodInvoked(object, methodIndex);
    else
        emit errorOccurred(tr("Method invoke failed: %1").arg(ret));
}

void GXDLMSDevice::readProfileGenericByEntryAsync(CGXDLMSObject *object, int index, int count)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Reading);
    QMetaObject::invokeMethod(m_communicator.get(), "readProfileGenericByEntryFromMeter",
                              Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(int, index),
                              Q_ARG(int, count));
}

void GXDLMSDevice::readProfileGenericByRangeAsync(CGXDLMSObject *object, const QDateTime &start,
                                                  const QDateTime &end)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Reading);
    QMetaObject::invokeMethod(m_communicator.get(), "readProfileGenericByRangeFromMeter",
                              Qt::QueuedConnection,
                              Q_ARG(quintptr, reinterpret_cast<quintptr>(object)),
                              Q_ARG(QDateTime, start),
                              Q_ARG(QDateTime, end));
}

void GXDLMSDevice::handleProfileGenericFinished(quintptr objectPtr, const ProfileGenericResult &result)
{
    auto *object = reinterpret_cast<CGXDLMSObject *>(objectPtr);
    setState(m_state & ~DeviceStates(DeviceState::Reading));
    emit profileGenericRead(object, result);
}

void GXDLMSDevice::handleReadAllFinished(const QList<ReadResult> &results)
{
    for (const ReadResult &result : results) {
        if (result.errorCode == 0)
            emit objectRead(result.object, result.attributeIndex, result.value);
    }

    const bool cancelled = m_cancelRequested.exchange(false);
    setState(m_state & ~DeviceStates(DeviceState::Reading));
    emit readAllFinished();
    if (cancelled)
        emit traceMessage(tr("Operation cancelled."));
}

void GXDLMSDevice::cancelOperation()
{
    m_cancelRequested = true;
}

void GXDLMSDevice::setNotificationsEnabled(bool enabled)
{
    m_notificationsEnabled = enabled;
    if (!m_communicator)
        return;

    const bool active = enabled && (m_state & DeviceState::Connected);
    QMetaObject::invokeMethod(m_communicator.get(), "setNotificationPolling", Qt::QueuedConnection,
                              Q_ARG(bool, active));
}
