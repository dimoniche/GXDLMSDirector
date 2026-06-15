#pragma once

#include "MediaConnection.h"

#include <QList>
#include <QString>
#include <QSerialPort>
#include <functional>

struct PlcMeterInfo
{
    uint16_t sourceAddress = 0;
    uint16_t destinationAddress = 0;
    QString systemTitleHex;
    short alarmDescriptor = 0;
    QString status;
};

struct PlcDiscoverSettings
{
    MediaType mediaType = MediaType::Serial;
    QString serialPort;
    int baudRate = 9600;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QString hostName = QStringLiteral("localhost");
    quint16 port = 4059;
    int interfaceType = 0;
    int waitTimeMs = 10000;
    int durationMs = 30000;
};

class PlcDiscoverer
{
public:
    using ProgressCallback = std::function<void(const QString &message)>;

    static QList<PlcMeterInfo> discover(const PlcDiscoverSettings &settings, ProgressCallback progress,
                                        bool *cancelled = nullptr);
};
