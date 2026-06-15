#pragma once

#include <QFlags>
#include <QMetaType>

enum class DeviceState {
    None = 0x0,
    Initialized = 1,
    Connecting = 2,
    Disconnecting = 3,
    Reading = 4,
    Writing = 5,
    Connected = 0x10
};

Q_DECLARE_FLAGS(DeviceStates, DeviceState)
Q_DECLARE_OPERATORS_FOR_FLAGS(DeviceStates)
Q_DECLARE_METATYPE(DeviceStates)
