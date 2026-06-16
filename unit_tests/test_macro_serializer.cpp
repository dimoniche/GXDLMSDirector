#include "core/MacroSerializer.h"

#include <QTemporaryDir>
#include <QTest>

class TestMacroSerializer : public QObject
{
    Q_OBJECT

private slots:
    void roundTripMacroSteps();
};

void TestMacroSerializer::roundTripMacroSteps()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString path = tempDir.path() + QStringLiteral("/test.gxm");

    QVector<MacroStep> steps;
    MacroStep step;
    step.timestamp = QDateTime(QDate(2026, 6, 15), QTime(12, 30, 0), Qt::UTC);
    step.type = MacroActionType::Get;
    step.verify = true;
    step.name = QStringLiteral("Read clock");
    step.device = QStringLiteral("Meter 1");
    step.objectType = 8;
    step.logicalName = QStringLiteral("0.0.1.0.0.255");
    step.index = 2;
    step.value = QStringLiteral("42");
    steps.append(step);

    MacroStep delay;
    delay.type = MacroActionType::Delay;
    delay.data = QStringLiteral("1000");
    steps.append(delay);

    QString error;
    QVERIFY2(MacroSerializer::save(path, steps, &error), qPrintable(error));

    QVector<MacroStep> loaded;
    QVERIFY2(MacroSerializer::load(path, loaded, &error), qPrintable(error));
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).type, MacroActionType::Get);
    QCOMPARE(loaded.at(0).verify, true);
    QCOMPARE(loaded.at(0).logicalName, QStringLiteral("0.0.1.0.0.255"));
    QCOMPARE(loaded.at(0).index, 2);
    QCOMPARE(loaded.at(1).type, MacroActionType::Delay);
    QCOMPARE(loaded.at(1).data, QStringLiteral("1000"));
}

QTEST_MAIN(TestMacroSerializer)
#include "test_macro_serializer.moc"
