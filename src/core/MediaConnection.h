#pragma once

#include <QMutex>
#include <QObject>
#include <QSerialPort>
#include <QTcpSocket>
#include <memory>

enum class MediaType {
    Serial,
    Network
};

class MediaConnection : public QObject
{
    Q_OBJECT

public:
    explicit MediaConnection(QObject *parent = nullptr);
    ~MediaConnection() override;

    MediaType type() const { return m_type; }
    bool isOpen() const;
    int waitTimeMs() const { return m_waitTimeMs; }
    void setWaitTimeMs(int ms) { m_waitTimeMs = ms; }

    int openSerial(const QString &portName, int baudRate, QSerialPort::DataBits dataBits,
                   QSerialPort::Parity parity, QSerialPort::StopBits stopBits);
    int openNetwork(const QString &host, quint16 port);

    void close();

    int sendData(const unsigned char *data, int size);
    int readData(QByteArray &buffer, unsigned char eop);
    int readNetworkChunk(QByteArray &buffer);
    int readAvailable(QByteArray &buffer, int timeoutMs);

signals:
    void traceData(const QString &direction, const QByteArray &data);

private:
    MediaType m_type = MediaType::Serial;
    int m_waitTimeMs = 5000;
    mutable QMutex m_mutex;
    std::unique_ptr<QSerialPort> m_serial;
    std::unique_ptr<QTcpSocket> m_socket;
};
