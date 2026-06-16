#pragma once

#include <QFlags>
#include <QMetaType>

enum class DeviceState {
    None = 0x0,
    Initialized = 0x01,
    Connecting = 0x02,
    Disconnecting = 0x04,
    Reading = 0x08,
    Writing = 0x10,
    Connected = 0x20
};

Q_DECLARE_FLAGS(DeviceStates, DeviceState)
Q_DECLARE_OPERATORS_FOR_FLAGS(DeviceStates)
Q_DECLARE_METATYPE(DeviceStates)
