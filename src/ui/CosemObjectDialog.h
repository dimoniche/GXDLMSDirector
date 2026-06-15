#pragma once

#include <QDialog>

#include <enums.h>

QT_BEGIN_NAMESPACE
namespace Ui { class CosemObjectDialog; }
QT_END_NAMESPACE

class CosemObjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CosemObjectDialog(QWidget *parent = nullptr);
    ~CosemObjectDialog() override;

    DLMS_OBJECT_TYPE objectType() const;
    QString logicalName() const;
    int version() const;
    QString description() const;

private:
    Ui::CosemObjectDialog *ui;
};
