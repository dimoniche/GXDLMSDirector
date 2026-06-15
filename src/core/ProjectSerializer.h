#pragma once

#include <QString>

class GXDLMSDevice;

class ProjectSerializer
{
public:
    static QString objectsFilePath(const QString &projectPath);

    static bool save(const QString &projectPath, GXDLMSDevice *device, QString *error = nullptr);
    static bool load(const QString &projectPath, GXDLMSDevice *device, QString *error = nullptr);
};
