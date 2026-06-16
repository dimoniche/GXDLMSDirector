#pragma once

#include "core/GXDLMSDevice.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class DevicePropertiesDialog; }
QT_END_NAMESPACE

class DevicePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DevicePropertiesDialog(GXDLMSDevice *device, QWidget *parent = nullptr);
    ~DevicePropertiesDialog() override;

private slots:
    void refreshSerialPorts();
    void onMediaTypeChanged(int index);
    void onManufacturerChanged(int index);
    void onInterfaceChanged(int index);
    void onSecurityChanged(int index);
    void applySettings();

private:
    void loadFromDevice();
    void updateMediaVisibility();
    void updateInterfaceVisibility();
    void updateSecurityVisibility();
    void setupSecurityCombo();
    static QString normalizeHexKey(const QString &text);
    int interfaceTypeFromIndex(int index) const;
    int interfaceIndexFromType(int type) const;

    Ui::DevicePropertiesDialog *ui;
    GXDLMSDevice *m_device;
};
