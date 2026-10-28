#include <QTest>

#include "core/formatting.h"

using namespace tm_linux;

class FormattingTest : public QObject {
    Q_OBJECT

private slots:
    void bytesPerSecondAuto()
    {
        QCOMPARE(QString::fromStdString(formatBytesPerSecond(0, UnitMode::Auto, UnitBase::Byte)),
                 QStringLiteral("0 B/s"));
        QCOMPARE(QString::fromStdString(formatBytesPerSecond(1536, UnitMode::Auto, UnitBase::Byte)),
                 QStringLiteral("1.5 KB/s"));
        QCOMPARE(QString::fromStdString(
                     formatBytesPerSecond(1024.0 * 1024.0, UnitMode::Auto, UnitBase::Byte)),
                 QStringLiteral("1.0 MB/s"));
    }

    void bytesPerSecondFixedAndBits()
    {
        QCOMPARE(QString::fromStdString(formatBytesPerSecond(1024, UnitMode::Kb, UnitBase::Byte)),
                 QStringLiteral("1.0 KB/s"));
        QCOMPARE(QString::fromStdString(formatBytesPerSecond(1024, UnitMode::Mb, UnitBase::Byte)),
                 QStringLiteral("0.00 MB/s"));
        QCOMPARE(QString::fromStdString(formatBytesPerSecond(1024, UnitMode::Kb, UnitBase::Bit)),
                 QStringLiteral("8.0 Kb/s"));
    }

    void percentValues()
    {
        QCOMPARE(QString::fromStdString(formatPercent(12.4)), QStringLiteral("12%"));
        QCOMPARE(QString::fromStdString(formatPercent(12.6)), QStringLiteral("13%"));
        QCOMPARE(QString::fromStdString(formatPercent(-1)), QStringLiteral("0%"));
        QCOMPARE(QString::fromStdString(formatPercent(120)), QStringLiteral("100%"));
    }

    void temperatureValues()
    {
        QCOMPARE(QString::fromStdString(formatTemperature(43.26)), QStringLiteral("43.3°C"));
        QCOMPARE(QString::fromStdString(formatTemperature(std::nullopt)), QStringLiteral("--"));
    }

    void frequencyValues()
    {
        QCOMPARE(QString::fromStdString(formatFrequencyMHz(3200)), QStringLiteral("3.20 GHz"));
        QCOMPARE(QString::fromStdString(formatFrequencyMHz(800)), QStringLiteral("800 MHz"));
    }

    void memoryValues()
    {
        const std::uint64_t gib = 1024ull * 1024ull * 1024ull;
        QCOMPARE(QString::fromStdString(formatMemory(7 * gib, 14 * gib)),
                 QStringLiteral("7.0 / 14.0 GiB"));
        QCOMPARE(QString::fromStdString(formatMemory(512ull * 1024ull * 1024ull,
                                                     1023ull * 1024ull * 1024ull)),
                 QStringLiteral("512.0 / 1023.0 MiB"));
    }
};

QTEST_APPLESS_MAIN(FormattingTest)

#include "test_formatting.moc"
