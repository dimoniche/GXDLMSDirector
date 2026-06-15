#pragma once

#include <QObject>
#include <QSerialPort>
#include <QString>
#include <memory>

#include "DeviceState.h"
#include "MediaConnection.h"
#include "ProfileGenericResult.h"
#include "ReadResult.h"

#include <QDateTime>

class CGXDLMSObject;
class CGXDLMSObjectCollection;
class GXDLMSCommunicator;

Q_DECLARE_OPAQUE_POINTER(CGXDLMSObject *)

class GXDLMSDevice : public QObject
{
    Q_OBJECT

public:
    explicit GXDLMSDevice(QObject *parent = nullptr);
    ~GXDLMSDevice() override;

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    DeviceStates state() const { return m_state; }
    void setState(DeviceStates state);

    MediaType mediaType() const { return m_mediaType; }
    void setMediaType(MediaType type) { m_mediaType = type; }

    QString serialPort() const { return m_serialPort; }
    void setSerialPort(const QString &port) { m_serialPort = port; }

    int baudRate() const { return m_baudRate; }
    void setBaudRate(int rate) { m_baudRate = rate; }

    QSerialPort::DataBits dataBits() const { return m_dataBits; }
    void setDataBits(QSerialPort::DataBits bits) { m_dataBits = bits; }

    QSerialPort::Parity parity() const { return m_parity; }
    void setParity(QSerialPort::Parity parity) { m_parity = parity; }

    QSerialPort::StopBits stopBits() const { return m_stopBits; }
    void setStopBits(QSerialPort::StopBits bits) { m_stopBits = bits; }

    QString hostName() const { return m_hostName; }
    void setHostName(const QString &host) { m_hostName = host; }

    quint16 port() const { return m_port; }
    void setPort(quint16 port) { m_port = port; }

    bool useLogicalNameReferencing() const { return m_useLogicalName; }
    void setUseLogicalNameReferencing(bool value) { m_useLogicalName = value; }

    unsigned char clientAddress() const { return m_clientAddress; }
    void setClientAddress(unsigned char address) { m_clientAddress = address; }

    unsigned long serverAddress() const { return m_serverAddress; }
    void setServerAddress(unsigned long address) { m_serverAddress = address; }

    int authentication() const { return m_authentication; }
    void setAuthentication(int auth) { m_authentication = auth; }

    QString password() const { return m_password; }
    void setPassword(const QString &password) { m_password = password; }

    int interfaceType() const { return m_interfaceType; }
    void setInterfaceType(int type) { m_interfaceType = type; }

    int standard() const { return m_standard; }
    void setStandard(int standard) { m_standard = standard; }

    int security() const { return m_security; }
    void setSecurity(int security) { m_security = security; }

    QString authenticationKey() const { return m_authenticationKey; }
    void setAuthenticationKey(const QString &key) { m_authenticationKey = key; }

    QString blockCipherKey() const { return m_blockCipherKey; }
    void setBlockCipherKey(const QString &key) { m_blockCipherKey = key; }

    int waitTimeMs() const { return m_waitTimeMs; }
    void setWaitTimeMs(int ms) { m_waitTimeMs = ms; }

    QString manufacturer() const { return m_manufacturer; }
    void setManufacturer(const QString &manufacturer) { m_manufacturer = manufacturer; }

    quint16 macSourceAddress() const { return m_macSourceAddress; }
    void setMacSourceAddress(quint16 address) { m_macSourceAddress = address; }

    quint16 macDestinationAddress() const { return m_macDestinationAddress; }
    void setMacDestinationAddress(quint16 address) { m_macDestinationAddress = address; }

    QString proposedConformance() const { return m_proposedConformance; }
    QString negotiatedConformance() const { return m_negotiatedConformance; }
    void setConformanceInfo(const QString &proposed, const QString &negotiated);

    GXDLMSCommunicator *communicator();
    CGXDLMSObjectCollection &objects();

    void connectAsync();
    void disconnectAsync();
    void readAllAsync();
    void readObjectAsync(CGXDLMSObject *object, int attributeIndex);
    void writeObjectAsync(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void invokeMethodAsync(CGXDLMSObject *object, int methodIndex, const QString &parameter);
    void readProfileGenericByEntryAsync(CGXDLMSObject *object, int index, int count);
    void readProfileGenericByRangeAsync(CGXDLMSObject *object, const QDateTime &start, const QDateTime &end);

    void applyConnectionSettings();

private:
    void handleConnectionFinished(int ret);
    void handleDisconnectionFinished();
    void handleObjectReadFinished(CGXDLMSObject *object, int attributeIndex, const QString &value, int ret);
    void handleObjectWriteFinished(CGXDLMSObject *object, int attributeIndex, const QString &value, int ret);
    void handleMethodInvokeFinished(CGXDLMSObject *object, int methodIndex, int ret);
    void handleReadAllFinished();
    void handleProfileGenericFinished(CGXDLMSObject *object, const ProfileGenericResult &result);

signals:
    void stateChanged(DeviceStates state);
    void traceMessage(const QString &message);
    void progressChanged(const QString &description, int current, int maximum);
    void errorOccurred(const QString &message);
    void objectRead(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void objectWritten(CGXDLMSObject *object, int attributeIndex, const QString &value);
    void methodInvoked(CGXDLMSObject *object, int methodIndex);
    void profileGenericRead(CGXDLMSObject *object, const ProfileGenericResult &result);
    void readAllFinished();

private:
    QString m_name;
    DeviceStates m_state = DeviceState::None;

    MediaType m_mediaType = MediaType::Serial;
    QString m_serialPort = QStringLiteral("/dev/ttyUSB0");
    int m_baudRate = 9600;
    QSerialPort::DataBits m_dataBits = QSerialPort::Data8;
    QSerialPort::Parity m_parity = QSerialPort::NoParity;
    QSerialPort::StopBits m_stopBits = QSerialPort::OneStop;

    QString m_hostName = QStringLiteral("localhost");
    quint16 m_port = 4059;

    bool m_useLogicalName = true;
    unsigned char m_clientAddress = 16;
    unsigned long m_serverAddress = 1;
    int m_authentication = 0;
    QString m_password;
    int m_interfaceType = 0;
    int m_standard = 0;
    int m_security = 0;
    QString m_authenticationKey;
    QString m_blockCipherKey;
    int m_waitTimeMs = 5000;
    QString m_manufacturer = QStringLiteral("GRX");
    quint16 m_macSourceAddress = 0;
    quint16 m_macDestinationAddress = 0;
    QString m_proposedConformance;
    QString m_negotiatedConformance;

    std::unique_ptr<GXDLMSCommunicator> m_communicator;
};
