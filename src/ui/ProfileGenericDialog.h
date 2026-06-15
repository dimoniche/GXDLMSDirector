#pragma once

#include "core/GXDLMSDevice.h"
#include "core/ProfileGenericResult.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ProfileGenericDialog; }
QT_END_NAMESPACE

class ProfileGenericDialog : public QDialog
{
    Q_OBJECT

public:
    ProfileGenericDialog(GXDLMSDevice *device, CGXDLMSObject *object, QWidget *parent = nullptr);
    ~ProfileGenericDialog() override;

private slots:
    void onRead();
    void onProfileGenericRead(CGXDLMSObject *object, const ProfileGenericResult &result);
    void onModeChanged(int index);

private:
    void showResult(const ProfileGenericResult &result);

    Ui::ProfileGenericDialog *ui;
    GXDLMSDevice *m_device;
    CGXDLMSObject *m_object;
};
