#include "ConformanceTestDialog.h"
#include "ui_ConformanceTestDialog.h"

#include "core/ConformanceHelper.h"
#include "core/MacroRunner.h"
#include "core/MacroSerializer.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

ConformanceTestDialog::ConformanceTestDialog(GXDLMSDevice *device, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConformanceTestDialog)
    , m_device(device)
{
    ui->setupUi(this);

    ui->proposedEdit->setText(m_device->proposedConformance());
    ui->negotiatedEdit->setText(m_device->negotiatedConformance());

    connect(ui->browseButton, &QPushButton::clicked, this, &ConformanceTestDialog::onBrowse);
    connect(ui->runButton, &QPushButton::clicked, this, &ConformanceTestDialog::onRun);
    connect(ui->openReportButton, &QPushButton::clicked, this, &ConformanceTestDialog::onOpenReport);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

ConformanceTestDialog::~ConformanceTestDialog()
{
    delete ui;
}

void ConformanceTestDialog::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Test Macro"), ui->fileEdit->text(),
                                                      tr("Macro files (*.gxm)"));
    if (!path.isEmpty())
        ui->fileEdit->setText(path);
}

void ConformanceTestDialog::onRun()
{
    const QString path = ui->fileEdit->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Conformance Tests"), tr("Select a .gxm test file."));
        return;
    }

    QVector<MacroStep> steps;
    QString error;
    if (!MacroSerializer::load(path, steps, &error)) {
        QMessageBox::warning(this, tr("Conformance Tests"), error);
        return;
    }

    for (MacroStep &step : steps)
        step.verify = true;

    ui->runButton->setEnabled(false);
    ui->logEdit->clear();

    auto *watcher = new QFutureWatcher<MacroRunResult>(this);
    connect(watcher, &QFutureWatcher<MacroRunResult>::finished, this, [this, watcher, path]() {
        const MacroRunResult result = watcher->result();
        ui->runButton->setEnabled(true);
        ui->logEdit->appendPlainText(tr("Passed: %1, Failed: %2, Skipped: %3")
                                         .arg(result.passed)
                                         .arg(result.failed)
                                         .arg(result.skipped));

        QString body = QStringLiteral("<p>Test file: %1</p><table border=\"1\" cellpadding=\"4\">")
                           .arg(path.toHtmlEscaped());
        body += QStringLiteral("<tr><th>#</th><th>Name</th><th>Result</th></tr>");
        for (int i = 0; i < result.steps.size(); ++i) {
            const MacroStep &step = result.steps.at(i);
            const QString status = step.lastException.isEmpty() ? QStringLiteral("PASS") : step.lastException;
            body += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                        .arg(i + 1)
                        .arg(step.name.toHtmlEscaped())
                        .arg(status.toHtmlEscaped());
        }
        body += QStringLiteral("</table>");

        const QString reportDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                  + QStringLiteral("/GXDLMSDirector/Conformance");
        QDir().mkpath(reportDir);
        m_lastReportPath = reportDir + QStringLiteral("/Results_")
                           + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
                           + QStringLiteral(".html");

        QString reportError;
        if (ConformanceHelper::writeHtmlReport(m_lastReportPath, tr("Conformance Test Results"), body,
                                               &reportError)) {
            ui->openReportButton->setEnabled(true);
            ui->logEdit->appendPlainText(tr("Report saved: %1").arg(m_lastReportPath));
        } else {
            ui->logEdit->appendPlainText(tr("Failed to save report: %1").arg(reportError));
        }

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
            });
    }));
}

void ConformanceTestDialog::onOpenReport()
{
    if (!m_lastReportPath.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastReportPath));
}
