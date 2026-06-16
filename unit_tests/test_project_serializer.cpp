#include "core/ProjectSerializer.h"
#include "core/GXDLMSProject.h"
#include "core/GXDLMSDevice.h"

#include <enums.h>

#include <GXDLMSData.h>
#include <GXDLMSObjectCollection.h>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestProjectSerializer : public QObject
{
    Q_OBJECT

private slots:
    void roundTripBoolReferencing();
    void roundTripSecurityKeys();
    void roundTripMultiDevice();
    void roundTripNetworkMedia();
    void roundTripBoolAttributeVariants();
    void roundTripObjectsSidecar();
    void rejectsUnsupportedFormat();
    void objectsFilePathForMultiDevice();
};

void TestProjectSerializer::roundTripMultiDevice()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/multi.gxc");

    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *first = project.addDevice(QStringLiteral("Meter A"));
    first->setClientAddress(16);
    project.addDevice(QStringLiteral("Meter B"));
    project.deviceAt(1)->setClientAddress(32);

    QString error;
    QVERIFY2(ProjectSerializer::save(projectPath, &project, &error), qPrintable(error));

    GXDLMSProject loaded;
    QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.deviceCount(), 2);
    QCOMPARE(loaded.deviceAt(0)->name(), QStringLiteral("Meter A"));
    QCOMPARE(loaded.deviceAt(1)->name(), QStringLiteral("Meter B"));
    QCOMPARE(loaded.deviceAt(1)->clientAddress(), static_cast<unsigned char>(32));
}

void TestProjectSerializer::roundTripNetworkMedia()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/network.gxc");

    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *device = project.addDevice(QStringLiteral("TCP meter"));
    device->setMediaType(MediaType::Network);
    device->setHostName(QStringLiteral("192.168.1.50"));
    device->setPort(4059);
    device->setWaitTimeMs(8000);

    QString error;
    QVERIFY2(ProjectSerializer::save(projectPath, &project, &error), qPrintable(error));

    GXDLMSProject loaded;
    QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.deviceAt(0)->mediaType(), MediaType::Network);
    QCOMPARE(loaded.deviceAt(0)->hostName(), QStringLiteral("192.168.1.50"));
    QCOMPARE(loaded.deviceAt(0)->port(), static_cast<quint16>(4059));
    QCOMPARE(loaded.deviceAt(0)->waitTimeMs(), 8000);
}

void TestProjectSerializer::roundTripBoolAttributeVariants()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto verifyBool = [&](bool value, const QString &attributeValue) {
        const QString projectPath = tempDir.path()
                                    + QStringLiteral("/bool_%1.gxc").arg(attributeValue);
        QFile file(projectPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QStringLiteral(
            R"(<?xml version="1.0" encoding="UTF-8"?>
<GXDLMSDirectorProject version="2">
 <Device name="Bool test">
  <Media type="serial" serialPort="/dev/ttyUSB0" baudRate="9600" dataBits="8" parity="0" stopBits="1" host="localhost" port="4059" waitTimeMs="5000"/>
  <Dlms useLogicalName="%1" clientAddress="16" serverAddress="1" authentication="0" password="" interfaceType="0" standard="0" security="0" authenticationKey="" blockCipherKey="" macSourceAddress="0" macDestinationAddress="0"/>
 </Device>
</GXDLMSDirectorProject>)")
                       .arg(attributeValue)
                       .toUtf8());
        file.close();

        GXDLMSProject loaded;
        QString error;
        QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.deviceAt(0)->useLogicalNameReferencing(), value);
    };

    verifyBool(true, QStringLiteral("true"));
    verifyBool(true, QStringLiteral("1"));
    verifyBool(false, QStringLiteral("false"));
    verifyBool(false, QStringLiteral("0"));
}

void TestProjectSerializer::roundTripObjectsSidecar()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/objects.gxc");

    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *device = project.addDevice(QStringLiteral("With objects"));
    device->objects().push_back(new CGXDLMSData(std::string("0.0.0.1.0.255")));

    QString error;
    QVERIFY2(ProjectSerializer::save(projectPath, &project, &error), qPrintable(error));
    QVERIFY(QFile::exists(ProjectSerializer::objectsFilePath(projectPath)));

    GXDLMSProject loaded;
    QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
    QCOMPARE(static_cast<int>(loaded.deviceAt(0)->objects().size()), 1);
}

void TestProjectSerializer::rejectsUnsupportedFormat()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/legacy.gxc");
    QFile file(projectPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QStringLiteral(
        R"(<?xml version="1.0" encoding="UTF-8"?>
<ArrayOfGXDLMSDevice xmlns="Gurux1">
 <GXDLMSDevice>
  <Name>Legacy</Name>
 </GXDLMSDevice>
</ArrayOfGXDLMSDevice>)")
                   .toUtf8());
    file.close();

    GXDLMSProject loaded;
    QString error;
    QVERIFY(!ProjectSerializer::load(projectPath, &loaded, &error));
    QVERIFY(!error.isEmpty());
}

void TestProjectSerializer::objectsFilePathForMultiDevice()
{
    const QString projectPath = QStringLiteral("/tmp/demo/project.gxc");
    GXDLMSDevice device;
    device.setName(QStringLiteral("Meter #1"));

    const QString sidecar = ProjectSerializer::objectsFilePath(projectPath, &device, 1);
    QVERIFY(sidecar.endsWith(QStringLiteral(".Meter__1.objects.xml")));
    QVERIFY(!sidecar.contains(QLatin1Char('#')));
}

void TestProjectSerializer::roundTripBoolReferencing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/test.gxc");

    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *device = project.addDevice(QStringLiteral("Meter SN"));
    device->setUseLogicalNameReferencing(false);
    device->setManufacturer(QStringLiteral("GRX"));
    device->setClientAddress(32);
    device->setServerAddress(17);

    QString error;
    QVERIFY2(ProjectSerializer::save(projectPath, &project, &error), qPrintable(error));

    GXDLMSProject loaded;
    QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.deviceCount(), 1);
    QCOMPARE(loaded.deviceAt(0)->useLogicalNameReferencing(), false);
    QCOMPARE(loaded.deviceAt(0)->clientAddress(), static_cast<unsigned char>(32));
    QCOMPARE(loaded.deviceAt(0)->serverAddress(), static_cast<unsigned long>(17));
}

void TestProjectSerializer::roundTripSecurityKeys()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectPath = tempDir.path() + QStringLiteral("/secure.gxc");

    GXDLMSProject project;
    project.clearDevices();
    GXDLMSDevice *device = project.addDevice(QStringLiteral("Secure meter"));
    device->setSecurity(DLMS_SECURITY_AUTHENTICATION_ENCRYPTION);
    device->setAuthenticationKey(QStringLiteral("00112233445566778899AABBCCDDEEFF"));
    device->setBlockCipherKey(QStringLiteral("FFEEDDCCBBAA99887766554433221100"));

    QString error;
    QVERIFY2(ProjectSerializer::save(projectPath, &project, &error), qPrintable(error));

    GXDLMSProject loaded;
    QVERIFY2(ProjectSerializer::load(projectPath, &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.deviceAt(0)->security(), DLMS_SECURITY_AUTHENTICATION_ENCRYPTION);
    QCOMPARE(loaded.deviceAt(0)->authenticationKey(),
             QStringLiteral("00112233445566778899AABBCCDDEEFF"));
    QCOMPARE(loaded.deviceAt(0)->blockCipherKey(),
             QStringLiteral("FFEEDDCCBBAA99887766554433221100"));
}

QTEST_MAIN(TestProjectSerializer)
#include "test_project_serializer.moc"
