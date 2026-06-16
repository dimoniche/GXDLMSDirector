#include "VariantConverter.h"

#include "ObjectAttributeNames.h"

#include <GXDLMSObject.h>
#include <GXDLMSVariant.h>
#include <GXDLMSValueEventArg.h>
#include <GXDLMSSettings.h>
#include <GXDateTime.h>
#include <GXHelpers.h>
#include <errorcodes.h>
#include <enums.h>

#include <QDateTime>

namespace VariantConverter {

DLMS_DATA_TYPE resolveType(CGXDLMSObject *object, int index)
{
    DLMS_DATA_TYPE type = DLMS_DATA_TYPE_NONE;
    if (object->GetDataType(index, type) != 0 || type == DLMS_DATA_TYPE_NONE)
        object->GetUIDataType(index, type);
    return type;
}

bool fromString(CGXDLMSObject *object, int attributeIndex, const QString &text, CGXDLMSVariant &value,
                QString *error)
{
    if (!object)
        return false;

    DLMS_DATA_TYPE type = DLMS_DATA_TYPE_NONE;
    type = resolveType(object, attributeIndex);
    if (type == DLMS_DATA_TYPE_NONE)
        type = DLMS_DATA_TYPE_STRING;

    value.Clear();
    const QString trimmed = text.trimmed();
    const std::string stdText = trimmed.toStdString();

    switch (type) {
    case DLMS_DATA_TYPE_BOOLEAN:
        value = trimmed.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
                || trimmed == QStringLiteral("1");
        return true;
    case DLMS_DATA_TYPE_INT8:
        value = static_cast<char>(trimmed.toInt());
        return true;
    case DLMS_DATA_TYPE_INT16:
        value = static_cast<short>(trimmed.toInt());
        return true;
    case DLMS_DATA_TYPE_INT32:
        value = trimmed.toInt();
        return true;
    case DLMS_DATA_TYPE_INT64:
        value = trimmed.toLongLong();
        return true;
    case DLMS_DATA_TYPE_UINT8:
        value = static_cast<unsigned char>(trimmed.toUInt());
        return true;
    case DLMS_DATA_TYPE_UINT16:
        value = static_cast<unsigned short>(trimmed.toUInt());
        return true;
    case DLMS_DATA_TYPE_UINT32:
        value = trimmed.toUInt();
        return true;
    case DLMS_DATA_TYPE_UINT64:
        value = trimmed.toULongLong();
        return true;
    case DLMS_DATA_TYPE_FLOAT32:
        value = trimmed.toFloat();
        return true;
    case DLMS_DATA_TYPE_FLOAT64:
        value = trimmed.toDouble();
        return true;
    case DLMS_DATA_TYPE_ENUM:
        value = static_cast<long>(trimmed.toInt());
        return true;
    case DLMS_DATA_TYPE_OCTET_STRING: {
        CGXByteBuffer bb;
        QString hex = trimmed;
        hex.remove(QLatin1Char(' '));
        if (hex.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
            hex = hex.mid(2);
        std::string hexStr = hex.toStdString();
        bb.SetHexString(hexStr);
        value = bb;
        return true;
    }
    case DLMS_DATA_TYPE_DATETIME:
    case DLMS_DATA_TYPE_DATE:
    case DLMS_DATA_TYPE_TIME: {
        const QDateTime dateTime = QDateTime::fromString(trimmed, Qt::ISODate);
        if (!dateTime.isValid()) {
            if (error)
                *error = QStringLiteral("Invalid date/time format. Use ISO format.");
            return false;
        }
        const QDateTime utc = dateTime.toUTC();
        struct tm tmValue {};
        tmValue.tm_year = utc.date().year() - 1900;
        tmValue.tm_mon = utc.date().month() - 1;
        tmValue.tm_mday = utc.date().day();
        tmValue.tm_hour = utc.time().hour();
        tmValue.tm_min = utc.time().minute();
        tmValue.tm_sec = utc.time().second();
        tmValue.tm_isdst = -1;
        CGXDateTime dt(tmValue);
        value = dt;
        return true;
    }
    case DLMS_DATA_TYPE_STRING:
    case DLMS_DATA_TYPE_STRING_UTF8:
    default:
        value = stdText;
        return true;
    }
}

QString toString(CGXDLMSObject *object, int attributeIndex, const CGXDLMSVariant &value)
{
    if (!object)
        return {};

    CGXDLMSVariant copy = value;
    const DLMS_DATA_TYPE type = copy.vt != DLMS_DATA_TYPE_NONE ? copy.vt : resolveType(object, attributeIndex);
    if (type == DLMS_DATA_TYPE_BOOLEAN)
        return copy.boolVal ? QStringLiteral("true") : QStringLiteral("false");
    return QString::fromStdString(copy.ToString());
}

QString attributeDisplayValue(CGXDLMSObject *object, int attributeIndex)
{
    if (!object || attributeIndex <= 0)
        return {};

    CGXDLMSSettings settings(false);
    CGXDLMSValueEventArg event(object, attributeIndex);
    if (object->GetValue(settings, event) == DLMS_ERROR_CODE_OK && event.GetError() == DLMS_ERROR_CODE_OK)
        return toString(object, attributeIndex, event.GetValue());

    std::vector<std::string> values;
    object->GetValues(values);
    const size_t index = static_cast<size_t>(attributeIndex - 1);
    if (index < values.size())
        return QString::fromStdString(values.at(index));

    return {};
}

QString dataTypeLabel(CGXDLMSObject *object, int attributeIndex)
{
    if (!object)
        return {};
    return ObjectAttributeNames::dataTypeName(resolveType(object, attributeIndex));
}

bool isWritable(CGXDLMSObject *object, int attributeIndex)
{
    if (!object)
        return false;
    const DLMS_ACCESS_MODE access = object->GetAccess(attributeIndex);
    return access == DLMS_ACCESS_MODE_WRITE || access == DLMS_ACCESS_MODE_READ_WRITE
           || access == DLMS_ACCESS_MODE_AUTHENTICATED_WRITE
           || access == DLMS_ACCESS_MODE_AUTHENTICATED_READ_WRITE;
}

} // namespace VariantConverter
