#include "ProjectSerializer.h"
#include "GXDLMSDevice.h"

#include <GXDLMSObjectCollection.h>

#include <QDomDocument>
#include <QFile>
#include <QFileInfo>

namespace {
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
    dlms.setAttribute(QStringLiteral("useLogicalName"), device->useLogicalNameReferencing());
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
        device->setUseLogicalNameReferencing(dlms.attribute(QStringLiteral("useLogicalName"), QStringLiteral("true")) == QStringLiteral("true"));
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
} // namespace

QString ProjectSerializer::objectsFilePath(const QString &projectPath)
{
    return QFileInfo(projectPath).absolutePath() + QLatin1Char('/')
           + QFileInfo(projectPath).completeBaseName() + QStringLiteral(".objects.xml");
}

bool ProjectSerializer::save(const QString &projectPath, GXDLMSDevice *device, QString *error)
{
    if (!device) {
        if (error)
            *error = QStringLiteral("No device");
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement(QStringLiteral("GXDLMSDirectorProject"));
    root.setAttribute(QStringLiteral("version"), QStringLiteral("2"));
    doc.appendChild(root);

    QDomElement deviceElement = doc.createElement(QStringLiteral("Device"));
    writeDeviceElement(doc, deviceElement, device);
    root.appendChild(deviceElement);

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(doc.toByteArray(2));
    file.close();

    if (!device->objects().empty()) {
        const int ret = device->objects().Save(objectsFilePath(projectPath).toStdString().c_str());
        if (ret != 0) {
            if (error)
                *error = QStringLiteral("Failed to save objects (%1)").arg(ret);
            return false;
        }
    } else {
        QFile::remove(objectsFilePath(projectPath));
    }

    return true;
}

bool ProjectSerializer::load(const QString &projectPath, GXDLMSDevice *device, QString *error)
{
    if (!device) {
        if (error)
            *error = QStringLiteral("No device");
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

    QDomElement deviceElement = root.firstChildElement(QStringLiteral("Device"));
    if (deviceElement.isNull())
        deviceElement = root.firstChildElement();

    if (!readDeviceElement(deviceElement, device, error))
        return false;

    device->applyConnectionSettings();
    device->objects().Free();

    const QString objectsPath = objectsFilePath(projectPath);
    if (QFile::exists(objectsPath)) {
        const int ret = device->objects().Load(objectsPath.toStdString().c_str());
        if (ret != 0) {
            if (error)
                *error = QStringLiteral("Failed to load objects (%1)").arg(ret);
            return false;
        }
    }

    return true;
}
