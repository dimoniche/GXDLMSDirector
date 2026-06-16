#pragma once

class CGXDLMSObject;
class CGXDLMSVariant;

#include <QString>
#include <QVector>

namespace VariantConverter {
bool fromString(CGXDLMSObject *object, int attributeIndex, const QString &text, CGXDLMSVariant &value,
                QString *error = nullptr);
QString toString(CGXDLMSObject *object, int attributeIndex, const CGXDLMSVariant &value);
QString attributeDisplayValue(CGXDLMSObject *object, int attributeIndex);
QString dataTypeLabel(CGXDLMSObject *object, int attributeIndex);
bool isWritable(CGXDLMSObject *object, int attributeIndex);
}
