#pragma once

#include "core/MacroStep.h"
#include "core/GXDLMSDevice.h"

#include <QDialog>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class MacroEditorDialog; }
QT_END_NAMESPACE

class MacroEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MacroEditorDialog(GXDLMSDevice *device, QWidget *parent = nullptr);
    ~MacroEditorDialog() override;

    bool isRecording() const { return m_recording; }
    void addRecordedStep(const MacroStep &step);

signals:
    void recordingChanged(bool enabled);

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onRunAll();
    void onRunSelected();
    void onStop();
    void onAddDelay();
    void onRemove();
    void onToggleRecord(bool enabled);
    void onSelectionChanged();

private:
    void refreshList();
    void updateTitle();
    void runSteps(const QVector<MacroStep> &steps);
    QString typeLabel(const MacroStep &step) const;

    Ui::MacroEditorDialog *ui;
    GXDLMSDevice *m_device;
    QVector<MacroStep> m_steps;
    QString m_path;
    bool m_dirty = false;
    bool m_recording = false;
    bool m_cancelled = false;
};
