#include "ValuesSerializer.h"
#include "GXDLMSDevice.h"

#include <GXDLMSObjectCollection.h>

#include <QFile>

namespace ValuesSerializer {

bool saveValues(const QString &path, GXDLMSDevice *device, QString *error)
{
    if (!device) {
        if (error)
            *error = QStringLiteral("No device");
        return false;
    }
    if (device->objects().empty()) {
        if (error)
            *error = QStringLiteral("No COSEM objects to save");
        return false;
    }

    const int ret = device->objects().Save(path.toStdString().c_str());
    if (ret != 0) {
        if (error)
            *error = QStringLiteral("Failed to save values (%1)").arg(ret);
        return false;
    }
    return true;
}

bool loadValues(const QString &path, GXDLMSDevice *device, QString *error)
{
    if (!device) {
        if (error)
            *error = QStringLiteral("No device");
        return false;
    }
    if (!QFile::exists(path)) {
        if (error)
            *error = QStringLiteral("File not found");
        return false;
    }

    device->objects().Free();
    const int ret = device->objects().Load(path.toStdString().c_str());
    if (ret != 0) {
        if (error)
            *error = QStringLiteral("Failed to load values (%1)").arg(ret);
        return false;
    }
    return true;
}

} // namespace ValuesSerializer
