#include "ConformanceHelper.h"

#include <TranslatorSimpleTags.h>

#include <QFile>
#include <QTextStream>

QString ConformanceHelper::conformanceToString(DLMS_CONFORMANCE value)
{
    std::string str;
    CTranslatorSimpleTags::ConformanceToString(value, str);
    return QString::fromStdString(str);
}

bool ConformanceHelper::writeHtmlReport(const QString &path, const QString &title, const QString &body,
                                        QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << "<html><head><meta charset=\"utf-8\"><title>" << title << "</title></head><body>\n";
    out << "<h1>" << title << "</h1>\n";
    out << body;
    out << "</body></html>\n";
    return true;
}
