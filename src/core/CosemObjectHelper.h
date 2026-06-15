#pragma once

class CGXDLMSObject;
class CGXDLMSObjectCollection;

#include <QString>

#include <enums.h>

namespace CosemObjectHelper {
bool parseLogicalName(const QString &text, std::string &logicalName, QString *error = nullptr);
CGXDLMSObject *addObject(CGXDLMSObjectCollection &objects, DLMS_OBJECT_TYPE type,
                         const QString &logicalName, int version, const QString &description,
                         QString *error = nullptr);
bool removeObject(CGXDLMSObjectCollection &objects, CGXDLMSObject *object);
} // namespace CosemObjectHelper
