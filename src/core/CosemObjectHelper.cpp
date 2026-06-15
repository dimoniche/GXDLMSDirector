#include "CosemObjectHelper.h"

#include <GXDLMSObjectCollection.h>
#include <GXDLMSObjectFactory.h>

#include <QRegularExpression>

namespace CosemObjectHelper {

bool parseLogicalName(const QString &text, std::string &logicalName, QString *error)
{
    const QRegularExpression pattern(QStringLiteral(R"(^\s*(\d+)\s*\.\s*(\d+)\s*\.\s*(\d+)\s*\.\s*(\d+)\s*\.\s*(\d+)\s*\.\s*(\d+)\s*$)"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        if (error)
            *error = QStringLiteral("Logical name must be in format a.b.c.d.e.f");
        return false;
    }

    logicalName = match.captured(1).toStdString() + '.' + match.captured(2).toStdString() + '.'
                  + match.captured(3).toStdString() + '.' + match.captured(4).toStdString() + '.'
                  + match.captured(5).toStdString() + '.' + match.captured(6).toStdString();
    return true;
}

CGXDLMSObject *addObject(CGXDLMSObjectCollection &objects, DLMS_OBJECT_TYPE type,
                         const QString &logicalName, int version, const QString &description,
                         QString *error)
{
    std::string ln;
    if (!parseLogicalName(logicalName, ln, error))
        return nullptr;

    if (objects.FindByLN(type, ln) != nullptr) {
        if (error)
            *error = QStringLiteral("Object with this type and logical name already exists.");
        return nullptr;
    }

    CGXDLMSObject *object = CGXDLMSObjectFactory::CreateObject(type, ln);
    if (!object) {
        if (error)
            *error = QStringLiteral("Failed to create object.");
        return nullptr;
    }

    object->SetVersion(static_cast<unsigned char>(version));
    if (!description.isEmpty()) {
        std::string desc = description.toStdString();
        object->SetDescription(desc);
    }

    objects.push_back(object);
    return object;
}

bool removeObject(CGXDLMSObjectCollection &objects, CGXDLMSObject *object)
{
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (*it == object) {
            delete *it;
            objects.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace CosemObjectHelper
