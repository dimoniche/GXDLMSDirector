#pragma once

#include "MacroStep.h"

#include <QString>
#include <QVector>

class MacroSerializer
{
public:
    static bool load(const QString &path, QVector<MacroStep> &steps, QString *error = nullptr);
    static bool save(const QString &path, const QVector<MacroStep> &steps, QString *error = nullptr);
};
