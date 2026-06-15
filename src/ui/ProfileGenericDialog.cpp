#include "ProfileGenericDialog.h"
#include "ui_ProfileGenericDialog.h"

#include <GXDLMSObject.h>
#include <enums.h>

#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>

ProfileGenericDialog::ProfileGenericDialog(GXDLMSDevice *device, CGXDLMSObject *object, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProfileGenericDialog)
    , m_device(device)
    , m_object(object)
{
    ui->setupUi(this);

    const QDateTime now = QDateTime::currentDateTime();
    ui->endDateTime->setDateTime(now);
    ui->startDateTime->setDateTime(now.addDays(-1));

    ui->resultTable->horizontalHeader()->setStretchLastSection(true);
    ui->resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfileGenericDialog::onModeChanged);
    connect(ui->readButton, &QPushButton::clicked, this, &ProfileGenericDialog::onRead);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_device, &GXDLMSDevice::profileGenericRead, this, &ProfileGenericDialog::onProfileGenericRead);

    onModeChanged(ui->modeCombo->currentIndex());
}

ProfileGenericDialog::~ProfileGenericDialog()
{
    delete ui;
}

void ProfileGenericDialog::onModeChanged(int index)
{
    const bool byEntry = index == 0;
    ui->indexLabel->setVisible(byEntry);
    ui->indexSpin->setVisible(byEntry);
    ui->countLabel->setVisible(byEntry);
    ui->countSpin->setVisible(byEntry);
    ui->startLabel->setVisible(!byEntry);
    ui->startDateTime->setVisible(!byEntry);
    ui->endLabel->setVisible(!byEntry);
    ui->endDateTime->setVisible(!byEntry);
}

void ProfileGenericDialog::onRead()
{
    ui->readButton->setEnabled(false);
    if (ui->modeCombo->currentIndex() == 0) {
        m_device->readProfileGenericByEntryAsync(m_object, ui->indexSpin->value(), ui->countSpin->value());
    } else {
        m_device->readProfileGenericByRangeAsync(m_object, ui->startDateTime->dateTime(),
                                                 ui->endDateTime->dateTime());
    }
}

void ProfileGenericDialog::onProfileGenericRead(CGXDLMSObject *object, const ProfileGenericResult &result)
{
    if (object != m_object)
        return;

    ui->readButton->setEnabled(true);
    if (result.errorCode != 0) {
        QMessageBox::warning(this, tr("Profile Generic"), result.errorMessage);
        return;
    }

    showResult(result);
}

void ProfileGenericDialog::showResult(const ProfileGenericResult &result)
{
    ui->resultTable->clear();
    ui->resultTable->setColumnCount(result.columnHeaders.size());
    ui->resultTable->setHorizontalHeaderLabels(result.columnHeaders);
    ui->resultTable->setRowCount(result.rows.size());

    for (int row = 0; row < result.rows.size(); ++row) {
        const ProfileGenericRow &profileRow = result.rows.at(row);
        for (int col = 0; col < profileRow.columns.size(); ++col) {
            auto *item = new QTableWidgetItem(profileRow.columns.at(col));
            ui->resultTable->setItem(row, col, item);
        }
    }
    ui->resultTable->resizeColumnsToContents();
}
