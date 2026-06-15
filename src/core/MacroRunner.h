#pragma once

#include "MacroStep.h"

#include <functional>

class GXDLMSDevice;

class MacroRunner
{
public:
    using ProgressCallback = std::function<void(int current, int total, const QString &message)>;

    static MacroRunResult run(GXDLMSDevice *device, const QVector<MacroStep> &steps,
                              ProgressCallback progress = {}, bool *cancelled = nullptr);
};
