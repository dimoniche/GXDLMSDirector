#include "ProjectSerializer.h"
#include "GXDLMSDevice.h"
#include "GXDLMSProject.h"

#include <GXDLMSObjectCollection.h>

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString boolAttribute(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

bool parseBoolAttribute(const QDomElement &element, const QString &name, bool defaultValue)
{
    const QString value = element.attribute(name).trimmed().toLower();
    if (value.isEmpty())
        return defaultValue;
    if (value == QStringLiteral("true") || value == QStringLiteral("1") || value == QStringLiteral("yes"))
        return true;
    if (value == QStringLiteral("false") || value == QStringLiteral("0") || value == QStringLiteral("no"))
        return false;
    return defaultValue;
}

QString sanitizeDeviceKey(const QString &name, int index)
{
    QString key = name;
    key.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")), QStringLiteral("_"));
    if (key.isEmpty())
        key = QStringLiteral("device%1").arg(index);
    return key;
}

void writeDeviceElement(QDomDocument &doc, QDomElement &deviceElement, GXDLMSDevice *device)
{
    deviceElement.setAttribute(QStringLiteral("name"), device->name());
    deviceElement.setAttribute(QStringLiteral("manufacturer"), device->manufacturer());

    QDomElement media = doc.createElement(QStringLiteral("Media"));
    media.setAttribute(QStringLiteral("type"), device->mediaType() == MediaType::Serial ? QStringLiteral("serial") : QStringLiteral("network"));
    media.setAttribute(QStringLiteral("serialPort"), device->serialPort());
    media.setAttribute(QStringLiteral("baudRate"), device->baudRate());
    media.setAttribute(QStringLiteral("dataBits"), static_cast<int>(device->dataBits()));
    media.setAttribute(QStringLiteral("parity"), static_cast<int>(device->parity()));
    media.setAttribute(QStringLiteral("stopBits"), static_cast<int>(device->stopBits()));
    media.setAttribute(QStringLiteral("host"), device->hostName());
    media.setAttribute(QStringLiteral("port"), device->port());
    media.setAttribute(QStringLiteral("waitTimeMs"), device->waitTimeMs());
    deviceElement.appendChild(media);

    QDomElement dlms = doc.createElement(QStringLiteral("Dlms"));
    dlms.setAttribute(QStringLiteral("useLogicalName"),
                      boolAttribute(device->useLogicalNameReferencing()));
    dlms.setAttribute(QStringLiteral("clientAddress"), device->clientAddress());
    dlms.setAttribute(QStringLiteral("serverAddress"), static_cast<qulonglong>(device->serverAddress()));
    dlms.setAttribute(QStringLiteral("authentication"), device->authentication());
    dlms.setAttribute(QStringLiteral("password"), device->password());
    dlms.setAttribute(QStringLiteral("interfaceType"), device->interfaceType());
    dlms.setAttribute(QStringLiteral("standard"), device->standard());
    dlms.setAttribute(QStringLiteral("security"), device->security());
    dlms.setAttribute(QStringLiteral("authenticationKey"), device->authenticationKey());
    dlms.setAttribute(QStringLiteral("blockCipherKey"), device->blockCipherKey());
    dlms.setAttribute(QStringLiteral("macSourceAddress"), device->macSourceAddress());
    dlms.setAttribute(QStringLiteral("macDestinationAddress"), device->macDestinationAddress());
    deviceElement.appendChild(dlms);
}

bool readDeviceElement(const QDomElement &deviceElement, GXDLMSDevice *device, QString *error)
{
    if (deviceElement.isNull()) {
        if (error)
            *error = QStringLiteral("Missing device element");
        return false;
    }

    device->setName(deviceElement.attribute(QStringLiteral("name"), QStringLiteral("Meter 1")));
    device->setManufacturer(deviceElement.attribute(QStringLiteral("manufacturer"), QStringLiteral("GRX")));

    const QDomElement media = deviceElement.firstChildElement(QStringLiteral("Media"));
    if (!media.isNull()) {
        device->setMediaType(media.attribute(QStringLiteral("type")) == QStringLiteral("network")
                                 ? MediaType::Network
                                 : MediaType::Serial);
        device->setSerialPort(media.attribute(QStringLiteral("serialPort"), device->serialPort()));
        device->setBaudRate(media.attribute(QStringLiteral("baudRate"), QStringLiteral("9600")).toInt());
        device->setDataBits(static_cast<QSerialPort::DataBits>(media.attribute(QStringLiteral("dataBits"), QStringLiteral("8")).toInt()));
        device->setParity(static_cast<QSerialPort::Parity>(media.attribute(QStringLiteral("parity"), QStringLiteral("0")).toInt()));
        device->setStopBits(static_cast<QSerialPort::StopBits>(media.attribute(QStringLiteral("stopBits"), QStringLiteral("1")).toInt()));
        device->setHostName(media.attribute(QStringLiteral("host"), device->hostName()));
        device->setPort(static_cast<quint16>(media.attribute(QStringLiteral("port"), QStringLiteral("4059")).toUInt()));
        device->setWaitTimeMs(media.attribute(QStringLiteral("waitTimeMs"), QStringLiteral("5000")).toInt());
    }

    const QDomElement dlms = deviceElement.firstChildElement(QStringLiteral("Dlms"));
    if (!dlms.isNull()) {
        device->setUseLogicalNameReferencing(
            parseBoolAttribute(dlms, QStringLiteral("useLogicalName"), true));
        device->setClientAddress(static_cast<unsigned char>(dlms.attribute(QStringLiteral("clientAddress"), QStringLiteral("16")).toUInt()));
        device->setServerAddress(dlms.attribute(QStringLiteral("serverAddress"), QStringLiteral("1")).toULong());
        device->setAuthentication(dlms.attribute(QStringLiteral("authentication"), QStringLiteral("0")).toInt());
        device->setPassword(dlms.attribute(QStringLiteral("password")));
        device->setInterfaceType(dlms.attribute(QStringLiteral("interfaceType"), QStringLiteral("0")).toInt());
        device->setStandard(dlms.attribute(QStringLiteral("standard"), QStringLiteral("0")).toInt());
        device->setSecurity(dlms.attribute(QStringLiteral("security"), QStringLiteral("0")).toInt());
        device->setAuthenticationKey(dlms.attribute(QStringLiteral("authenticationKey")));
        device->setBlockCipherKey(dlms.attribute(QStringLiteral("blockCipherKey")));
        device->setMacSourceAddress(static_cast<quint16>(
            dlms.attribute(QStringLiteral("macSourceAddress"), QStringLiteral("0")).toUInt()));
        device->setMacDestinationAddress(static_cast<quint16>(
            dlms.attribute(QStringLiteral("macDestinationAddress"), QStringLiteral("0")).toUInt()));
    }

    return true;
}

bool loadDeviceObjects(const QString &projectPath, GXDLMSDevice *device, int deviceIndex, int deviceCount,
                       QString *error)
{
    device->objects().Free();

    QString objectsPath = ProjectSerializer::objectsFilePath(projectPath, device, deviceIndex);
    if (!QFile::exists(objectsPath) && deviceCount == 1)
        objectsPath = ProjectSerializer::objectsFilePath(projectPath);

    if (!QFile::exists(objectsPath))
        return true;

    const int ret = device->objects().Load(objectsPath.toStdString().c_str());
    if (ret != 0) {
        if (error)
            *error = QStringLiteral("Failed to load objects for %1 (%2)").arg(device->name()).arg(ret);
        return false;
    }
    return true;
}

bool saveDeviceObjects(const QString &projectPath, GXDLMSDevice *device, int deviceIndex, int deviceCount,
                       QString *error)
{
    if (device->objects().empty()) {
        QFile::remove(ProjectSerializer::objectsFilePath(projectPath, device, deviceIndex));
        if (deviceCount == 1)
            QFile::remove(ProjectSerializer::objectsFilePath(projectPath));
        return true;
    }

    const QString objectsPath = deviceCount == 1
                                    ? ProjectSerializer::objectsFilePath(projectPath)
                                    : ProjectSerializer::objectsFilePath(projectPath, device, deviceIndex);
    const int ret = device->objects().Save(objectsPath.toStdString().c_str());
    if (ret != 0) {
        if (error)
            *error = QStringLiteral("Failed to save objects for %1 (%2)").arg(device->name()).arg(ret);
        return false;
    }
    return true;
}

} // namespace

QString ProjectSerializer::objectsFilePath(const QString &projectPath)
{
    return QFileInfo(projectPath).absolutePath() + QLatin1Char('/')
           + QFileInfo(projectPath).completeBaseName() + QStringLiteral(".objects.xml");
}

QString ProjectSerializer::objectsFilePath(const QString &projectPath, const GXDLMSDevice *device, int deviceIndex)
{
    const QString key = sanitizeDeviceKey(device ? device->name() : QString(), deviceIndex);
    return QFileInfo(projectPath).absolutePath() + QLatin1Char('/')
           + QFileInfo(projectPath).completeBaseName() + QLatin1Char('.') + key
           + QStringLiteral(".objects.xml");
}

bool ProjectSerializer::save(const QString &projectPath, GXDLMSProject *project, QString *error)
{
    if (!project || project->deviceCount() == 0) {
        if (error)
            *error = QStringLiteral("No devices in project");
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement(QStringLiteral("GXDLMSDirectorProject"));
    root.setAttribute(QStringLiteral("version"), project->deviceCount() > 1 ? QStringLiteral("3") : QStringLiteral("2"));
    doc.appendChild(root);

    for (int i = 0; i < project->deviceCount(); ++i) {
        GXDLMSDevice *device = project->deviceAt(i);
        QDomElement deviceElement = doc.createElement(QStringLiteral("Device"));
        writeDeviceElement(doc, deviceElement, device);
        root.appendChild(deviceElement);
    }

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(doc.toByteArray(2));
    file.close();

    for (int i = 0; i < project->deviceCount(); ++i) {
        if (!saveDeviceObjects(projectPath, project->deviceAt(i), i, project->deviceCount(), error))
            return false;
    }

    return true;
}

bool ProjectSerializer::load(const QString &projectPath, GXDLMSProject *project, QString *error)
{
    if (!project) {
        if (error)
            *error = QStringLiteral("No project");
        return false;
    }

    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        if (error)
            *error = QStringLiteral("Invalid project XML");
        return false;
    }

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("GXDLMSDirectorProject")
        && root.tagName() != QStringLiteral("ArrayOfGXDLMSDevice")) {
        if (error)
            *error = QStringLiteral("Unsupported project format");
        return false;
    }

    QVector<QDomElement> deviceElements;
    for (QDomElement element = root.firstChildElement(QStringLiteral("Device")); !element.isNull();
         element = element.nextSiblingElement(QStringLiteral("Device"))) {
        deviceElements.append(element);
    }
    if (deviceElements.isEmpty()) {
        QDomElement fallback = root.firstChildElement();
        if (!fallback.isNull())
            deviceElements.append(fallback);
    }
    if (deviceElements.isEmpty()) {
        if (error)
            *error = QStringLiteral("Project contains no devices");
        return false;
    }

    project->clearDevices();

    for (int i = 0; i < deviceElements.size(); ++i) {
        const QDomElement deviceElement = deviceElements.at(i);
        const QString name = deviceElement.attribute(QStringLiteral("name"),
                                                     QStringLiteral("Meter %1").arg(i + 1));
        GXDLMSDevice *device = project->addDevice(name);
        if (!readDeviceElement(deviceElement, device, error))
            return false;
        device->applyConnectionSettings();
        if (!loadDeviceObjects(projectPath, device, i, deviceElements.size(), error))
            return false;
    }

    project->setCurrentDeviceIndex(0);
    project->setPath(projectPath);
    project->setDirty(false);
    return true;
}

bool ProjectSerializer::save(const QString &projectPath, GXDLMSDevice *device, QString *error)
{
    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *copy = project.addDevice(device->name());
    DeviceCloner::copySettings(*device, *copy);
    DeviceCloner::copyObjects(*device, *copy, error);
    return save(projectPath, &project, error);
}

bool ProjectSerializer::load(const QString &projectPath, GXDLMSDevice *device, QString *error)
{
    GXDLMSProject project;
    if (!load(projectPath, &project, error))
        return false;
    DeviceCloner::copySettings(*project.deviceAt(0), *device);
    return DeviceCloner::copyObjects(*project.deviceAt(0), *device, error);
}
