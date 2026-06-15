#pragma once

#include "core/HdlcAddressScanner.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class HdlcAddressScannerDialog; }
QT_END_NAMESPACE

class HdlcAddressScannerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HdlcAddressScannerDialog(QWidget *parent = nullptr);
    ~HdlcAddressScannerDialog() override;

private slots:
    void refreshSerialPorts();
    void onMediaTypeChanged(int index);
    void onScan();
    void onStop();

private:
    void appendLog(const QString &message);
    void showResults(const QList<HdlcScanResult> &results);

    Ui::HdlcAddressScannerDialog *ui;
    bool m_cancelled = false;
};
