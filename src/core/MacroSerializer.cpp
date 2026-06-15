#include "MacroSerializer.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

MacroActionType actionTypeFromString(const QString &text)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (ok)
        return static_cast<MacroActionType>(value);
    return MacroActionType::None;
}

QString actionTypeToString(MacroActionType type)
{
    return QString::number(static_cast<int>(type));
}

void readMacroElement(QXmlStreamReader &xml, MacroStep &step)
{
    while (xml.readNextStartElement()) {
        const QString tag = xml.name().toString();
        if (tag == QLatin1String("Timestamp"))
            step.timestamp = QDateTime::fromString(xml.readElementText(), Qt::ISODate);
        else if (tag == QLatin1String("Disable"))
            step.disabled = xml.readElementText() == QLatin1String("true");
        else if (tag == QLatin1String("Verify"))
            step.verify = xml.readElementText() == QLatin1String("true");
        else if (tag == QLatin1String("Type"))
            step.type = actionTypeFromString(xml.readElementText());
        else if (tag == QLatin1String("Name"))
            step.name = xml.readElementText();
        else if (tag == QLatin1String("Description"))
            step.description = xml.readElementText();
        else if (tag == QLatin1String("Device"))
            step.device = xml.readElementText();
        else if (tag == QLatin1String("ObjectType"))
            step.objectType = xml.readElementText().toInt();
        else if (tag == QLatin1String("ObjectVersion"))
            step.objectVersion = xml.readElementText().toInt();
        else if (tag == QLatin1String("LogicalName"))
            step.logicalName = xml.readElementText();
        else if (tag == QLatin1String("Index"))
            step.index = xml.readElementText().toInt();
        else if (tag == QLatin1String("Value"))
            step.value = xml.readElementText();
        else if (tag == QLatin1String("Data"))
            step.data = xml.readElementText();
        else if (tag == QLatin1String("Parameters"))
            step.parameters = xml.readElementText();
        else if (tag == QLatin1String("External"))
            step.external = xml.readElementText();
        else if (tag == QLatin1String("DataType"))
            step.dataType = xml.readElementText().toInt();
        else if (tag == QLatin1String("UIDataType"))
            step.uiDataType = xml.readElementText().toInt();
        else if (tag == QLatin1String("Exception"))
            step.expectedException = xml.readElementText();
        else
            xml.skipCurrentElement();
    }
}

void writeMacroElement(QXmlStreamWriter &xml, const MacroStep &step)
{
    xml.writeStartElement(QStringLiteral("GXMacro"));
    if (step.timestamp.isValid())
        xml.writeTextElement(QStringLiteral("Timestamp"), step.timestamp.toString(Qt::ISODate));
    if (step.disabled)
        xml.writeTextElement(QStringLiteral("Disable"), QStringLiteral("true"));
    if (step.verify)
        xml.writeTextElement(QStringLiteral("Verify"), QStringLiteral("true"));
    xml.writeTextElement(QStringLiteral("Type"), actionTypeToString(step.type));
    if (!step.name.isEmpty())
        xml.writeTextElement(QStringLiteral("Name"), step.name);
    if (!step.description.isEmpty())
        xml.writeTextElement(QStringLiteral("Description"), step.description);
    if (!step.device.isEmpty())
        xml.writeTextElement(QStringLiteral("Device"), step.device);
    if (step.objectType != 0)
        xml.writeTextElement(QStringLiteral("ObjectType"), QString::number(step.objectType));
    if (step.objectVersion != 0)
        xml.writeTextElement(QStringLiteral("ObjectVersion"), QString::number(step.objectVersion));
    if (!step.logicalName.isEmpty())
        xml.writeTextElement(QStringLiteral("LogicalName"), step.logicalName);
    if (step.index != 0)
        xml.writeTextElement(QStringLiteral("Index"), QString::number(step.index));
    if (!step.value.isEmpty())
        xml.writeTextElement(QStringLiteral("Value"), step.value);
    if (!step.data.isEmpty())
        xml.writeTextElement(QStringLiteral("Data"), step.data);
    if (!step.parameters.isEmpty())
        xml.writeTextElement(QStringLiteral("Parameters"), step.parameters);
    if (!step.external.isEmpty())
        xml.writeTextElement(QStringLiteral("External"), step.external);
    if (step.dataType != 0)
        xml.writeTextElement(QStringLiteral("DataType"), QString::number(step.dataType));
    if (step.uiDataType != 0)
        xml.writeTextElement(QStringLiteral("UIDataType"), QString::number(step.uiDataType));
    if (!step.expectedException.isEmpty())
        xml.writeTextElement(QStringLiteral("Exception"), step.expectedException);
    xml.writeEndElement();
}

} // namespace

bool MacroSerializer::load(const QString &path, QVector<MacroStep> &steps, QString *error)
{
    steps.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("GXMacro")) {
            MacroStep step;
            readMacroElement(xml, step);
            steps.append(step);
        }
    }

    if (xml.hasError()) {
        if (error)
            *error = xml.errorString();
        return false;
    }
    return true;
}

bool MacroSerializer::save(const QString &path, const QVector<MacroStep> &steps, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("ArrayOfGXMacro"));
    xml.writeDefaultNamespace(QStringLiteral("http://tempuri.org/"));
    for (const MacroStep &step : steps)
        writeMacroElement(xml, step);
    xml.writeEndElement();
    xml.writeEndDocument();
    return true;
}
