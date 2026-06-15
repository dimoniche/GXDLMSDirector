#pragma once

#include <QDialog>

class ByteArrayEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ByteArrayEditorDialog(const QString &hexValue, QWidget *parent = nullptr);

    QString hexValue() const;

private:
    class QPlainTextEdit *m_edit = nullptr;
};
