#include "MediaConnection.h"

#include <errorcodes.h>

#include <QIODevice>

namespace {
constexpr int DLMS_COMM_ERROR = DLMS_ERROR_TYPE_COMMUNICATION_ERROR;
}

MediaConnection::MediaConnection(QObject *parent)
    : QObject(parent)
{
}

MediaConnection::~MediaConnection()
{
    close();
}

bool MediaConnection::isOpen() const
{
    QMutexLocker lock(&m_mutex);
    if (m_serial)
        return m_serial->isOpen();
    if (m_socket)
        return m_socket->state() == QAbstractSocket::ConnectedState;
    return false;
}

int MediaConnection::openSerial(const QString &portName, int baudRate,
                                QSerialPort::DataBits dataBits,
                                QSerialPort::Parity parity,
                                QSerialPort::StopBits stopBits)
{
    QMutexLocker lock(&m_mutex);
    close();
    m_type = MediaType::Serial;
    m_serial = std::make_unique<QSerialPort>();
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(dataBits);
    m_serial->setParity(parity);
    m_serial->setStopBits(stopBits);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        return DLMS_COMM_ERROR | static_cast<int>(m_serial->error());
    }
    return 0;
}

int MediaConnection::openNetwork(const QString &host, quint16 port)
{
    QMutexLocker lock(&m_mutex);
    close();
    m_type = MediaType::Network;
    m_socket = std::make_unique<QTcpSocket>();
    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(m_waitTimeMs)) {
        const int err = static_cast<int>(m_socket->error());
        m_socket.reset();
        return DLMS_COMM_ERROR | err;
    }

    while (m_socket->bytesAvailable() > 0)
        m_socket->readAll();

    return 0;
}

void MediaConnection::close()
{
    m_hdlcStash.clear();

    if (m_serial && m_serial->isOpen())
        m_serial->close();
    m_serial.reset();

    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState)
            m_socket->disconnectFromHost();
        m_socket.reset();
    }
}

void MediaConnection::clearReadStash()
{
    QMutexLocker lock(&m_mutex);
    m_hdlcStash.clear();
}

void MediaConnection::drainInput()
{
    QMutexLocker lock(&m_mutex);
    QIODevice *io = m_serial ? static_cast<QIODevice *>(m_serial.get())
                             : m_socket ? static_cast<QIODevice *>(m_socket.get()) : nullptr;
    if (!io)
        return;

    while (io->bytesAvailable() > 0)
        io->readAll();
}

namespace {

int closingHdlcFlagIndex(const QByteArray &buffer, int startPos)
{
    for (int pos = buffer.size() - 1; pos > startPos; --pos) {
        if (static_cast<unsigned char>(buffer.at(pos)) == 0x7E)
            return pos;
    }
    return -1;
}

} // namespace

int MediaConnection::readHdlcFrame(QByteArray &frame)
{
    QMutexLocker lock(&m_mutex);
    frame.clear();
    if (!m_serial && !m_socket)
        return DLMS_COMM_ERROR;

    while (true) {
        const int start = m_hdlcStash.indexOf(static_cast<char>(0x7E));
        if (start >= 0) {
            const int end = closingHdlcFlagIndex(m_hdlcStash, start);
            if (end > start) {
                frame = m_hdlcStash.mid(start, end - start + 1);
                m_hdlcStash.remove(0, end + 1);
                emit traceData(QStringLiteral("RX"), frame);
                return 0;
            }
        }

        QIODevice *io = m_serial ? static_cast<QIODevice *>(m_serial.get())
                                 : static_cast<QIODevice *>(m_socket.get());
        if (io->bytesAvailable() == 0 && !io->waitForReadyRead(m_waitTimeMs)) {
            const int err = m_serial ? static_cast<int>(m_serial->error())
                                   : static_cast<int>(m_socket->error());
            return DLMS_COMM_ERROR | err;
        }

        const QByteArray chunk = io->readAll();
        if (chunk.isEmpty())
            return DLMS_COMM_ERROR;

        m_hdlcStash.append(chunk);
    }
}

int MediaConnection::sendData(const unsigned char *data, int size)
{
    QMutexLocker lock(&m_mutex);
    if ((!m_serial && !m_socket) || size <= 0)
        return DLMS_COMM_ERROR;

    const QByteArray payload(reinterpret_cast<const char *>(data), size);
    emit traceData(QStringLiteral("TX"), payload);

    qint64 written = 0;
    if (m_serial)
        written = m_serial->write(payload);
    else if (m_socket)
        written = m_socket->write(payload);

    if (written != size)
        return DLMS_COMM_ERROR;

    if (m_serial && !m_serial->waitForBytesWritten(m_waitTimeMs))
        return DLMS_COMM_ERROR | static_cast<int>(m_serial->error());

    if (m_socket && !m_socket->waitForBytesWritten(m_waitTimeMs))
        return DLMS_COMM_ERROR | static_cast<int>(m_socket->error());

    return 0;
}

int MediaConnection::readUntilByte(QByteArray &buffer, unsigned char eop)
{
    QMutexLocker lock(&m_mutex);
    buffer.clear();
    if (!m_serial && !m_socket)
        return DLMS_COMM_ERROR;

    int lastIndex = 0;
    while (true) {
        QIODevice *io = m_serial ? static_cast<QIODevice *>(m_serial.get())
                                 : static_cast<QIODevice *>(m_socket.get());
        if (!io->waitForReadyRead(m_waitTimeMs)) {
            const int err = m_serial ? static_cast<int>(m_serial->error())
                                   : static_cast<int>(m_socket->error());
            return DLMS_COMM_ERROR | err;
        }

        const QByteArray chunk = io->readAll();
        if (chunk.isEmpty())
            return DLMS_COMM_ERROR;

        buffer.append(chunk);

        if (buffer.size() <= 4)
            continue;

        for (int pos = buffer.size() - 1; pos >= lastIndex; --pos) {
            if (static_cast<unsigned char>(buffer.at(pos)) == eop) {
                emit traceData(QStringLiteral("RX"), buffer);
                return 0;
            }
        }
        lastIndex = buffer.size() - 1;
    }
}

int MediaConnection::readData(QByteArray &buffer, unsigned char eop)
{
    QMutexLocker lock(&m_mutex);
    if (!m_serial)
        return DLMS_COMM_ERROR;
    lock.unlock();
    return readUntilByte(buffer, eop);
}

int MediaConnection::readNetworkChunk(QByteArray &buffer)
{
    QMutexLocker lock(&m_mutex);
    buffer.clear();
    if (!m_socket)
        return DLMS_COMM_ERROR;

    if (!m_socket->waitForReadyRead(m_waitTimeMs))
        return DLMS_COMM_ERROR | static_cast<int>(m_socket->error());

    buffer = m_socket->readAll();
    if (buffer.isEmpty())
        return DLMS_COMM_ERROR;

    emit traceData(QStringLiteral("RX"), buffer);
    return 0;
}

int MediaConnection::readAvailable(QByteArray &buffer, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    buffer.clear();
    if (!m_serial && !m_socket)
        return DLMS_COMM_ERROR;

    if (m_serial) {
        if (!m_serial->waitForReadyRead(timeoutMs))
            return m_serial->bytesAvailable() > 0 ? 0 : DLMS_COMM_ERROR;
        buffer = m_serial->readAll();
    } else if (m_socket) {
        if (!m_socket->waitForReadyRead(timeoutMs))
            return m_socket->bytesAvailable() > 0 ? 0 : DLMS_COMM_ERROR;
        buffer = m_socket->readAll();
    }

    return buffer.isEmpty() ? DLMS_COMM_ERROR : 0;
}
