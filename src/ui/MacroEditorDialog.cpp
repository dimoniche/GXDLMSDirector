#include "MacroEditorDialog.h"
#include "ui_MacroEditorDialog.h"

#include "core/MacroRunner.h"
#include "core/MacroSerializer.h"

#include <QFileDialog>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QtConcurrent>

#include <algorithm>

MacroEditorDialog::MacroEditorDialog(GXDLMSDevice *device, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MacroEditorDialog)
    , m_device(device)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->stepsTable->horizontalHeader()->setStretchLastSection(true);
    ui->stepsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->stepsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->openButton, &QPushButton::clicked, this, &MacroEditorDialog::onOpen);
    connect(ui->saveButton, &QPushButton::clicked, this, &MacroEditorDialog::onSave);
    connect(ui->saveAsButton, &QPushButton::clicked, this, &MacroEditorDialog::onSaveAs);
    connect(ui->runAllButton, &QPushButton::clicked, this, &MacroEditorDialog::onRunAll);
    connect(ui->runSelectedButton, &QPushButton::clicked, this, &MacroEditorDialog::onRunSelected);
    connect(ui->stopButton, &QPushButton::clicked, this, &MacroEditorDialog::onStop);
    connect(ui->recordCheck, &QCheckBox::toggled, this, &MacroEditorDialog::onToggleRecord);
    connect(ui->addDelayButton, &QPushButton::clicked, this, &MacroEditorDialog::onAddDelay);
    connect(ui->removeButton, &QPushButton::clicked, this, &MacroEditorDialog::onRemove);
    connect(ui->stepsTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MacroEditorDialog::onSelectionChanged);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);

    updateTitle();
}

MacroEditorDialog::~MacroEditorDialog()
{
    if (m_recording)
        emit recordingChanged(false);
    delete ui;
}

void MacroEditorDialog::addRecordedStep(const MacroStep &step)
{
    if (!m_recording)
        return;

    m_steps.append(step);
    m_dirty = true;
    refreshList();
    updateTitle();
}

void MacroEditorDialog::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Macro"), m_path,
                                                      tr("Macro files (*.gxm *.gdm)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!MacroSerializer::load(path, m_steps, &error)) {
        QMessageBox::warning(this, tr("Open Macro"), error);
        return;
    }

    m_path = path;
    m_dirty = false;
    refreshList();
    updateTitle();
}

void MacroEditorDialog::onSave()
{
    if (m_path.isEmpty()) {
        onSaveAs();
        return;
    }

    QString error;
    if (!MacroSerializer::save(m_path, m_steps, &error)) {
        QMessageBox::warning(this, tr("Save Macro"), error);
        return;
    }

    m_dirty = false;
    updateTitle();
}

void MacroEditorDialog::onSaveAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Macro As"), m_path,
                                                tr("Macro files (*.gxm)"));
    if (path.isEmpty())
        return;

    if (!path.endsWith(QStringLiteral(".gxm"), Qt::CaseInsensitive))
        path += QStringLiteral(".gxm");

    m_path = path;
    onSave();
}

void MacroEditorDialog::runSteps(const QVector<MacroStep> &steps)
{
    m_cancelled = false;
    ui->runAllButton->setEnabled(false);
    ui->runSelectedButton->setEnabled(false);
    ui->stopButton->setEnabled(true);

    auto *watcher = new QFutureWatcher<MacroRunResult>(this);
    connect(watcher, &QFutureWatcher<MacroRunResult>::finished, this, [this, watcher]() {
        const MacroRunResult result = watcher->result();
        ui->logEdit->appendPlainText(tr("Finished: %1 passed, %2 failed, %3 skipped")
                                         .arg(result.passed)
                                         .arg(result.failed)
                                         .arg(result.skipped));

        for (int i = 0; i < result.steps.size() && i < ui->stepsTable->rowCount(); ++i) {
            const MacroStep &step = result.steps.at(i);
            auto *item = ui->stepsTable->item(i, 3);
            if (!item)
                continue;
            item->setText(step.lastException.isEmpty() ? tr("OK") : step.lastException);
        }

        ui->runAllButton->setEnabled(true);
        ui->runSelectedButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([this, steps]() {
        return MacroRunner::run(
            m_device, steps,
            [this](int current, int total, const QString &message) {
                QMetaObject::invokeMethod(
                    this,
                    [this, current, total, message]() {
                        ui->progressBar->setMaximum(total);
                        ui->progressBar->setValue(current);
                        ui->logEdit->appendPlainText(message);
                    },
                    Qt::QueuedConnection);
            },
            &m_cancelled);
    }));
}

void MacroEditorDialog::onRunAll()
{
    runSteps(m_steps);
}

void MacroEditorDialog::onRunSelected()
{
    QVector<MacroStep> selected;
    for (const QModelIndex &index : ui->stepsTable->selectionModel()->selectedRows()) {
        if (index.row() >= 0 && index.row() < m_steps.size())
            selected.append(m_steps.at(index.row()));
    }
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Macro Editor"), tr("Select one or more steps."));
        return;
    }
    runSteps(selected);
}

void MacroEditorDialog::onStop()
{
    m_cancelled = true;
    ui->stopButton->setEnabled(false);
}

void MacroEditorDialog::onAddDelay()
{
    const int ms = QInputDialog::getInt(this, tr("Add Delay"), tr("Delay (ms):"), 1000, 0, 600000);
    MacroStep step;
    step.type = MacroActionType::Delay;
    step.name = tr("Delay %1 ms").arg(ms);
    step.value = QString::number(ms);
    step.timestamp = QDateTime::currentDateTime();
    m_steps.append(step);
    m_dirty = true;
    refreshList();
    updateTitle();
}

void MacroEditorDialog::onRemove()
{
    QList<int> rows;
    for (const QModelIndex &index : ui->stepsTable->selectionModel()->selectedRows())
        rows.append(index.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < m_steps.size())
            m_steps.removeAt(row);
    }
    m_dirty = true;
    refreshList();
    updateTitle();
}

void MacroEditorDialog::onToggleRecord(bool enabled)
{
    m_recording = enabled;
    emit recordingChanged(enabled);
    ui->logEdit->appendPlainText(enabled ? tr("Recording started.") : tr("Recording stopped."));
}

void MacroEditorDialog::onSelectionChanged()
{
    ui->removeButton->setEnabled(!ui->stepsTable->selectionModel()->selectedRows().isEmpty());
}

QString MacroEditorDialog::typeLabel(const MacroStep &step) const
{
    switch (step.type) {
    case MacroActionType::Connect:
        return tr("Connect");
    case MacroActionType::Disconnect:
        return tr("Disconnect");
    case MacroActionType::Get:
        return tr("Get");
    case MacroActionType::Set:
        return tr("Set");
    case MacroActionType::Action:
        return tr("Action");
    case MacroActionType::Delay:
        return tr("Delay");
    default:
        return tr("None");
    }
}

void MacroEditorDialog::refreshList()
{
    ui->stepsTable->setRowCount(m_steps.size());
    for (int i = 0; i < m_steps.size(); ++i) {
        const MacroStep &step = m_steps.at(i);
        ui->stepsTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        ui->stepsTable->setItem(i, 1, new QTableWidgetItem(typeLabel(step)));
        ui->stepsTable->setItem(i, 2, new QTableWidgetItem(step.name));
        ui->stepsTable->setItem(i, 3, new QTableWidgetItem(QString()));
    }
    ui->stepsTable->resizeColumnsToContents();
}

void MacroEditorDialog::updateTitle()
{
    QString title = tr("Macro Editor");
    if (!m_path.isEmpty())
        title += QStringLiteral(" - ") + m_path;
    if (m_dirty)
        title += QStringLiteral(" *");
    setWindowTitle(title);
}
