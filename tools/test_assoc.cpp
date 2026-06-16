#include <GXDLMSSecureClient.h>
#include <GXDLMSConverter.h>
#include <GXBytebuffer.h>
#include <GXReplyData.h>
#include <enums.h>
#include <GXHelpers.h>

#include "../src/core/AssociationViewParser.h"

#include <QTcpSocket>
#include <QCoreApplication>

#include <cstdio>
#include <vector>

static int sendFrame(QTcpSocket &sock, const unsigned char *data, int size)
{
    return sock.write(reinterpret_cast<const char *>(data), size) == size ? 0 : 1;
}

static int readChunk(QTcpSocket &sock, CGXByteBuffer &buf, int timeoutMs)
{
    if (!sock.waitForReadyRead(timeoutMs))
        return 1;
    const QByteArray chunk = sock.readAll();
    if (chunk.isEmpty())
        return 1;
    return buf.Set(chunk.constData(), static_cast<unsigned long>(chunk.size()));
}

static int readPacket(CGXDLMSSecureClient &client, QTcpSocket &sock, CGXByteBuffer &tx, CGXReplyData &reply)
{
    if (tx.GetSize() != 0 && sendFrame(sock, tx.GetData(), static_cast<int>(tx.GetSize())) != 0)
        return 1;

    CGXByteBuffer rx;
    CGXReplyData notify;
    int ret = 0;
    do {
        CGXByteBuffer chunk;
        if ((ret = readChunk(sock, chunk, 5000)) != 0)
            return ret;
        if ((ret = rx.Set(&chunk)) != 0)
            return ret;
        ret = client.GetData(rx, reply, notify);
        if (ret != DLMS_ERROR_CODE_FALSE) {
            if (rx.GetPosition() >= rx.GetSize())
                rx.Clear();
            else
                rx.Trim();
        }
    } while (ret == DLMS_ERROR_CODE_FALSE);

    return ret;
}

static int readBlock(CGXDLMSSecureClient &client, QTcpSocket &sock, std::vector<CGXByteBuffer> &data,
                     CGXReplyData &reply)
{
    for (auto &packet : data) {
        reply.Clear();
        int ret = readPacket(client, sock, packet, reply);
        if (ret != 0)
            return ret;
        while (reply.IsMoreData()) {
            CGXByteBuffer rr;
            if (!reply.IsStreaming()) {
                if ((ret = client.ReceiverReady(reply.GetMoreData(), rr)) != 0)
                    return ret;
            }
            if ((ret = readPacket(client, sock, rr, reply)) != 0)
                return ret;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    CGXDLMSSecureClient client(true, 32, 1, DLMS_AUTHENTICATION_LOW, "12345678",
                               DLMS_INTERFACE_TYPE_WRAPPER);

    QTcpSocket sock;
    sock.connectToHost(QStringLiteral("localhost"), 4059);
    if (!sock.waitForConnected(5000)) {
        std::fprintf(stderr, "connect failed\n");
        return 1;
    }

    std::vector<CGXByteBuffer> data;
    CGXReplyData reply;
    int ret = 0;

    if ((ret = client.AARQRequest(data)) != 0
        || (ret = readBlock(client, sock, data, reply)) != 0
        || (ret = client.ParseAAREResponse(reply.GetData())) != 0) {
        std::fprintf(stderr, "AARE failed %d %s\n", ret, CGXDLMSConverter::GetErrorMessage(ret));
        return 1;
    }

    reply.Clear();
    if ((ret = client.GetObjectsRequest(data)) != 0
        || (ret = readBlock(client, sock, data, reply)) != 0) {
        std::fprintf(stderr, "read failed %d %s more=%d\n", ret,
                     CGXDLMSConverter::GetErrorMessage(ret), reply.IsMoreData());
        return 1;
    }

    std::fprintf(stderr, "after read: more=%d valueVt=%d arrSize=%zu dataSize=%lu pos=%lu\n",
                 reply.IsMoreData(), reply.GetValue().vt, reply.GetValue().Arr.size(),
                 reply.GetData().GetSize(), reply.GetData().GetPosition());

    if (reply.GetData().GetSize() > 0) {
        unsigned char b = 0;
        reply.GetData().GetUInt8(0, &b);
        std::fprintf(stderr, "data[0]=0x%02X\n", b);
        reply.GetData().SetPosition(0);
    }

    unsigned char tag = 0;
    unsigned long cnt = 0;
    reply.GetData().GetUInt8(0, &tag);
    reply.GetData().GetPosition();
    CGXByteBuffer &buf = reply.GetData();
    buf.SetPosition(1);
    GXHelpers::GetObjectCount(buf, cnt);
    buf.SetPosition(0);
    std::fprintf(stderr, "arrayTag=0x%02X declaredCount=%lu parsedArr=%zu bufSize=%lu\n", tag, cnt,
                 reply.GetValue().Arr.size(), reply.GetData().GetSize());

    reply.GetData().SetPosition(0);
    std::vector<CGXDLMSVariant> objects = reply.GetValue().Arr;
    AssociationViewParser::normalizeObjects(objects);
    ret = client.ParseObjects(objects, true);
    std::fprintf(stderr, "ParseObjects(normalized) -> %d %s objects=%zu\n", ret,
                 CGXDLMSConverter::GetErrorMessage(ret), client.GetObjects().size());

    return ret == 0 ? 0 : 1;
}
