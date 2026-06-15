#pragma once

#include <QString>
#include <QVector>

class GXDLMSDevice;
class GXDLMSProject;

class ProjectSerializer
{
public:
    static QString objectsFilePath(const QString &projectPath);
    static QString objectsFilePath(const QString &projectPath, const GXDLMSDevice *device, int deviceIndex);

    static bool save(const QString &projectPath, GXDLMSProject *project, QString *error = nullptr);
    static bool load(const QString &projectPath, GXDLMSProject *project, QString *error = nullptr);

    // Backward-compatible single-device helpers
    static bool save(const QString &projectPath, GXDLMSDevice *device, QString *error = nullptr);
    static bool load(const QString &projectPath, GXDLMSDevice *device, QString *error = nullptr);
};
