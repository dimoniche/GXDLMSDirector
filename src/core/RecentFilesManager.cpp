#include "RecentFilesManager.h"

#include <QFileInfo>
#include <QSettings>

RecentFilesManager &RecentFilesManager::instance()
{
    static RecentFilesManager manager;
    return manager;
}

void RecentFilesManager::load()
{
    QSettings settings(QStringLiteral("Gurux"), QStringLiteral("GXDLMSDirector"));
    m_files = settings.value(QStringLiteral("recentFiles")).toStringList();
}

void RecentFilesManager::save() const
{
    QSettings settings(QStringLiteral("Gurux"), QStringLiteral("GXDLMSDirector"));
    settings.setValue(QStringLiteral("recentFiles"), m_files);
}

void RecentFilesManager::add(const QString &path)
{
    const QString canonical = QFileInfo(path).absoluteFilePath();
    if (canonical.isEmpty())
        return;

    m_files.removeAll(canonical);
    m_files.prepend(canonical);
    while (m_files.size() > maxFiles)
        m_files.removeLast();
    save();
}

void RecentFilesManager::remove(const QString &path)
{
    m_files.removeAll(QFileInfo(path).absoluteFilePath());
    save();
}
