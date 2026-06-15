#pragma once

#include <QString>

#include <enums.h>

class ConformanceHelper
{
public:
    static QString conformanceToString(DLMS_CONFORMANCE value);
    static bool writeHtmlReport(const QString &path, const QString &title, const QString &body,
                                QString *error = nullptr);
};
