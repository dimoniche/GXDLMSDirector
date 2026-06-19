#pragma once

#include "DeviceState.h"
#include "MediaConnection.h"
#include "ProfileGenericResult.h"
#include "ReadResult.h"

#include <QDateTime>
#include <QTimer>

#include <GXDLMSSecureClient.h>
#include <GXDLMSObject.h>
#include <GXReplyData.h>

#include <QList>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

class GXDLMSDevice;

class GXDLMSCommunicator : public QObject
{
    Q_OBJECT

public:
    explicit GXDLMSCommunicator(GXDLMSDevice *device, QObject *parent = nullptr);
    ~GXDLMSCommunicator() override;

    CGXDLMSSecureClient *client() { return m_client.get(); }
    MediaConnection *media() { return m_media.get(); }

    int initializeConnection();
    int disconnect();
    int close();

    int read(CGXDLMSObject *object, int attributeIndex, QString &value);
    int write(CGXDLMSObject *object, int attributeIndex, const QString &value, QString *error = nullptr);
    int method(CGXDLMSObject *object, int methodIndex, CGXDLMSVariant &value);

    int readList(std::vector<std::pair<CGXDLMSObject *, unsigned char>> &list);
    int getAssociationView();
    QList<ReadResult> readAll(bool forceAll, std::atomic<bool> *cancelFlag);
    QList<ReadResult> readObjectAttributes(CGXDLMSObject *object, bool forceAll, std::atomic<bool> *cancelFlag);
    QList<ReadResult> readObjects(const QList<CGXDLMSObject *> &objects, bool forceAll,
                                  std::atomic<bool> *cancelFlag);

    int readProfileGenericColumns(CGXDLMSObject *object);
    int readProfileGenericByEntry(CGXDLMSObject *object, int index, int count, ProfileGenericResult &result);
    int readProfileGenericByRange(CGXDLMSObject *object, const QDateTime &start, const QDateTime &end,
                                  ProfileGenericResult &result);

    QString proposedConformanceString() const;
    QString negotiatedConformanceString() const;

    void applyClientSettings();

public slots:
    void initIo();
    void connectToMeter();
    void disconnectFromMeter();
    void readAllFromMeter(bool forceRead);
    void readSelectedFromMeter(quintptr objectPtr, bool forceAll);
    void readObjectsFromMeter(const QList<quintptr> &objectPtrs, bool forceAll);
    void readAttributeFromMeter(quintptr objectPtr, int attributeIndex);
    void writeAttributeToMeter(quintptr objectPtr, int attributeIndex, const QString &value);
    void invokeMethodOnMeter(quintptr objectPtr, int methodIndex, const QString &parameter);
    void readProfileGenericByEntryFromMeter(quintptr objectPtr, int index, int count);
    void readProfileGenericByRangeFromMeter(quintptr objectPtr, const QDateTime &start,
                                            const QDateTime &end);
    void setNotificationPolling(bool enabled);
    void pollNotifications();
    void shutdown();

    int syncConnect();
    int syncDisconnect();

signals:
    void traceMessage(const QString &message);
    void traceData(const QString &direction, const QByteArray &data);
    void notificationReceived(const QByteArray &data);
    void progressChanged(const QString &description, int current, int maximum);
    void errorOccurred(const QString &message);

    void connectFinished(int ret);
    void disconnectFinished();
    void readAllCompleted(const QList<ReadResult> &results);
    void readSelectedCompleted(const QList<ReadResult> &results);
    void readAttributeCompleted(const ReadResult &result);
    void writeCompleted(const ReadResult &result);
    void methodInvokeCompleted(const ReadResult &result);
    void profileGenericCompleted(quintptr objectPtr, const ProfileGenericResult &result);

private:
    int readDLMSPacket(CGXByteBuffer &data, CGXReplyData &reply);
    int readDataBlock(CGXByteBuffer &data, CGXReplyData &reply);
    int readDataBlock(std::vector<CGXByteBuffer> &data, CGXReplyData &reply);
    int sendData(CGXByteBuffer &data);
    int readBytes(CGXByteBuffer &reply, unsigned char eop);
    int readNetworkBytes(CGXByteBuffer &reply);
    bool usesSerialFrameDelimiter() const;
    bool usesAccessService() const;
    int updateFrameCounter();
    DLMS_INTERFACE_TYPE effectiveInterfaceType() const;

    GXDLMSDevice *m_device;
    std::string m_passwordBuffer;
    std::unique_ptr<CGXDLMSSecureClient> m_client;
    std::unique_ptr<MediaConnection> m_media;
    QTimer *m_notificationTimer = nullptr;
    QByteArray m_notificationBuffer;
    CGXByteBuffer m_rxBuffer;
    std::atomic<bool> *m_cancelFlag = nullptr;
};
