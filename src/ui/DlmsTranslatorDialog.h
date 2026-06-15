#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class DlmsTranslatorDialog; }
QT_END_NAMESPACE

class DlmsTranslatorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DlmsTranslatorDialog(QWidget *parent = nullptr);
    ~DlmsTranslatorDialog() override;

private slots:
    void onTranslate();
    void onClear();

private:
    Ui::DlmsTranslatorDialog *ui;
};
