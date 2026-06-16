#include "core/AssociationViewParser.h"

#include <GXDLMSVariant.h>
#include <enums.h>

#include <QTest>

class TestAssociationViewParser : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAccessModeFromUint8ToEnum();
    void addsMissingAttributeSelectors();
};

static CGXDLMSVariant makeAccessMode(DLMS_DATA_TYPE type)
{
    CGXDLMSVariant mode;
    mode.vt = type;
    mode.uiVal = 3;
    return mode;
}

static CGXDLMSVariant makeAttributeAccessItem(int attributeIndex, DLMS_DATA_TYPE modeType, int selectorCount)
{
    CGXDLMSVariant item;
    item.vt = DLMS_DATA_TYPE_STRUCTURE;

    CGXDLMSVariant index;
    index = static_cast<long>(attributeIndex);
    item.Arr.push_back(index);
    item.Arr.push_back(makeAccessMode(modeType));

    for (int i = 0; i < selectorCount; ++i) {
        CGXDLMSVariant selector;
        selector = static_cast<long>(i);
        item.Arr.push_back(selector);
    }
    return item;
}

static CGXDLMSVariant makeAssociationObject(const CGXDLMSVariant &attributeAccessList)
{
    CGXDLMSVariant object;
    object.vt = DLMS_DATA_TYPE_STRUCTURE;

    CGXDLMSVariant classId;
    classId = static_cast<long>(1);
    object.Arr.push_back(classId);

    CGXDLMSVariant version;
    version = static_cast<long>(0);
    object.Arr.push_back(version);

    CGXDLMSVariant ln;
    ln = std::string("0.0.0.1.0.255");
    object.Arr.push_back(ln);

    CGXDLMSVariant access;
    access.vt = DLMS_DATA_TYPE_STRUCTURE;

    CGXDLMSVariant attributes;
    attributes.vt = DLMS_DATA_TYPE_ARRAY;
    attributes.Arr.push_back(attributeAccessList);
    access.Arr.push_back(attributes);

    CGXDLMSVariant methods;
    methods.vt = DLMS_DATA_TYPE_ARRAY;
    access.Arr.push_back(methods);

    object.Arr.push_back(access);
    return object;
}

void TestAssociationViewParser::normalizesAccessModeFromUint8ToEnum()
{
    std::vector<CGXDLMSVariant> objects;
    objects.push_back(makeAssociationObject(makeAttributeAccessItem(2, DLMS_DATA_TYPE_UINT8, 1)));

    AssociationViewParser::normalizeObjects(objects);

    const CGXDLMSVariant &access = objects.front().Arr.at(3);
    const CGXDLMSVariant &attributes = access.Arr.at(0);
    const CGXDLMSVariant &attribute = attributes.Arr.at(0);
    QCOMPARE(attribute.Arr.at(1).vt, DLMS_DATA_TYPE_ENUM);
}

void TestAssociationViewParser::addsMissingAttributeSelectors()
{
    std::vector<CGXDLMSVariant> objects;
    objects.push_back(makeAssociationObject(makeAttributeAccessItem(2, DLMS_DATA_TYPE_UINT8, 0)));

    AssociationViewParser::normalizeObjects(objects);

    const CGXDLMSVariant &access = objects.front().Arr.at(3);
    const CGXDLMSVariant &attributes = access.Arr.at(0);
    const CGXDLMSVariant &attribute = attributes.Arr.at(0);
    QCOMPARE(attribute.Arr.size(), static_cast<size_t>(3));
    QCOMPARE(attribute.Arr.at(2).vt, DLMS_DATA_TYPE_NONE);
}

QTEST_MAIN(TestAssociationViewParser)
#include "test_association_view_parser.moc"
