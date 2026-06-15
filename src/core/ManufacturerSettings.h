#pragma once

#include <QString>
#include <QVector>

struct ManufacturerProfile {
    QString id;
    QString name;
    bool useLogicalName = true;
    int clientAddress = 16;
    int serverAddress = 1;
    int authentication = 0;
    int interfaceType = 0;
};

class ManufacturerSettings
{
public:
    static ManufacturerSettings &instance();

    bool load();
    bool save() const;

    const QVector<ManufacturerProfile> &profiles() const { return m_profiles; }
    ManufacturerProfile profile(const QString &id) const;
    QString filePath() const;

    void applyToDevice(const ManufacturerProfile &profile, class GXDLMSDevice *device) const;

private:
    ManufacturerSettings();

    void loadDefaults();

    QVector<ManufacturerProfile> m_profiles;
};
