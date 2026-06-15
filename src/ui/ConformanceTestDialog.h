#pragma once

#include "core/GXDLMSDevice.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ConformanceTestDialog; }
QT_END_NAMESPACE

class ConformanceTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConformanceTestDialog(GXDLMSDevice *device, QWidget *parent = nullptr);
    ~ConformanceTestDialog() override;

private slots:
    void onBrowse();
    void onRun();
    void onOpenReport();

private:
    Ui::ConformanceTestDialog *ui;
    GXDLMSDevice *m_device;
    QString m_lastReportPath;
};
