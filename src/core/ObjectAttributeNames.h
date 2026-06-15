#pragma once

#include <QString>

#include <enums.h>

namespace ObjectAttributeNames {
QString attributeName(DLMS_OBJECT_TYPE type, int index);
QString methodName(DLMS_OBJECT_TYPE type, int index);
QString dataTypeName(DLMS_DATA_TYPE type);
QStringList allObjectTypeLabels();
QList<DLMS_OBJECT_TYPE> allObjectTypes();
} // namespace ObjectAttributeNames
