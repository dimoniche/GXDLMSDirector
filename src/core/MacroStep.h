#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

enum class MacroActionType {
    None = 0,
    Connect = 1,
    Disconnect = 2,
    Get = 3,
    Set = 4,
    Action = 5,
    Delay = 6
};

struct MacroStep
{
    QDateTime timestamp;
    bool disabled = false;
    bool verify = false;
    MacroActionType type = MacroActionType::None;
    QString name;
    QString description;
    QString device;
    int objectType = 0;
    int objectVersion = 0;
    QString logicalName;
    int index = 0;
    QString value;
    QString data;
    QString parameters;
    QString external;
    int dataType = 0;
    int uiDataType = 0;
    QString expectedException;
    QString lastException;
    QString lastResult;
};

struct MacroRunResult
{
    QVector<MacroStep> steps;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};
