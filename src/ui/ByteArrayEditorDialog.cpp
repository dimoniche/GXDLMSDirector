#include "ByteArrayEditorDialog.h"

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

ByteArrayEditorDialog::ByteArrayEditorDialog(const QString &hexValue, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Octet String"));
    resize(500, 300);

    auto *layout = new QVBoxLayout(this);
    m_edit = new QPlainTextEdit(this);
    m_edit->setPlainText(hexValue);
    m_edit->setPlaceholderText(tr("Enter hex bytes..."));
    layout->addWidget(m_edit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString ByteArrayEditorDialog::hexValue() const
{
    return m_edit->toPlainText().remove(QLatin1Char(' ')).remove(QLatin1Char('\n'));
}
