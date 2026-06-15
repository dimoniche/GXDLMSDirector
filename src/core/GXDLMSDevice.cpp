#include "GXDLMSDevice.h"
#include "GXDLMSCommunicator.h"
#include "ConformanceHelper.h"
#include "VariantConverter.h"

#include <errorcodes.h>
#include <enums.h>

#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>

struct ProfileGenericTaskResult
{
    CGXDLMSObject *object = nullptr;
    ProfileGenericResult result;
};

GXDLMSDevice::GXDLMSDevice(QObject *parent)
    : QObject(parent)
    , m_communicator(std::make_unique<GXDLMSCommunicator>(this))
    , m_notificationTimer(new QTimer(this))
{
    m_notificationTimer->setInterval(200);
    connect(m_notificationTimer, &QTimer::timeout, this, &GXDLMSDevice::onNotificationPoll);

    QObject::connect(m_communicator.get(), &GXDLMSCommunicator::traceMessage,
                     this, &GXDLMSDevice::traceMessage);
    QObject::connect(m_communicator.get(), &GXDLMSCommunicator::traceData,
                     this, &GXDLMSDevice::traceData);
    QObject::connect(m_communicator.get(), &GXDLMSCommunicator::notificationReceived,
                     this, &GXDLMSDevice::notificationReceived);
    QObject::connect(m_communicator.get(), &GXDLMSCommunicator::progressChanged,
                     this, &GXDLMSDevice::progressChanged);
    QObject::connect(m_communicator.get(), &GXDLMSCommunicator::errorOccurred,
                     this, &GXDLMSDevice::errorOccurred);
}

GXDLMSDevice::~GXDLMSDevice()
{
    if (m_communicator)
        m_communicator->close();
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

    auto *watcher = new QFutureWatcher<int>(this);
    QObject::connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        handleConnectionFinished(watcher->result());
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([this]() {
        int ret = m_communicator->initializeConnection();
        if (ret == 0)
            ret = m_communicator->getAssociationView();
        return ret;
    }));
}

void GXDLMSDevice::disconnectAsync()
{
    setState(DeviceState::Disconnecting);

    auto *watcher = new QFutureWatcher<void>(this);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        handleDisconnectionFinished();
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([this]() {
        m_communicator->close();
    }));
}

void GXDLMSDevice::readAllAsync()
{
    if (!(m_state & DeviceState::Connected))
        return;

    m_cancelRequested = false;
    setState(m_state | DeviceState::Reading);

    auto *watcher = new QFutureWatcher<QList<ReadResult>>(this);
    QObject::connect(watcher, &QFutureWatcher<QList<ReadResult>>::finished, this,
                     [this, watcher]() {
                         for (const ReadResult &result : watcher->result()) {
                             if (result.errorCode == 0)
                                 emit objectRead(result.object, result.attributeIndex, result.value);
                         }
                         handleReadAllFinished();
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this]() {
        return m_communicator->readAll(m_forceRead, &m_cancelRequested);
    }));
}

void GXDLMSDevice::readSelectedObjectAsync(CGXDLMSObject *object, bool forceAll)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    m_cancelRequested = false;
    setState(m_state | DeviceState::Reading);

    auto *watcher = new QFutureWatcher<QList<ReadResult>>(this);
    QObject::connect(watcher, &QFutureWatcher<QList<ReadResult>>::finished, this,
                     [this, watcher]() {
                         for (const ReadResult &result : watcher->result()) {
                             if (result.errorCode == 0)
                                 emit objectRead(result.object, result.attributeIndex, result.value);
                             else
                                 emit errorOccurred(tr("Read failed: %1").arg(result.errorCode));
                         }
                         setState((m_state & ~DeviceStates(DeviceState::Reading)) | DeviceState::Connected);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, forceAll]() {
        return m_communicator->readObjectAttributes(object, forceAll, &m_cancelRequested);
    }));
}

void GXDLMSDevice::readObjectAsync(CGXDLMSObject *object, int attributeIndex)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Reading);

    auto *watcher = new QFutureWatcher<ReadResult>(this);
    QObject::connect(watcher, &QFutureWatcher<ReadResult>::finished, this,
                     [this, watcher]() {
                         const ReadResult result = watcher->result();
                         handleObjectReadFinished(result.object, result.attributeIndex, result.value,
                                                  result.errorCode);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, attributeIndex]() {
        ReadResult result;
        result.object = object;
        result.attributeIndex = attributeIndex;
        result.errorCode = m_communicator->read(object, attributeIndex, result.value);
        return result;
    }));
}

void GXDLMSDevice::applyConnectionSettings()
{
    m_communicator->applyClientSettings();
}

void GXDLMSDevice::writeObjectAsync(CGXDLMSObject *object, int attributeIndex, const QString &value)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Writing);

    auto *watcher = new QFutureWatcher<ReadResult>(this);
    QObject::connect(watcher, &QFutureWatcher<ReadResult>::finished, this,
                     [this, watcher]() {
                         const ReadResult result = watcher->result();
                         handleObjectWriteFinished(result.object, result.attributeIndex, result.value,
                                                   result.errorCode);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, attributeIndex, value]() {
        ReadResult result;
        result.object = object;
        result.attributeIndex = attributeIndex;
        result.value = value;
        result.errorCode = m_communicator->write(object, attributeIndex, value);
        return result;
    }));
}

void GXDLMSDevice::invokeMethodAsync(CGXDLMSObject *object, int methodIndex, const QString &parameter)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Writing);

    auto *watcher = new QFutureWatcher<ReadResult>(this);
    QObject::connect(watcher, &QFutureWatcher<ReadResult>::finished, this,
                     [this, watcher]() {
                         const ReadResult result = watcher->result();
                         handleMethodInvokeFinished(result.object, result.attributeIndex, result.errorCode);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, methodIndex, parameter]() {
        ReadResult result;
        result.object = object;
        result.attributeIndex = methodIndex;
        CGXDLMSVariant variant;
        QString error;
        if (!parameter.trimmed().isEmpty()) {
            if (!VariantConverter::fromString(object, methodIndex, parameter, variant, &error)) {
                result.errorCode = DLMS_ERROR_CODE_INVALID_PARAMETER;
                result.value = error;
                return result;
            }
        }
        result.errorCode = m_communicator->method(object, methodIndex, variant);
        return result;
    }));
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
        if (m_notificationsEnabled)
            m_notificationTimer->start();
    } else {
        setState(DeviceState::None);
    }
}

void GXDLMSDevice::handleDisconnectionFinished()
{
    m_notificationTimer->stop();
    m_notificationBuffer.clear();
    setState(DeviceState::None);
}

void GXDLMSDevice::handleObjectReadFinished(CGXDLMSObject *object, int attributeIndex,
                                            const QString &value, int ret)
{
    setState((m_state & ~DeviceStates(DeviceState::Reading)) | DeviceState::Connected);
    if (ret == 0)
        emit objectRead(object, attributeIndex, value);
    else
        emit errorOccurred(tr("Read failed: %1").arg(ret));
}

void GXDLMSDevice::handleObjectWriteFinished(CGXDLMSObject *object, int attributeIndex,
                                             const QString &value, int ret)
{
    setState((m_state & ~DeviceStates(DeviceState::Writing)) | DeviceState::Connected);
    if (ret == 0)
        emit objectWritten(object, attributeIndex, value);
    else
        emit errorOccurred(tr("Write failed: %1").arg(ret));
}

void GXDLMSDevice::handleMethodInvokeFinished(CGXDLMSObject *object, int methodIndex, int ret)
{
    setState((m_state & ~DeviceStates(DeviceState::Writing)) | DeviceState::Connected);
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

    auto *watcher = new QFutureWatcher<ProfileGenericTaskResult>(this);
    QObject::connect(watcher, &QFutureWatcher<ProfileGenericTaskResult>::finished, this,
                     [this, watcher]() {
                         const ProfileGenericTaskResult taskResult = watcher->result();
                         handleProfileGenericFinished(taskResult.object, taskResult.result);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, index, count]() {
        ProfileGenericTaskResult taskResult;
        taskResult.object = object;
        taskResult.result.errorCode =
            m_communicator->readProfileGenericByEntry(object, index, count, taskResult.result);
        if (taskResult.result.errorCode != 0) {
            taskResult.result.errorMessage =
                tr("Profile Generic read failed: %1").arg(taskResult.result.errorCode);
        }
        return taskResult;
    }));
}

void GXDLMSDevice::readProfileGenericByRangeAsync(CGXDLMSObject *object, const QDateTime &start,
                                                  const QDateTime &end)
{
    if (!(m_state & DeviceState::Connected) || !object)
        return;

    setState(m_state | DeviceState::Reading);

    auto *watcher = new QFutureWatcher<ProfileGenericTaskResult>(this);
    QObject::connect(watcher, &QFutureWatcher<ProfileGenericTaskResult>::finished, this,
                     [this, watcher]() {
                         const ProfileGenericTaskResult taskResult = watcher->result();
                         handleProfileGenericFinished(taskResult.object, taskResult.result);
                         watcher->deleteLater();
                     });

    watcher->setFuture(QtConcurrent::run([this, object, start, end]() {
        ProfileGenericTaskResult taskResult;
        taskResult.object = object;
        taskResult.result.errorCode =
            m_communicator->readProfileGenericByRange(object, start, end, taskResult.result);
        if (taskResult.result.errorCode != 0) {
            taskResult.result.errorMessage =
                tr("Profile Generic read failed: %1").arg(taskResult.result.errorCode);
        }
        return taskResult;
    }));
}

void GXDLMSDevice::handleProfileGenericFinished(CGXDLMSObject *object, const ProfileGenericResult &result)
{
    setState((m_state & ~DeviceStates(DeviceState::Reading)) | DeviceState::Connected);
    if (result.errorCode == 0)
        emit profileGenericRead(object, result);
    else
        emit errorOccurred(result.errorMessage);
}

void GXDLMSDevice::handleReadAllFinished()
{
    const bool cancelled = m_cancelRequested.exchange(false);
    setState((m_state & ~DeviceStates(DeviceState::Reading)) | DeviceState::Connected);
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
    if (enabled && (m_state & DeviceState::Connected)) {
        m_notificationBuffer.clear();
        m_notificationTimer->start();
    } else {
        m_notificationTimer->stop();
        m_notificationBuffer.clear();
    }
}

void GXDLMSDevice::onNotificationPoll()
{
    if (!m_notificationsEnabled || !(m_state & DeviceState::Connected))
        return;
    if (m_state & (DeviceState::Reading | DeviceState::Writing | DeviceState::Connecting
                   | DeviceState::Disconnecting))
        return;

    MediaConnection *media = m_communicator->media();
    if (!media || !media->isOpen())
        return;

    QByteArray chunk;
    if (media->readAvailable(chunk, 100) != 0 || chunk.isEmpty())
        return;

    m_notificationBuffer.append(chunk);
    const unsigned char eop = m_communicator->client()->GetInterfaceType() == DLMS_INTERFACE_TYPE_HDLC
                                  ? 0x7E
                                  : 0x00;
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
