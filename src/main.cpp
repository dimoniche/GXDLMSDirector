#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QSharedMemory>
#include <QStandardPaths>

#include "core/DeviceState.h"
#include "core/ManufacturerSettings.h"
#include "core/ProfileGenericResult.h"
#include "core/ReadResult.h"

#include <GXDLMSObject.h>

namespace {
constexpr auto APP_ID = "GXDLMSDirector-Qt6";
}

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

    QSharedMemory singleInstance(APP_ID);
    if (!singleInstance.create(1)) {
        return 0;
    }

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + QStringLiteral("/GXDLMSDirector");
    QDir().mkpath(dataDir);
    QDir::setCurrent(dataDir);
    ManufacturerSettings::instance().save();

    MainWindow window;
    window.show();

    return app.exec();
}
