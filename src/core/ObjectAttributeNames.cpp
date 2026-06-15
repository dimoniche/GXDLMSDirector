#include "ObjectAttributeNames.h"

#include <GXDLMSConverter.h>

#include <QMap>

namespace {

QString fromMap(const QMap<int, QString> &map, int index, const QString &fallback)
{
    return map.value(index, fallback);
}

QMap<int, QString> clockAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Time")},
            {3, QStringLiteral("Time zone")},   {4, QStringLiteral("Status")},
            {5, QStringLiteral("Daylight savings begin")}, {6, QStringLiteral("Daylight savings end")},
            {7, QStringLiteral("Daylight savings deviation")}, {8, QStringLiteral("Daylight savings enabled")},
            {9, QStringLiteral("Clock base")},  {10, QStringLiteral("Clock status")}};
}

QMap<int, QString> registerAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Value")},
            {3, QStringLiteral("Scaler unit")}};
}

QMap<int, QString> dataAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Value")}};
}

QMap<int, QString> hdlcAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Comm speed")},
            {3, QStringLiteral("Window size transmit")}, {4, QStringLiteral("Window size receive")},
            {5, QStringLiteral("Max info length transmit")}, {6, QStringLiteral("Max info length receive")},
            {7, QStringLiteral("Inter octet time out")}, {8, QStringLiteral("Inactivity time out")},
            {9, QStringLiteral("Device address")}};
}

QMap<int, QString> disconnectAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Output state")},
            {3, QStringLiteral("Control state")},   {4, QStringLiteral("Control mode")}};
}

QMap<int, QString> profileGenericAttributes()
{
    return {{1, QStringLiteral("Logical name")}, {2, QStringLiteral("Buffer")},
            {3, QStringLiteral("Capture objects")}, {4, QStringLiteral("Capture period")},
            {5, QStringLiteral("Sort method")},   {6, QStringLiteral("Sort object")},
            {7, QStringLiteral("Entries in use")}, {8, QStringLiteral("Profile entries")}};
}

QMap<int, QString> disconnectMethods()
{
    return {{1, QStringLiteral("Remote disconnect")}, {2, QStringLiteral("Remote reconnect")},
            {3, QStringLiteral("Remote local disconnect")}, {4, QStringLiteral("Remote local reconnect")}};
}

} // namespace

namespace ObjectAttributeNames {

QString attributeName(DLMS_OBJECT_TYPE type, int index)
{
    const QString fallback = QStringLiteral("Attribute %1").arg(index);
    switch (type) {
    case DLMS_OBJECT_TYPE_CLOCK:
        return fromMap(clockAttributes(), index, fallback);
    case DLMS_OBJECT_TYPE_REGISTER:
    case DLMS_OBJECT_TYPE_EXTENDED_REGISTER:
    case DLMS_OBJECT_TYPE_DEMAND_REGISTER:
        return fromMap(registerAttributes(), index, fallback);
    case DLMS_OBJECT_TYPE_DATA:
        return fromMap(dataAttributes(), index, fallback);
    case DLMS_OBJECT_TYPE_IEC_HDLC_SETUP:
        return fromMap(hdlcAttributes(), index, fallback);
    case DLMS_OBJECT_TYPE_DISCONNECT_CONTROL:
        return fromMap(disconnectAttributes(), index, fallback);
    case DLMS_OBJECT_TYPE_PROFILE_GENERIC:
        return fromMap(profileGenericAttributes(), index, fallback);
    default:
        return fallback;
    }
}

QString methodName(DLMS_OBJECT_TYPE type, int index)
{
    const QString fallback = QStringLiteral("Method %1").arg(index);
    switch (type) {
    case DLMS_OBJECT_TYPE_DISCONNECT_CONTROL:
        return fromMap(disconnectMethods(), index, fallback);
    default:
        return fallback;
    }
}

QString dataTypeName(DLMS_DATA_TYPE type)
{
    switch (type) {
    case DLMS_DATA_TYPE_NONE:
        return QStringLiteral("None");
    case DLMS_DATA_TYPE_ARRAY:
        return QStringLiteral("Array");
    case DLMS_DATA_TYPE_STRUCTURE:
        return QStringLiteral("Structure");
    case DLMS_DATA_TYPE_BOOLEAN:
        return QStringLiteral("Boolean");
    case DLMS_DATA_TYPE_INT8:
        return QStringLiteral("Int8");
    case DLMS_DATA_TYPE_INT16:
        return QStringLiteral("Int16");
    case DLMS_DATA_TYPE_INT32:
        return QStringLiteral("Int32");
    case DLMS_DATA_TYPE_INT64:
        return QStringLiteral("Int64");
    case DLMS_DATA_TYPE_UINT8:
        return QStringLiteral("UInt8");
    case DLMS_DATA_TYPE_UINT16:
        return QStringLiteral("UInt16");
    case DLMS_DATA_TYPE_UINT32:
        return QStringLiteral("UInt32");
    case DLMS_DATA_TYPE_UINT64:
        return QStringLiteral("UInt64");
    case DLMS_DATA_TYPE_ENUM:
        return QStringLiteral("Enum");
    case DLMS_DATA_TYPE_FLOAT32:
        return QStringLiteral("Float32");
    case DLMS_DATA_TYPE_FLOAT64:
        return QStringLiteral("Float64");
    case DLMS_DATA_TYPE_OCTET_STRING:
        return QStringLiteral("OctetString");
    case DLMS_DATA_TYPE_STRING:
        return QStringLiteral("String");
    case DLMS_DATA_TYPE_STRING_UTF8:
        return QStringLiteral("StringUtf8");
    case DLMS_DATA_TYPE_DATETIME:
        return QStringLiteral("DateTime");
    case DLMS_DATA_TYPE_DATE:
        return QStringLiteral("Date");
    case DLMS_DATA_TYPE_TIME:
        return QStringLiteral("Time");
    default:
        return QStringLiteral("Type %1").arg(static_cast<int>(type));
    }
}

QStringList allObjectTypeLabels()
{
    QStringList labels;
    for (int i = 0; i <= 200; ++i) {
        const auto type = static_cast<DLMS_OBJECT_TYPE>(i);
        const char *name = CGXDLMSConverter::ToString(type);
        if (!name || QString(name) == QStringLiteral("None"))
            continue;
        labels.append(QStringLiteral("%1 (%2)").arg(QString::fromUtf8(name)).arg(i));
    }
    return labels;
}

QList<DLMS_OBJECT_TYPE> allObjectTypes()
{
    QList<DLMS_OBJECT_TYPE> types;
    for (int i = 1; i <= 200; ++i) {
        const auto type = static_cast<DLMS_OBJECT_TYPE>(i);
        const char *name = CGXDLMSConverter::ToString(type);
        if (!name || QString(name) == QStringLiteral("None"))
            continue;
        types.append(type);
    }
    return types;
}

} // namespace ObjectAttributeNames
