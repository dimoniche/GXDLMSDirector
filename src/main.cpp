#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>

#include "core/DeviceState.h"
#include "core/ManufacturerSettings.h"
#include "core/ProfileGenericResult.h"
#include "core/ReadResult.h"

#include <GXDLMSObject.h>

namespace {

bool ensureSingleInstance()
{
    const QString lockPath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .absoluteFilePath(QStringLiteral("GXDLMSDirector.lock"));

    static QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0);
    if (lockFile.tryLock(100))
        return true;

    QMessageBox::warning(
        nullptr,
        QStringLiteral("GXDLMSDirector"),
        QObject::tr("GXDLMSDirector is already running."));
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GXDLMSDirector"));
    QApplication::setOrganizationName(QStringLiteral("Gurux"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    qRegisterMetaType<CGXDLMSObject *>("CGXDLMSObject*");
    qRegisterMetaType<DeviceStates>("DeviceStates");
    qRegisterMetaType<ReadResult>("ReadResult");
    qRegisterMetaType<QList<ReadResult>>("QList<ReadResult>");
    qRegisterMetaType<ProfileGenericResult>("ProfileGenericResult");

    if (!ensureSingleInstance())
        return 1;

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + QStringLiteral("/GXDLMSDirector");
    QDir().mkpath(dataDir);
    QDir::setCurrent(dataDir);
    ManufacturerSettings::instance().save();

    MainWindow window;
    window.show();

    return app.exec();
}
