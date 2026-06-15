#pragma once

#include "MediaConnection.h"

#include <QList>
#include <QString>
#include <QSerialPort>
#include <functional>

struct HdlcScanResult
{
    int clientAddress = 0;
    int serverAddress = 0;
    int logicalAddress = 0;
    int physicalAddress = 0;
    QString details;
    bool aarqSucceeded = false;
};

struct HdlcScanSettings
{
    MediaType mediaType = MediaType::Serial;
    QString serialPort;
    int baudRate = 9600;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QString hostName = QStringLiteral("localhost");
    quint16 port = 4059;
    QList<int> serverAddresses;
    QList<int> clientAddresses;
    int waitTimeMs = 2000;
    bool tryAarq = true;
};

class HdlcAddressScanner
{
public:
    using ProgressCallback = std::function<void(int current, int total, const QString &message)>;

    static QList<HdlcScanResult> scan(const HdlcScanSettings &settings, ProgressCallback progress,
                                        bool *cancelled = nullptr);
};
