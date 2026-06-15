#include "PlcDiscoverDialog.h"
#include "ui_PlcDiscoverDialog.h"

#include "core/GXDLMSDevice.h"
#include "core/PlcDiscoverer.h"

#include <enums.h>

#include <QFutureWatcher>
#include <QHeaderView>
#include <QSerialPortInfo>
#include <QtConcurrent>

PlcDiscoverDialog::PlcDiscoverDialog(GXDLMSDevice *device, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PlcDiscoverDialog)
    , m_device(device)
{
    ui->setupUi(this);

    ui->metersTable->horizontalHeader()->setStretchLastSection(true);
    ui->metersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->metersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshSerialPorts();
    onMediaTypeChanged(ui->mediaCombo->currentIndex());

    connect(ui->mediaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PlcDiscoverDialog::onMediaTypeChanged);
    connect(ui->refreshPortsButton, &QPushButton::clicked, this, &PlcDiscoverDialog::refreshSerialPorts);
    connect(ui->discoverButton, &QPushButton::clicked, this, &PlcDiscoverDialog::onDiscover);
    connect(ui->stopButton, &QPushButton::clicked, this, &PlcDiscoverDialog::onStop);
    connect(ui->createDeviceButton, &QPushButton::clicked, this, &PlcDiscoverDialog::onCreateDevice);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->metersTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { ui->createDeviceButton->setEnabled(ui->metersTable->currentRow() >= 0); });
}

PlcDiscoverDialog::~PlcDiscoverDialog()
{
    delete ui;
}

void PlcDiscoverDialog::refreshSerialPorts()
{
    ui->serialPortCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->serialPortCombo->addItem(info.portName());
}

void PlcDiscoverDialog::onMediaTypeChanged(int index)
{
    const bool serial = index == 0;
    ui->serialPortCombo->setVisible(serial);
    ui->refreshPortsButton->setVisible(serial);
    ui->baudRateCombo->setVisible(serial);
    ui->hostEdit->setVisible(!serial);
    ui->portSpin->setVisible(!serial);
}

void PlcDiscoverDialog::onInterfaceChanged(int /*index*/)
{
}

void PlcDiscoverDialog::onDiscover()
{
    PlcDiscoverSettings settings;
    settings.mediaType = ui->mediaCombo->currentIndex() == 0 ? MediaType::Serial : MediaType::Network;
    settings.serialPort = ui->serialPortCombo->currentText();
    settings.baudRate = ui->baudRateCombo->currentText().toInt();
    settings.hostName = ui->hostEdit->text();
    settings.port = static_cast<quint16>(ui->portSpin->value());
    settings.interfaceType = ui->interfaceCombo->currentIndex() == 0 ? DLMS_INTERFACE_TYPE_PLC
                                                                     : DLMS_INTERFACE_TYPE_PLC_HDLC;

    m_cancelled = false;
    ui->discoverButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->metersTable->setRowCount(0);
    ui->logEdit->clear();

    auto *watcher = new QFutureWatcher<QList<PlcMeterInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<PlcMeterInfo>>::finished, this, [this, watcher]() {
        const QList<PlcMeterInfo> meters = watcher->result();
        ui->metersTable->setRowCount(meters.size());
        for (int row = 0; row < meters.size(); ++row) {
            const PlcMeterInfo &meter = meters.at(row);
            ui->metersTable->setItem(row, 0, new QTableWidgetItem(meter.systemTitleHex));
            ui->metersTable->setItem(row, 1, new QTableWidgetItem(QString::number(meter.sourceAddress)));
            ui->metersTable->setItem(row, 2, new QTableWidgetItem(QString::number(meter.destinationAddress)));
            ui->metersTable->setItem(row, 3, new QTableWidgetItem(meter.status));
        }
        ui->discoverButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        ui->logEdit->appendPlainText(tr("Discover finished. Found %1 meter(s).").arg(meters.size()));
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([settings, this]() {
        return PlcDiscoverer::discover(
            settings,
            [this](const QString &message) {
                QMetaObject::invokeMethod(this, [this, message]() { ui->logEdit->appendPlainText(message); },
                                          Qt::QueuedConnection);
            },
            &m_cancelled);
    }));
}

void PlcDiscoverDialog::onStop()
{
    m_cancelled = true;
    ui->stopButton->setEnabled(false);
}

void PlcDiscoverDialog::onCreateDevice()
{
    const int row = ui->metersTable->currentRow();
    if (row < 0 || !m_device)
        return;

    m_device->setInterfaceType(ui->interfaceCombo->currentIndex() == 0 ? DLMS_INTERFACE_TYPE_PLC
                                                                       : DLMS_INTERFACE_TYPE_PLC_HDLC);
    m_device->setMacSourceAddress(
        static_cast<quint16>(ui->metersTable->item(row, 1)->text().toUInt()));
    m_device->setMacDestinationAddress(
        static_cast<quint16>(ui->metersTable->item(row, 2)->text().toUInt()));
    m_device->applyConnectionSettings();
    accept();
}
