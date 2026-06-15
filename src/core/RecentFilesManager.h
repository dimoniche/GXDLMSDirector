#pragma once

#include <QString>
#include <QStringList>

class RecentFilesManager
{
public:
    static RecentFilesManager &instance();

    QStringList files() const { return m_files; }
    void add(const QString &path);
    void remove(const QString &path);
    void load();
    void save() const;

    static constexpr int maxFiles = 10;

private:
    RecentFilesManager() = default;

    QStringList m_files;
};
