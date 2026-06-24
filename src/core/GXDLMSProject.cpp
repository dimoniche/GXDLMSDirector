#include "GXDLMSProject.h"
#include "GXDLMSDevice.h"

#include <GXDLMSObjectCollection.h>

#include <QTemporaryFile>

GXDLMSProject::GXDLMSProject(QObject *parent)
    : QObject(parent)
{
    addDevice(QStringLiteral("Meter 1"));
    setDirty(false);
}

void GXDLMSProject::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged(dirty);
}

GXDLMSDevice *GXDLMSProject::currentDevice()
{
    if (m_devices.empty())
        return nullptr;
    if (m_currentDeviceIndex < 0 || m_currentDeviceIndex >= m_devices.size())
        m_currentDeviceIndex = 0;
    return m_devices.at(m_currentDeviceIndex).get();
}

const GXDLMSDevice *GXDLMSProject::currentDevice() const
{
    return const_cast<GXDLMSProject *>(this)->currentDevice();
}

GXDLMSDevice *GXDLMSProject::deviceAt(int index)
{
    if (index < 0 || index >= m_devices.size())
        return nullptr;
    return m_devices.at(index).get();
}

const GXDLMSDevice *GXDLMSProject::deviceAt(int index) const
{
    if (index < 0 || index >= m_devices.size())
        return nullptr;
    return m_devices.at(index).get();
}

void GXDLMSProject::setCurrentDeviceIndex(int index)
{
    if (index < 0 || index >= m_devices.size() || index == m_currentDeviceIndex)
        return;
    m_currentDeviceIndex = index;
    emit currentDeviceChanged(currentDevice());
}

QString GXDLMSProject::nextDeviceName() const
{
    return QStringLiteral("Meter %1").arg(m_devices.size() + 1);
}

GXDLMSDevice *GXDLMSProject::addDevice(const QString &name)
{
    auto device = std::make_unique<GXDLMSDevice>(this);
    device->setName(name.isEmpty() ? nextDeviceName() : name);
    device->setManufacturer(QStringLiteral("GRX"));
    GXDLMSDevice *raw = device.get();
    m_devices.push_back(std::move(device));
    m_currentDeviceIndex = m_devices.size() - 1;
    setDirty(true);
    emit currentDeviceChanged(raw);
    return raw;
}

GXDLMSDevice *GXDLMSProject::cloneDeviceAt(int index)
{
    GXDLMSDevice *source = deviceAt(index);
    if (!source)
        return nullptr;

    auto device = std::make_unique<GXDLMSDevice>(this);
    DeviceCloner::copySettings(*source, *device);
    device->setName(nextDeviceName());
    DeviceCloner::copyObjects(*source, *device, nullptr);

    GXDLMSDevice *raw = device.get();
    m_devices.push_back(std::move(device));
    m_currentDeviceIndex = m_devices.size() - 1;
    setDirty(true);
    emit currentDeviceChanged(raw);
    return raw;
}

bool GXDLMSProject::removeDeviceAt(int index)
{
    if (m_devices.size() <= 1 || index < 0 || index >= m_devices.size())
        return false;

    if (m_devices.at(index)->state() & DeviceState::Connected)
        m_devices.at(index)->disconnectAsync();

    m_devices.erase(m_devices.begin() + index);
    if (m_currentDeviceIndex >= m_devices.size())
        m_currentDeviceIndex = m_devices.size() - 1;
    setDirty(true);
    emit currentDeviceChanged(currentDevice());
    return true;
}

void GXDLMSProject::clearDevices()
{
    for (auto &device : m_devices) {
        if (device && (device->state() & DeviceState::Connected))
            device->disconnectAsync();
    }
    m_devices.clear();
    m_currentDeviceIndex = 0;
}

void GXDLMSProject::resetToSingleDevice()
{
    clearDevices();
    m_path.clear();
    addDevice(QStringLiteral("Meter 1"));
    setDirty(false);
}

namespace DeviceCloner {

bool copySettings(const GXDLMSDevice &from, GXDLMSDevice &to)
{
    to.setName(from.name());
    to.setManufacturer(from.manufacturer());
    to.setMediaType(from.mediaType());
    to.setSerialPort(from.serialPort());
    to.setBaudRate(from.baudRate());
    to.setDataBits(from.dataBits());
    to.setParity(from.parity());
    to.setStopBits(from.stopBits());
    to.setHostName(from.hostName());
    to.setPort(from.port());
    to.setWaitTimeMs(from.waitTimeMs());
    to.setUseLogicalNameReferencing(from.useLogicalNameReferencing());
    to.setClientAddress(from.clientAddress());
    to.setServerLogicalAddress(from.serverLogicalAddress());
    to.setServerPhysicalAddress(from.serverPhysicalAddress());
    to.setAuthentication(from.authentication());
    to.setPassword(from.password());
    to.setInterfaceType(from.interfaceType());
    to.setStandard(from.standard());
    to.setSecurity(from.security());
    to.setAuthenticationKey(from.authenticationKey());
    to.setBlockCipherKey(from.blockCipherKey());
    to.setMacSourceAddress(from.macSourceAddress());
    to.setMacDestinationAddress(from.macDestinationAddress());
    to.applyConnectionSettings();
    return true;
}

bool copyObjects(GXDLMSDevice &from, GXDLMSDevice &to, QString *error)
{
    to.objects().Free();
    if (from.objects().empty())
        return true;

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        if (error)
            *error = tempFile.errorString();
        return false;
    }
    tempFile.close();

    const int saveRet = from.objects().Save(tempFile.fileName().toStdString().c_str());
    if (saveRet != 0) {
        if (error)
            *error = QStringLiteral("Failed to clone objects (%1)").arg(saveRet);
        return false;
    }

    const int loadRet = to.objects().Load(tempFile.fileName().toStdString().c_str());
    if (loadRet != 0) {
        if (error)
            *error = QStringLiteral("Failed to load cloned objects (%1)").arg(loadRet);
        return false;
    }
    return true;
}

} // namespace DeviceCloner
