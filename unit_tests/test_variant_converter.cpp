#include "core/VariantConverter.h"

#include <GXDLMSData.h>
#include <GXDLMSVariant.h>
#include <GXByteBuffer.h>
#include <enums.h>

#include <QTest>

class TestVariantConverter : public QObject
{
    Q_OBJECT

private slots:
    void booleanFromString();
    void octetStringFromHex();
    void dataTypeLabelForDataObject();
    void integerRoundTrip();
    void booleanToString();
    void invalidDateTimeRejected();
};

static CGXDLMSData makeDataObject()
{
    return CGXDLMSData(std::string("0.0.96.14.0.255"));
}

static CGXDLMSData makeTypedDataObject(DLMS_DATA_TYPE type)
{
    CGXDLMSData object = makeDataObject();
    object.SetDataType(2, type);
    return object;
}

void TestVariantConverter::booleanFromString()
{
    CGXDLMSData object = makeDataObject();
    object.SetDataType(2, DLMS_DATA_TYPE_BOOLEAN);

    CGXDLMSVariant value;
    QVERIFY(VariantConverter::fromString(&object, 2, QStringLiteral("true"), value));
    QCOMPARE(value.boolVal, true);

    QVERIFY(VariantConverter::fromString(&object, 2, QStringLiteral("0"), value));
    QCOMPARE(value.boolVal, false);
}

void TestVariantConverter::octetStringFromHex()
{
    CGXDLMSData object = makeDataObject();
    object.SetDataType(2, DLMS_DATA_TYPE_OCTET_STRING);

    CGXDLMSVariant value;
    QVERIFY(VariantConverter::fromString(&object, 2, QStringLiteral("0x0102AB"), value));

    CGXByteBuffer buffer;
    buffer.Set(value.byteArr, value.size);
    QCOMPARE(buffer.GetSize(), static_cast<unsigned long>(3));
    QCOMPARE(buffer.GetData()[0], static_cast<unsigned char>(0x01));
    QCOMPARE(buffer.GetData()[2], static_cast<unsigned char>(0xAB));
}

void TestVariantConverter::dataTypeLabelForDataObject()
{
    CGXDLMSData object = makeDataObject();
    object.SetDataType(2, DLMS_DATA_TYPE_UINT32);
    QCOMPARE(VariantConverter::dataTypeLabel(&object, 2), QStringLiteral("UInt32"));
}

void TestVariantConverter::integerRoundTrip()
{
    CGXDLMSData object = makeTypedDataObject(DLMS_DATA_TYPE_INT32);

    CGXDLMSVariant value;
    QVERIFY(VariantConverter::fromString(&object, 2, QStringLiteral("-12345"), value));
    QCOMPARE(value.lVal, static_cast<long>(-12345));
    QCOMPARE(value.vt, DLMS_DATA_TYPE_INT32);
}

void TestVariantConverter::booleanToString()
{
    CGXDLMSData object = makeTypedDataObject(DLMS_DATA_TYPE_BOOLEAN);

    CGXDLMSVariant value;
    value = true;
    QCOMPARE(VariantConverter::toString(&object, 2, value), QStringLiteral("true"));

    value = false;
    QCOMPARE(VariantConverter::toString(&object, 2, value), QStringLiteral("false"));
}

void TestVariantConverter::invalidDateTimeRejected()
{
    CGXDLMSData object = makeTypedDataObject(DLMS_DATA_TYPE_DATETIME);

    CGXDLMSVariant value;
    QString error;
    QVERIFY(!VariantConverter::fromString(&object, 2, QStringLiteral("not-a-date"), value, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestVariantConverter)
#include "test_variant_converter.moc"
