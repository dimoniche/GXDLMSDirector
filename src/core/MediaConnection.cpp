#include "MediaConnection.h"

#include <QDateTime>

namespace {
constexpr int DLMS_COMM_ERROR = 0x10000;
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
    close();
    m_type = MediaType::Serial;
    m_serial = std::make_unique<QSerialPort>(this);
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
    close();
    m_type = MediaType::Network;
    m_socket = std::make_unique<QTcpSocket>(this);
    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(m_waitTimeMs)) {
        const int err = static_cast<int>(m_socket->error());
        m_socket.reset();
        return DLMS_COMM_ERROR | err;
    }
    return 0;
}

void MediaConnection::close()
{
    if (m_serial && m_serial->isOpen())
        m_serial->close();
    m_serial.reset();

    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState)
            m_socket->disconnectFromHost();
        m_socket.reset();
    }
}

int MediaConnection::sendData(const unsigned char *data, int size)
{
    if (!isOpen() || size <= 0)
        return DLMS_COMM_ERROR;

    const QByteArray payload(reinterpret_cast<const char *>(data), size);
    emit traceMessage(QStringLiteral("TX"),
                      QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))
                          + QStringLiteral("\t") + payload.toHex(' ').toUpper());

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

int MediaConnection::readData(QByteArray &buffer, unsigned char eop)
{
    buffer.clear();
    if (!isOpen())
        return DLMS_COMM_ERROR;

    int lastIndex = 0;
    while (true) {
        QByteArray chunk;
        if (m_serial) {
            if (!m_serial->waitForReadyRead(m_waitTimeMs))
                return DLMS_COMM_ERROR | static_cast<int>(m_serial->error());
            chunk = m_serial->readAll();
        } else if (m_socket) {
            if (!m_socket->waitForReadyRead(m_waitTimeMs))
                return DLMS_COMM_ERROR | static_cast<int>(m_socket->error());
            chunk = m_socket->readAll();
        }

        if (chunk.isEmpty())
            return DLMS_COMM_ERROR;

        buffer.append(chunk);

        for (int pos = buffer.size() - 1; pos >= lastIndex; --pos) {
            if (static_cast<unsigned char>(buffer.at(pos)) == eop) {
                emit traceMessage(QStringLiteral("RX"),
                                  QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))
                                      + QStringLiteral("\t") + buffer.toHex(' ').toUpper());
                return 0;
            }
        }
        lastIndex = buffer.size() - 1;
    }
}
