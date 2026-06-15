#pragma once

#include <QDialog>

namespace Ui { class FindObjectDialog; }

class FindObjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindObjectDialog(QWidget *parent = nullptr);
    ~FindObjectDialog() override;

    QString searchText() const;
    QString logicalName() const;
    void setSearchText(const QString &text);
    void setLogicalName(const QString &logicalName);

private:
    Ui::FindObjectDialog *ui;
};
