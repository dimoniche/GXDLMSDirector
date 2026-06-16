#include "ManufacturerSettings.h"
#include "GXDLMSDevice.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

ManufacturerSettings &ManufacturerSettings::instance()
{
    static ManufacturerSettings settings;
    return settings;
}

ManufacturerSettings::ManufacturerSettings()
{
    loadDefaults();
    load();
}

QString ManufacturerSettings::filePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/GXDLMSDirector");
    return dir + QStringLiteral("/Manufacturers.xml");
}

void ManufacturerSettings::loadDefaults()
{
    m_profiles = {
        {QStringLiteral("GRX"), QStringLiteral("Gurux Generic"), true, 16, 1, 0, 0},
        {QStringLiteral("ABB"), QStringLiteral("ABB"), true, 16, 1, 0, 0},
        {QStringLiteral("KAM"), QStringLiteral("Kamstrup"), true, 16, 1, 0, 0},
        {QStringLiteral("LGZ"), QStringLiteral("Landis+Gyr"), true, 16, 1, 0, 0},
        {QStringLiteral("EMH"), QStringLiteral("EMH"), true, 16, 1, 0, 0},
        {QStringLiteral("ELS"), QStringLiteral("Elster"), true, 16, 1, 0, 0},
        {QStringLiteral("ITR"), QStringLiteral("Itron"), true, 16, 1, 0, 0},
    };
}

bool ManufacturerSettings::load()
{
    QFile file(filePath());
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDomDocument doc;
    if (!doc.setContent(&file))
        return false;

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("Manufacturers"))
        return false;

    QVector<ManufacturerProfile> loaded;
    for (QDomElement item = root.firstChildElement(QStringLiteral("Manufacturer"));
         !item.isNull();
         item = item.nextSiblingElement(QStringLiteral("Manufacturer"))) {
        ManufacturerProfile profile;
        profile.id = item.attribute(QStringLiteral("id"));
        profile.name = item.attribute(QStringLiteral("name"), profile.id);
        const QString useLogicalName = item.attribute(QStringLiteral("useLogicalName"), QStringLiteral("true")).trimmed().toLower();
        profile.useLogicalName = useLogicalName == QStringLiteral("true") || useLogicalName == QStringLiteral("1")
                                 || useLogicalName == QStringLiteral("yes");
        profile.clientAddress = item.attribute(QStringLiteral("clientAddress"), QStringLiteral("16")).toInt();
        profile.serverAddress = item.attribute(QStringLiteral("serverAddress"), QStringLiteral("1")).toInt();
        profile.authentication = item.attribute(QStringLiteral("authentication"), QStringLiteral("0")).toInt();
        profile.interfaceType = item.attribute(QStringLiteral("interfaceType"), QStringLiteral("0")).toInt();
        if (!profile.id.isEmpty())
            loaded.append(profile);
    }

    if (!loaded.isEmpty())
        m_profiles = loaded;
    return true;
}

bool ManufacturerSettings::save() const
{
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QDomDocument doc;
    QDomElement root = doc.createElement(QStringLiteral("Manufacturers"));
    doc.appendChild(root);

    for (const ManufacturerProfile &profile : m_profiles) {
        QDomElement item = doc.createElement(QStringLiteral("Manufacturer"));
        item.setAttribute(QStringLiteral("id"), profile.id);
        item.setAttribute(QStringLiteral("name"), profile.name);
        item.setAttribute(QStringLiteral("useLogicalName"), profile.useLogicalName ? QStringLiteral("true") : QStringLiteral("false"));
        item.setAttribute(QStringLiteral("clientAddress"), profile.clientAddress);
        item.setAttribute(QStringLiteral("serverAddress"), profile.serverAddress);
        item.setAttribute(QStringLiteral("authentication"), profile.authentication);
        item.setAttribute(QStringLiteral("interfaceType"), profile.interfaceType);
        root.appendChild(item);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(doc.toByteArray(2));
    return true;
}

ManufacturerProfile ManufacturerSettings::profile(const QString &id) const
{
    for (const ManufacturerProfile &item : m_profiles) {
        if (item.id.compare(id, Qt::CaseInsensitive) == 0)
            return item;
    }
    return m_profiles.first();
}

void ManufacturerSettings::applyToDevice(const ManufacturerProfile &profile, GXDLMSDevice *device) const
{
    if (!device)
        return;

    device->setManufacturer(profile.id);
    device->setUseLogicalNameReferencing(profile.useLogicalName);
    device->setClientAddress(static_cast<unsigned char>(profile.clientAddress));
    device->setServerAddress(static_cast<unsigned long>(profile.serverAddress));
    device->setAuthentication(profile.authentication);
    device->setInterfaceType(profile.interfaceType);
}
