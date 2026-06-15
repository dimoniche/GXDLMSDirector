#include "DevicePropertiesDialog.h"
#include "ui_DevicePropertiesDialog.h"

#include "core/ManufacturerSettings.h"

#include <enums.h>

#include <QSerialPortInfo>
#include <QSignalBlocker>

DevicePropertiesDialog::DevicePropertiesDialog(GXDLMSDevice *device, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DevicePropertiesDialog)
    , m_device(device)
{
    ui->setupUi(this);

    for (const ManufacturerProfile &profile : ManufacturerSettings::instance().profiles())
        ui->manufacturerCombo->addItem(profile.name + QStringLiteral(" (") + profile.id + QLatin1Char(')'), profile.id);

    refreshSerialPorts();
    loadFromDevice();
    updateMediaVisibility();

    connect(ui->mediaTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DevicePropertiesDialog::onMediaTypeChanged);
    connect(ui->manufacturerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DevicePropertiesDialog::onManufacturerChanged);
    connect(ui->interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DevicePropertiesDialog::onInterfaceChanged);
    connect(ui->refreshPortsButton, &QPushButton::clicked,
            this, &DevicePropertiesDialog::refreshSerialPorts);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DevicePropertiesDialog::applySettings);
}

DevicePropertiesDialog::~DevicePropertiesDialog()
{
    delete ui;
}

void DevicePropertiesDialog::refreshSerialPorts()
{
    const QString current = ui->serialPortCombo->currentText();
    QSignalBlocker blocker(ui->serialPortCombo);
    ui->serialPortCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->serialPortCombo->addItem(info.portName());

    const int index = ui->serialPortCombo->findText(current);
    if (index >= 0)
        ui->serialPortCombo->setCurrentIndex(index);
    else if (!current.isEmpty())
        ui->serialPortCombo->setEditText(current);
}

void DevicePropertiesDialog::onMediaTypeChanged(int /*index*/)
{
    updateMediaVisibility();
}

void DevicePropertiesDialog::updateMediaVisibility()
{
    const bool serial = ui->mediaTypeCombo->currentIndex() == 0;
    ui->labelSerialPort->setVisible(serial);
    ui->serialPortCombo->setVisible(serial);
    ui->refreshPortsButton->setVisible(serial);
    ui->labelBaudRate->setVisible(serial);
    ui->baudRateCombo->setVisible(serial);

    ui->labelHost->setVisible(!serial);
    ui->hostEdit->setVisible(!serial);
    ui->labelPort->setVisible(!serial);
    ui->portSpin->setVisible(!serial);
}

void DevicePropertiesDialog::loadFromDevice()
{
    if (!m_device)
        return;

    ui->nameEdit->setText(m_device->name());
    const int manufacturerIndex = ui->manufacturerCombo->findData(m_device->manufacturer());
    if (manufacturerIndex >= 0)
        ui->manufacturerCombo->setCurrentIndex(manufacturerIndex);
    ui->mediaTypeCombo->setCurrentIndex(m_device->mediaType() == MediaType::Serial ? 0 : 1);
    ui->serialPortCombo->setEditText(m_device->serialPort());
    ui->baudRateCombo->setCurrentText(QString::number(m_device->baudRate()));
    ui->hostEdit->setText(m_device->hostName());
    ui->portSpin->setValue(m_device->port());
    ui->waitTimeSpin->setValue(m_device->waitTimeMs());
    ui->referencingCombo->setCurrentIndex(m_device->useLogicalNameReferencing() ? 0 : 1);
    ui->interfaceCombo->setCurrentIndex(interfaceIndexFromType(m_device->interfaceType()));
    ui->macSourceSpin->setValue(m_device->macSourceAddress());
    ui->macDestinationSpin->setValue(m_device->macDestinationAddress());
    ui->clientAddressSpin->setValue(m_device->clientAddress());
    ui->serverAddressSpin->setValue(static_cast<int>(m_device->serverAddress()));
    ui->authCombo->setCurrentIndex(m_device->authentication());
    ui->passwordEdit->setText(m_device->password());
    ui->proposedConformanceEdit->setText(m_device->proposedConformance());
    ui->negotiatedConformanceEdit->setText(m_device->negotiatedConformance());
    updateInterfaceVisibility();
}

int DevicePropertiesDialog::interfaceTypeFromIndex(int index) const
{
    switch (index) {
    case 1:
        return DLMS_INTERFACE_TYPE_WRAPPER;
    case 2:
        return DLMS_INTERFACE_TYPE_PLC;
    case 3:
        return DLMS_INTERFACE_TYPE_PLC_HDLC;
    default:
        return DLMS_INTERFACE_TYPE_HDLC;
    }
}

int DevicePropertiesDialog::interfaceIndexFromType(int type) const
{
    switch (type) {
    case DLMS_INTERFACE_TYPE_WRAPPER:
        return 1;
    case DLMS_INTERFACE_TYPE_PLC:
        return 2;
    case DLMS_INTERFACE_TYPE_PLC_HDLC:
        return 3;
    default:
        return 0;
    }
}

void DevicePropertiesDialog::onInterfaceChanged(int /*index*/)
{
    updateInterfaceVisibility();
}

void DevicePropertiesDialog::updateInterfaceVisibility()
{
    const int interfaceType = interfaceTypeFromIndex(ui->interfaceCombo->currentIndex());
    const bool plc = interfaceType == DLMS_INTERFACE_TYPE_PLC
                     || interfaceType == DLMS_INTERFACE_TYPE_PLC_HDLC;
    ui->labelMacSource->setVisible(plc);
    ui->macSourceSpin->setVisible(plc);
    ui->labelMacDestination->setVisible(plc);
    ui->macDestinationSpin->setVisible(plc);
}

void DevicePropertiesDialog::applySettings()
{
    if (!m_device)
        return;

    m_device->setName(ui->nameEdit->text());
    m_device->setManufacturer(ui->manufacturerCombo->currentData().toString());
    m_device->applyConnectionSettings();
    m_device->setMediaType(ui->mediaTypeCombo->currentIndex() == 0 ? MediaType::Serial : MediaType::Network);
    m_device->setSerialPort(ui->serialPortCombo->currentText());
    m_device->setBaudRate(ui->baudRateCombo->currentText().toInt());
    m_device->setHostName(ui->hostEdit->text());
    m_device->setPort(static_cast<quint16>(ui->portSpin->value()));
    m_device->setWaitTimeMs(ui->waitTimeSpin->value());
    m_device->setUseLogicalNameReferencing(ui->referencingCombo->currentIndex() == 0);
    m_device->setInterfaceType(interfaceTypeFromIndex(ui->interfaceCombo->currentIndex()));
    m_device->setMacSourceAddress(static_cast<quint16>(ui->macSourceSpin->value()));
    m_device->setMacDestinationAddress(static_cast<quint16>(ui->macDestinationSpin->value()));
    m_device->setClientAddress(static_cast<unsigned char>(ui->clientAddressSpin->value()));
    m_device->setServerAddress(static_cast<unsigned long>(ui->serverAddressSpin->value()));
    m_device->setAuthentication(ui->authCombo->currentIndex());
    m_device->setPassword(ui->passwordEdit->text());
}

void DevicePropertiesDialog::onManufacturerChanged(int index)
{
    if (index < 0 || !m_device)
        return;

    const QString id = ui->manufacturerCombo->itemData(index).toString();
    ManufacturerSettings::instance().applyToDevice(ManufacturerSettings::instance().profile(id), m_device);

    ui->referencingCombo->setCurrentIndex(m_device->useLogicalNameReferencing() ? 0 : 1);
    ui->interfaceCombo->setCurrentIndex(interfaceIndexFromType(m_device->interfaceType()));
    ui->macSourceSpin->setValue(m_device->macSourceAddress());
    ui->macDestinationSpin->setValue(m_device->macDestinationAddress());
    ui->clientAddressSpin->setValue(m_device->clientAddress());
    ui->serverAddressSpin->setValue(static_cast<int>(m_device->serverAddress()));
    ui->authCombo->setCurrentIndex(m_device->authentication());
}
