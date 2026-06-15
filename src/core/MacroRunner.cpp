#include "MacroRunner.h"

#include "GXDLMSCommunicator.h"
#include "GXDLMSDevice.h"
#include "VariantConverter.h"

#include <GXDLMSObjectFactory.h>
#include <enums.h>

#include <QThread>

#include <memory>

namespace {

CGXDLMSObject *findObjectInCollection(GXDLMSDevice *device, const MacroStep &step)
{
    for (auto *obj : device->objects()) {
        if (static_cast<int>(obj->GetObjectType()) == step.objectType) {
            std::string ln;
            obj->GetLogicalName(ln);
            if (QString::fromStdString(ln) == step.logicalName)
                return obj;
        }
    }
    return nullptr;
}

CGXDLMSObject *createMacroObject(const MacroStep &step)
{
    CGXDLMSObject *created =
        CGXDLMSObjectFactory::CreateObject(static_cast<DLMS_OBJECT_TYPE>(step.objectType),
                                             step.logicalName.toStdString());
    if (!created)
        return nullptr;

    created->SetVersion(static_cast<unsigned char>(step.objectVersion));
    return created;
}

void verifyStep(const MacroStep &original, MacroStep &executed)
{
    if (!original.verify)
        return;

    if (!original.expectedException.isEmpty()) {
        if (executed.lastException != original.expectedException) {
            executed.lastException = executed.lastException.isEmpty()
                                         ? QStringLiteral("Error expected but meter didn't return an error.")
                                         : QStringLiteral("Different exception: ") + executed.lastException;
        }
        return;
    }

    if (original.type == MacroActionType::Get && executed.lastResult != original.data)
        executed.lastException = QStringLiteral("Different data.");
}

} // namespace

MacroRunResult MacroRunner::run(GXDLMSDevice *device, const QVector<MacroStep> &steps,
                                ProgressCallback progress, bool *cancelled)
{
    MacroRunResult result;
    const int total = steps.size();

    for (int i = 0; i < steps.size(); ++i) {
        if (cancelled && *cancelled)
            break;

        MacroStep executed = steps.at(i);
        executed.lastException.clear();
        executed.lastResult.clear();

        if (progress) {
            progress(i + 1, total,
                     executed.name.isEmpty() ? QStringLiteral("Step %1").arg(i + 1) : executed.name);
        }

        if (executed.disabled) {
            ++result.skipped;
            result.steps.append(executed);
            continue;
        }

        GXDLMSCommunicator *comm = device->communicator();
        int ret = 0;

        switch (executed.type) {
        case MacroActionType::Connect:
            ret = comm->initializeConnection();
            if (ret == 0)
                ret = comm->getAssociationView();
            if (ret != 0)
                executed.lastException = QString::number(ret);
            break;
        case MacroActionType::Disconnect:
            comm->close();
            break;
        case MacroActionType::Delay:
            QThread::msleep(qMax(0, executed.value.toInt()));
            break;
        case MacroActionType::Get: {
            CGXDLMSObject *obj = findObjectInCollection(device, executed);
            std::unique_ptr<CGXDLMSObject> owned;
            if (!obj) {
                owned.reset(createMacroObject(executed));
                obj = owned.get();
            }
            if (!obj) {
                executed.lastException = QStringLiteral("Object not found.");
                break;
            }
            QString value;
            ret = comm->read(obj, executed.index, value);
            if (ret != 0) {
                executed.lastException = QString::number(ret);
            } else {
                executed.lastResult = value;
                executed.value = value;
            }
            break;
        }
        case MacroActionType::Set: {
            CGXDLMSObject *obj = findObjectInCollection(device, executed);
            std::unique_ptr<CGXDLMSObject> owned;
            if (!obj) {
                owned.reset(createMacroObject(executed));
                obj = owned.get();
            }
            if (!obj) {
                executed.lastException = QStringLiteral("Object not found.");
                break;
            }
            const QString writeValue = !executed.value.isEmpty() ? executed.value : executed.data;
            ret = comm->write(obj, executed.index, writeValue, &executed.lastException);
            if (ret != 0 && executed.lastException.isEmpty())
                executed.lastException = QString::number(ret);
            break;
        }
        case MacroActionType::Action: {
            CGXDLMSObject *obj = findObjectInCollection(device, executed);
            std::unique_ptr<CGXDLMSObject> owned;
            if (!obj) {
                owned.reset(createMacroObject(executed));
                obj = owned.get();
            }
            if (!obj) {
                executed.lastException = QStringLiteral("Object not found.");
                break;
            }
            CGXDLMSVariant variant;
            if (!executed.value.isEmpty()) {
                if (!VariantConverter::fromString(obj, executed.index, executed.value, variant,
                                                  &executed.lastException)) {
                    break;
                }
            }
            ret = comm->method(obj, executed.index, variant);
            if (ret != 0)
                executed.lastException = QString::number(ret);
            break;
        }
        default:
            executed.lastException = QStringLiteral("Unsupported action type.");
            break;
        }

        verifyStep(steps.at(i), executed);
        if (executed.lastException.isEmpty())
            ++result.passed;
        else
            ++result.failed;

        result.steps.append(executed);
    }

    return result;
}
