#pragma once

#include <QString>
#include <QStringList>

class DataConcentratorManager
{
public:
    static DataConcentratorManager &instance();

    QStringList availablePlugins() const;
    bool hasPlugins() const { return !availablePlugins().isEmpty(); }

private:
    DataConcentratorManager();
};
