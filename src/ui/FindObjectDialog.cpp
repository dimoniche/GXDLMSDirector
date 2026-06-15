#include "FindObjectDialog.h"
#include "ui_FindObjectDialog.h"

FindObjectDialog::FindObjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindObjectDialog)
{
    ui->setupUi(this);
}

FindObjectDialog::~FindObjectDialog()
{
    delete ui;
}

QString FindObjectDialog::searchText() const
{
    return ui->searchTextEdit->text().trimmed();
}

QString FindObjectDialog::logicalName() const
{
    return ui->logicalNameEdit->text().trimmed();
}

void FindObjectDialog::setSearchText(const QString &text)
{
    ui->searchTextEdit->setText(text);
}

void FindObjectDialog::setLogicalName(const QString &logicalName)
{
    ui->logicalNameEdit->setText(logicalName);
}
