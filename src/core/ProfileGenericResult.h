#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct ProfileGenericRow
{
    QStringList columns;
};

struct ProfileGenericResult
{
    QStringList columnHeaders;
    QVector<ProfileGenericRow> rows;
    int errorCode = 0;
    QString errorMessage;
};

Q_DECLARE_METATYPE(ProfileGenericResult)
