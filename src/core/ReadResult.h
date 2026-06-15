#pragma once

#include <QMetaType>
#include <QString>

class CGXDLMSObject;

struct ReadResult {
    CGXDLMSObject *object = nullptr;
    int attributeIndex = 0;
    QString value;
    int errorCode = 0;
};

Q_DECLARE_METATYPE(ReadResult)
