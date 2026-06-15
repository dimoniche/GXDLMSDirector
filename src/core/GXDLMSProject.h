#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class GXDLMSDevice;

class GXDLMSProject : public QObject
{
    Q_OBJECT

public:
    explicit GXDLMSProject(QObject *parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path) { m_path = path; }

    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty);

    int deviceCount() const { return static_cast<int>(m_devices.size()); }
    int currentDeviceIndex() const { return m_currentDeviceIndex; }
    GXDLMSDevice *currentDevice();
    const GXDLMSDevice *currentDevice() const;
    GXDLMSDevice *deviceAt(int index);
    const GXDLMSDevice *deviceAt(int index) const;

    void setCurrentDeviceIndex(int index);
    GXDLMSDevice *addDevice(const QString &name = {});
    GXDLMSDevice *cloneDeviceAt(int index);
    bool removeDeviceAt(int index);
    void resetToSingleDevice();
    void clearDevices();

signals:
    void dirtyChanged(bool dirty);
    void currentDeviceChanged(GXDLMSDevice *device);

private:
    QString m_path;
    bool m_dirty = false;
    int m_currentDeviceIndex = 0;
    std::vector<std::unique_ptr<GXDLMSDevice>> m_devices;

    QString nextDeviceName() const;
};

namespace DeviceCloner {
bool copySettings(const GXDLMSDevice &from, GXDLMSDevice &to);
bool copyObjects(GXDLMSDevice &from, GXDLMSDevice &to, QString *error = nullptr);
}
