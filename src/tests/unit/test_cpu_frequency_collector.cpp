#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/collectors/CPU/cpu_frequency_collector.h"

using namespace tm_linux;

namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(contents);
}

void makeCpuFreq(const QTemporaryDir& dir, const QString& cpu, const QByteArray& khz)
{
    const QString base = dir.filePath(cpu + QStringLiteral("/cpufreq"));
    QDir().mkpath(base);
    writeFile(base + QStringLiteral("/scaling_cur_freq"), khz);
}

} // namespace

class CpuFrequencyCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void parseKilohertzValues()
    {
        QCOMPARE(tm_linux::parseKilohertz("400330\n").value(), std::uint64_t{400330});
        QCOMPARE(tm_linux::parseKilohertz("1800000").value(), std::uint64_t{1800000});
        QVERIFY(!tm_linux::parseKilohertz("bad").has_value());
        QVERIFY(!tm_linux::parseKilohertz("").has_value());
    }

    void averageFrequencyInMHz()
    {
        QCOMPARE(averageFrequencyMHz({1000000, 2000000}), 1500.0);
        QCOMPARE(averageFrequencyMHz({}), 0.0);
    }

    void scanAveragesOnlineCpus()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeCpuFreq(dir, "cpu0", "1000000\n");
        makeCpuFreq(dir, "cpu1", "3000000\n");

        CpuFrequencyCollector collector;
        const auto mhz = collector.scan(dir.path().toStdString());

        QVERIFY(mhz.has_value());
        QCOMPARE(mhz.value(), 2000.0);
    }

    void scanReturnsNulloptWithoutData()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        CpuFrequencyCollector collector;
        QVERIFY(!collector.scan(dir.path().toStdString()).has_value());
    }

    void parseCpuInfoFallback()
    {
        const auto mhz = parseCpuInfoAverageMHz(
            "processor : 0\n"
            "cpu MHz : 1200.000\n"
            "processor : 1\n"
            "cpu MHz : 2400.000\n");

        QVERIFY(mhz.has_value());
        QCOMPARE(mhz.value(), 1800.0);
    }
};

QTEST_APPLESS_MAIN(CpuFrequencyCollectorTest)

#include "test_cpu_frequency_collector.moc"
