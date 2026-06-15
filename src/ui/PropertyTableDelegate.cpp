#include "PropertyTableDelegate.h"

#include <GXDLMSObject.h>
#include <enums.h>

#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>

PropertyTableDelegate::PropertyTableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void PropertyTableDelegate::setObject(CGXDLMSObject *object)
{
    m_object = object;
}

QWidget *PropertyTableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    Q_UNUSED(option)

    if (!m_object || index.column() != 2)
        return QStyledItemDelegate::createEditor(parent, option, index);

    const int attributeIndex = index.row() + 1;
    DLMS_DATA_TYPE type = DLMS_DATA_TYPE_NONE;
    m_object->GetDataType(attributeIndex, type);
    if (type == DLMS_DATA_TYPE_NONE)
        m_object->GetUIDataType(attributeIndex, type);

    if (type == DLMS_DATA_TYPE_BOOLEAN) {
        auto *combo = new QComboBox(parent);
        combo->addItem(QStringLiteral("false"));
        combo->addItem(QStringLiteral("true"));
        return combo;
    }

    if (m_object->GetObjectType() == DLMS_OBJECT_TYPE_IEC_HDLC_SETUP && attributeIndex == 2) {
        auto *combo = new QComboBox(parent);
        combo->addItem(QStringLiteral("300"), static_cast<int>(DLMS_BAUD_RATE_300));
        combo->addItem(QStringLiteral("600"), static_cast<int>(DLMS_BAUD_RATE_600));
        combo->addItem(QStringLiteral("1200"), static_cast<int>(DLMS_BAUD_RATE_1200));
        combo->addItem(QStringLiteral("2400"), static_cast<int>(DLMS_BAUD_RATE_2400));
        combo->addItem(QStringLiteral("4800"), static_cast<int>(DLMS_BAUD_RATE_4800));
        combo->addItem(QStringLiteral("9600"), static_cast<int>(DLMS_BAUD_RATE_9600));
        combo->addItem(QStringLiteral("19200"), static_cast<int>(DLMS_BAUD_RATE_19200));
        combo->addItem(QStringLiteral("38400"), static_cast<int>(DLMS_BAUD_RATE_38400));
        combo->addItem(QStringLiteral("57600"), static_cast<int>(DLMS_BAUD_RATE_57600));
        combo->addItem(QStringLiteral("115200"), static_cast<int>(DLMS_BAUD_RATE_115200));
        return combo;
    }

    return new QLineEdit(parent);
}

void PropertyTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        const QString text = index.model()->data(index, Qt::EditRole).toString();
        const int idx = combo->findText(text);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
        else
            combo->setCurrentText(text);
        return;
    }

    QStyledItemDelegate::setEditorData(editor, index);
}

void PropertyTableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        model->setData(index, combo->currentText(), Qt::EditRole);
        return;
    }

    QStyledItemDelegate::setModelData(editor, model, index);
}
