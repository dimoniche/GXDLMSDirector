#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class PlcDiscoverDialog; }
QT_END_NAMESPACE

class GXDLMSDevice;

class PlcDiscoverDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlcDiscoverDialog(GXDLMSDevice *device, QWidget *parent = nullptr);
    ~PlcDiscoverDialog() override;

private slots:
    void refreshSerialPorts();
    void onMediaTypeChanged(int index);
    void onInterfaceChanged(int index);
    void onDiscover();
    void onStop();
    void onCreateDevice();

private:
    Ui::PlcDiscoverDialog *ui;
    GXDLMSDevice *m_device;
    bool m_cancelled = false;
};
