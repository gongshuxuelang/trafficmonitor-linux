#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "tm/collectors.h"

using namespace tm_linux;

namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

void makeHwmon(const QTemporaryDir& dir, const QString& hwmon, const QString& name,
               const QList<QPair<QString, QByteArray>>& temps)
{
    const QString base = dir.filePath(hwmon);
    QVERIFY(QDir().mkpath(base));
    writeFile(base + QStringLiteral("/name"), name.toUtf8() + '\n');
    for (const auto& [file, value] : temps) {
        writeFile(base + QLatin1Char('/') + file, value);
    }
}

} // namespace

class TemperatureCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void classifyHwmonNames()
    {
        QCOMPARE(classifyHwmonName("coretemp"), TemperatureKind::Cpu);
        QCOMPARE(classifyHwmonName("k10temp"), TemperatureKind::Cpu);
        QCOMPARE(classifyHwmonName("amdgpu"), TemperatureKind::Gpu);
        QCOMPARE(classifyHwmonName("nvme"), TemperatureKind::Hdd);
        QCOMPARE(classifyHwmonName("acpitz"), TemperatureKind::Mainboard);
        QCOMPARE(classifyHwmonName("unknown-chip"), TemperatureKind::Unknown);
    }

    void parseMilliCelsius()
    {
        QCOMPARE(tm_linux::parseMilliCelsius("43000\n").value(), 43.0);
        QCOMPARE(tm_linux::parseMilliCelsius("39850").value(), 39.85);
        QVERIFY(!tm_linux::parseMilliCelsius("not-a-number").has_value());
        QVERIFY(!tm_linux::parseMilliCelsius("").has_value());
    }

    void scanPicksMaximumPerCategory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        makeHwmon(dir, "hwmon0", "coretemp",
                  {{"temp1_input", "43000\n"}, {"temp2_input", "55000\n"}});
        makeHwmon(dir, "hwmon1", "nvme", {{"temp1_input", "39850\n"}});
        makeHwmon(dir, "hwmon2", "amdgpu", {{"temp1_input", "45000\n"}});
        makeHwmon(dir, "hwmon3", "acpitz", {{"temp1_input", "42000\n"}});

        TemperatureCollector collector;
        const TemperatureInfo info = collector.scan(dir.path().toStdString());

        QCOMPARE(info.cpu_c.value(), 55.0);
        QCOMPARE(info.gpu_c.value(), 45.0);
        QCOMPARE(info.hdd_c.value(), 39.85);
        QCOMPARE(info.mainboard_c.value(), 42.0);
    }

    void unreadableSensorYieldsNullopt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeHwmon(dir, "hwmon0", "coretemp", {});

        TemperatureCollector collector;
        const TemperatureInfo info = collector.scan(dir.path().toStdString());

        QVERIFY(!info.cpu_c.has_value());
        QVERIFY(!info.gpu_c.has_value());
        QVERIFY(!info.hdd_c.has_value());
        QVERIFY(!info.mainboard_c.has_value());
    }
};

QTEST_APPLESS_MAIN(TemperatureCollectorTest)

#include "test_temperature_collector.moc"
