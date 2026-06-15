#pragma once

#include <QString>

class GXDLMSDevice;

namespace ValuesSerializer {
bool saveValues(const QString &path, GXDLMSDevice *device, QString *error = nullptr);
bool loadValues(const QString &path, GXDLMSDevice *device, QString *error = nullptr);
}
