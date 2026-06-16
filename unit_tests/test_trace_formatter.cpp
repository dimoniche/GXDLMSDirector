#include "core/TraceFormatter.h"

#include <QTest>

class TestTraceFormatter : public QObject
{
    Q_OBJECT

private slots:
    void hexPacketFormatting();
    void emptyDataReturnsEmpty();
    void noneModeReturnsEmpty();
};

void TestTraceFormatter::hexPacketFormatting()
{
    const QByteArray data = QByteArray::fromHex("0102AB");
    const QString formatted = TraceFormatter::formatPacket(QStringLiteral("TX"),
                                                           data,
                                                           TraceDisplayMode::Hex,
                                                           false);
    QVERIFY(formatted.startsWith(QStringLiteral("TX: ")));
    QVERIFY(formatted.contains(QStringLiteral("01 02 AB")));
}

void TestTraceFormatter::emptyDataReturnsEmpty()
{
    QCOMPARE(TraceFormatter::formatPacket(QStringLiteral("RX"),
                                          QByteArray(),
                                          TraceDisplayMode::Hex,
                                          false),
             QString());
}

void TestTraceFormatter::noneModeReturnsEmpty()
{
    const QByteArray data = QByteArray::fromHex("01");
    QCOMPARE(TraceFormatter::formatPacket(QStringLiteral("TX"),
                                          data,
                                          TraceDisplayMode::None,
                                          false),
             QString());
}

QTEST_MAIN(TestTraceFormatter)
#include "test_trace_formatter.moc"
