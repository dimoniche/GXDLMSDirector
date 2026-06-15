#include "DataConcentratorManager.h"

DataConcentratorManager &DataConcentratorManager::instance()
{
    static DataConcentratorManager manager;
    return manager;
}

DataConcentratorManager::DataConcentratorManager() = default;

QStringList DataConcentratorManager::availablePlugins() const
{
    return {};
}
