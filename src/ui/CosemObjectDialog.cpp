#include "CosemObjectDialog.h"
#include "ui_CosemObjectDialog.h"

#include "core/ObjectAttributeNames.h"

CosemObjectDialog::CosemObjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CosemObjectDialog)
{
    ui->setupUi(this);

    const QStringList labels = ObjectAttributeNames::allObjectTypeLabels();
    const QList<DLMS_OBJECT_TYPE> types = ObjectAttributeNames::allObjectTypes();
    for (int i = 0; i < labels.size() && i < types.size(); ++i)
        ui->typeCombo->addItem(labels.at(i), static_cast<int>(types.at(i)));

    const int clockIndex = ui->typeCombo->findData(static_cast<int>(DLMS_OBJECT_TYPE_CLOCK));
    if (clockIndex >= 0)
        ui->typeCombo->setCurrentIndex(clockIndex);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CosemObjectDialog::~CosemObjectDialog()
{
    delete ui;
}

DLMS_OBJECT_TYPE CosemObjectDialog::objectType() const
{
    return static_cast<DLMS_OBJECT_TYPE>(ui->typeCombo->currentData().toInt());
}

QString CosemObjectDialog::logicalName() const
{
    return ui->logicalNameEdit->text();
}

int CosemObjectDialog::version() const
{
    return ui->versionSpin->value();
}

QString CosemObjectDialog::description() const
{
    return ui->descriptionEdit->text();
}
