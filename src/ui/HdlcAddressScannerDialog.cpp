#include "HdlcAddressScannerDialog.h"
#include "ui_HdlcAddressScannerDialog.h"

#include "core/HdlcAddressScanner.h"

#include <QFutureWatcher>
#include <QHeaderView>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QtConcurrent>

namespace {

QList<int> parseAddressList(const QString &text)
{
    QList<int> addresses;
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int value = part.trimmed().toInt(&ok);
        if (ok)
            addresses.append(value);
    }
    return addresses;
}

} // namespace

HdlcAddressScannerDialog::HdlcAddressScannerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HdlcAddressScannerDialog)
{
    ui->setupUi(this);

    ui->baudRateCombo->setCurrentText(QStringLiteral("9600"));
    ui->resultTable->horizontalHeader()->setStretchLastSection(true);
    ui->resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshSerialPorts();
    onMediaTypeChanged(ui->mediaCombo->currentIndex());

    connect(ui->mediaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HdlcAddressScannerDialog::onMediaTypeChanged);
    connect(ui->refreshPortsButton, &QPushButton::clicked, this, &HdlcAddressScannerDialog::refreshSerialPorts);
    connect(ui->scanButton, &QPushButton::clicked, this, &HdlcAddressScannerDialog::onScan);
    connect(ui->stopButton, &QPushButton::clicked, this, &HdlcAddressScannerDialog::onStop);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

HdlcAddressScannerDialog::~HdlcAddressScannerDialog()
{
    delete ui;
}

void HdlcAddressScannerDialog::refreshSerialPorts()
{
    ui->serialPortCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->serialPortCombo->addItem(info.portName());
}

void HdlcAddressScannerDialog::onMediaTypeChanged(int index)
{
    const bool serial = index == 0;
    ui->serialPortLabel->setVisible(serial);
    ui->serialPortCombo->setVisible(serial);
    ui->refreshPortsButton->setVisible(serial);
    ui->baudRateLabel->setVisible(serial);
    ui->baudRateCombo->setVisible(serial);
    ui->hostLabel->setVisible(!serial);
    ui->hostEdit->setVisible(!serial);
    ui->portLabel->setVisible(!serial);
    ui->portSpin->setVisible(!serial);
}

void HdlcAddressScannerDialog::onScan()
{
    HdlcScanSettings settings;
    settings.mediaType = ui->mediaCombo->currentIndex() == 0 ? MediaType::Serial : MediaType::Network;
    settings.serialPort = ui->serialPortCombo->currentText();
    settings.baudRate = ui->baudRateCombo->currentText().toInt();
    settings.hostName = ui->hostEdit->text();
    settings.port = static_cast<quint16>(ui->portSpin->value());
    settings.serverAddresses = parseAddressList(ui->serverAddressesEdit->text());
    settings.clientAddresses = parseAddressList(ui->clientAddressesEdit->text());
    settings.waitTimeMs = ui->waitTimeSpin->value();
    settings.tryAarq = ui->tryAarqCheck->isChecked();

    if (settings.mediaType == MediaType::Serial && settings.serialPort.isEmpty()) {
        QMessageBox::warning(this, tr("HDLC Scanner"), tr("Select a serial port."));
        return;
    }

    m_cancelled = false;
    ui->scanButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->logEdit->clear();
    ui->resultTable->setRowCount(0);
    ui->progressBar->setValue(0);

    auto *watcher = new QFutureWatcher<QList<HdlcScanResult>>(this);
    connect(watcher, &QFutureWatcher<QList<HdlcScanResult>>::finished, this, [this, watcher]() {
        showResults(watcher->result());
        ui->scanButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        appendLog(tr("Scan finished. Found %1 address(es).").arg(watcher->result().size()));
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([settings, this]() {
        return HdlcAddressScanner::scan(
            settings,
            [this](int current, int total, const QString &message) {
                QMetaObject::invokeMethod(this, [this, current, total, message]() {
                    ui->progressBar->setMaximum(total);
                    ui->progressBar->setValue(current);
                    appendLog(message);
                }, Qt::QueuedConnection);
            },
            &m_cancelled);
    }));
}

void HdlcAddressScannerDialog::onStop()
{
    m_cancelled = true;
    ui->stopButton->setEnabled(false);
    appendLog(tr("Stopping scan..."));
}

void HdlcAddressScannerDialog::appendLog(const QString &message)
{
    ui->logEdit->appendPlainText(message);
}

void HdlcAddressScannerDialog::showResults(const QList<HdlcScanResult> &results)
{
    ui->resultTable->setRowCount(results.size());
    for (int row = 0; row < results.size(); ++row) {
        const HdlcScanResult &result = results.at(row);
        ui->resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(result.clientAddress)));
        ui->resultTable->setItem(row, 1, new QTableWidgetItem(QString::number(result.serverAddress)));
        ui->resultTable->setItem(row, 2, new QTableWidgetItem(QString::number(result.logicalAddress)));
        ui->resultTable->setItem(row, 3, new QTableWidgetItem(QString::number(result.physicalAddress)));
        ui->resultTable->setItem(row, 4, new QTableWidgetItem(result.details));
    }
    ui->resultTable->resizeColumnsToContents();
}
