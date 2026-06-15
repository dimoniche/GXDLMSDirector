#pragma once

#include "DeviceState.h"
#include "MediaConnection.h"
#include "ProfileGenericResult.h"
#include "ReadResult.h"

#include <QDateTime>

#include <GXDLMSSecureClient.h>
#include <GXDLMSObject.h>
#include <GXReplyData.h>

#include <QList>
#include <QObject>
#include <QString>
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
    QList<ReadResult> readAll();

    int readProfileGenericColumns(CGXDLMSObject *object);
    int readProfileGenericByEntry(CGXDLMSObject *object, int index, int count, ProfileGenericResult &result);
    int readProfileGenericByRange(CGXDLMSObject *object, const QDateTime &start, const QDateTime &end,
                                  ProfileGenericResult &result);

    QString proposedConformanceString() const;
    QString negotiatedConformanceString() const;

    void applyClientSettings();

signals:
    void traceMessage(const QString &message);
    void progressChanged(const QString &description, int current, int maximum);
    void errorOccurred(const QString &message);

private:
    int readDLMSPacket(CGXByteBuffer &data, CGXReplyData &reply);
    int readDataBlock(CGXByteBuffer &data, CGXReplyData &reply);
    int readDataBlock(std::vector<CGXByteBuffer> &data, CGXReplyData &reply);
    int sendData(CGXByteBuffer &data);
    int readBytes(CGXByteBuffer &reply, unsigned char eop);

    GXDLMSDevice *m_device;
    std::string m_passwordBuffer;
    std::unique_ptr<CGXDLMSSecureClient> m_client;
    std::unique_ptr<MediaConnection> m_media;
};
