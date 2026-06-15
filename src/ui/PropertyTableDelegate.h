#pragma once

#include <QStyledItemDelegate>

class CGXDLMSObject;

class PropertyTableDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit PropertyTableDelegate(QObject *parent = nullptr);

    void setObject(CGXDLMSObject *object);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private:
    CGXDLMSObject *m_object = nullptr;
};
