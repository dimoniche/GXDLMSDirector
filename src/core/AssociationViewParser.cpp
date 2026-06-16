#include "AssociationViewParser.h"

#include <enums.h>

namespace AssociationViewParser {

namespace {

void normalizeAccessMode(CGXDLMSVariant &mode)
{
    if (mode.vt == DLMS_DATA_TYPE_UINT8)
        mode.vt = DLMS_DATA_TYPE_ENUM;
}

void normalizeAttributeAccessItem(CGXDLMSVariant &attr)
{
    if (attr.vt != DLMS_DATA_TYPE_STRUCTURE)
        return;

    if (attr.Arr.size() == 2) {
        CGXDLMSVariant selectors;
        selectors.vt = DLMS_DATA_TYPE_NONE;
        attr.Arr.push_back(selectors);
    }

    if (attr.Arr.size() >= 2)
        normalizeAccessMode(attr.Arr[1]);
}

void normalizeMethodAccessItem(CGXDLMSVariant &method)
{
    if (method.vt != DLMS_DATA_TYPE_STRUCTURE || method.Arr.size() < 2)
        return;

    normalizeAccessMode(method.Arr[1]);
}

void normalizeAssociationObject(CGXDLMSVariant &object)
{
    if (object.vt != DLMS_DATA_TYPE_STRUCTURE || object.Arr.size() < 4)
        return;

    CGXDLMSVariant &access = object.Arr[3];
    if (access.vt != DLMS_DATA_TYPE_STRUCTURE || access.Arr.size() != 2)
        return;

    if (access.Arr[0].vt == DLMS_DATA_TYPE_ARRAY) {
        for (auto &attr : access.Arr[0].Arr)
            normalizeAttributeAccessItem(attr);
    }

    if (access.Arr[1].vt == DLMS_DATA_TYPE_ARRAY) {
        for (auto &method : access.Arr[1].Arr)
            normalizeMethodAccessItem(method);
    }
}

} // namespace

void normalizeObjects(std::vector<CGXDLMSVariant> &objects)
{
    for (auto &object : objects)
        normalizeAssociationObject(object);
}

} // namespace AssociationViewParser
