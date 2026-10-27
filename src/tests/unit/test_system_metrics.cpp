#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "tm/system_metrics.h"

using namespace tm_linux;

namespace {

void writeFile(const QString& path, const QByteArray& contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(contents);
}

QByteArray netDev(std::uint64_t rx, std::uint64_t tx)
{
    return QByteArray("Inter-| Receive | Transmit\n"
                      " face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed\n"
                      "    lo: 1 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0\n"
                      "wlp0s20f3: ")
        + QByteArray::number(rx) + " 10 0 0 0 0 0 0 " + QByteArray::number(tx)
        + " 20 0 0 0 0 0 0\n";
}

QByteArray diskStats(std::uint64_t io_ticks)
{
    return QByteArray(" 259 0 nvme0n1 100 0 2000 50 200 0 4000 60 0 ")
        + QByteArray::number(io_ticks) + " 100 0 0 0 0\n";
}

void makeFixture(const QTemporaryDir& dir)
{
    const QString proc = dir.filePath("proc");
    const QString sys = dir.filePath("sys");
    writeFile(proc + "/net/dev", netDev(1000, 2000));
    writeFile(proc + "/net/route",
              "Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT\n"
              "wlp0s20f3 00000000 0101A8C0 0003 0 0 600 00000000 0 0 0\n");
    writeFile(proc + "/stat", "cpu 100 0 100 800 0 0 0 0\ncpu0 50 0 50 400 0 0 0 0\n");
    writeFile(proc + "/meminfo",
              "MemTotal: 1000000 kB\nMemFree: 100000 kB\nMemAvailable: 500000 kB\n"
              "Buffers: 10000 kB\nCached: 100000 kB\nSwapTotal: 200000 kB\nSwapFree: 150000 kB\n");
    writeFile(proc + "/diskstats", diskStats(1000));
    writeFile(proc + "/cpuinfo", "processor : 0\ncpu MHz : 2000.000\n");
    writeFile(sys + "/class/hwmon/hwmon0/name", "coretemp\n");
    writeFile(sys + "/class/hwmon/hwmon0/temp1_input", "43000\n");
    writeFile(sys + "/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "2000000\n");
}

} // namespace

class SystemMetricsTest : public QObject {
    Q_OBJECT

private slots:
    void firstSampleReadsAllMetrics()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeFixture(dir);

        SystemMetricsCollector collector(dir.filePath("proc").toStdString(),
                                         dir.filePath("sys").toStdString());
        collector.setDiskMountPoint(dir.path().toStdString());
        const Snapshot snapshot = collector.sample();

        QVERIFY(snapshot.network.has_value());
        QCOMPARE(QString::fromStdString(snapshot.network->interface), QStringLiteral("wlp0s20f3"));
        QCOMPARE(snapshot.network->rx_bytes, std::uint64_t{1000});
        QCOMPARE(snapshot.network->tx_bytes, std::uint64_t{2000});
        QCOMPARE(snapshot.network->rx_bytes_per_sec, 0.0);
        QCOMPARE(snapshot.network->tx_bytes_per_sec, 0.0);

        QVERIFY(snapshot.memory.has_value());
        QCOMPARE(snapshot.memory->total_bytes, std::uint64_t{1000000} * 1024);

        QVERIFY(snapshot.temperature.cpu_c.has_value());
        QCOMPARE(snapshot.temperature.cpu_c.value(), 43.0);
        QCOMPARE(snapshot.cpu.frequency_mhz, 2000.0);
        QVERIFY(snapshot.disk.has_value());
    }

    void todayTotalsCanBeSeeded()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeFixture(dir);

        SystemMetricsCollector collector(dir.filePath("proc").toStdString(),
                                         dir.filePath("sys").toStdString());
        collector.setTodayTotals(1000, 2000);
        const Snapshot snapshot = collector.sample();

        QCOMPARE(snapshot.today_up_bytes, std::uint64_t{1000});
        QCOMPARE(snapshot.today_down_bytes, std::uint64_t{2000});
    }

    void secondSampleComputesRates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeFixture(dir);

        SystemMetricsCollector collector(dir.filePath("proc").toStdString(),
                                         dir.filePath("sys").toStdString());
        collector.setDiskMountPoint(dir.path().toStdString());
        collector.sample();

        writeFile(dir.filePath("proc/net/dev"), netDev(3000, 5000));
        writeFile(dir.filePath("proc/diskstats"), diskStats(1500));
        writeFile(dir.filePath("proc/stat"), "cpu 200 0 200 1200 0 0 0 0\ncpu0 100 0 100 600 0 0 0 0\n");
        QTest::qWait(20);

        const Snapshot snapshot = collector.sample();

        QVERIFY(snapshot.network.has_value());
        QVERIFY(snapshot.disk.has_value());
        QVERIFY(snapshot.network->rx_bytes_per_sec > 0.0);
        QVERIFY(snapshot.network->tx_bytes_per_sec > 0.0);
        QVERIFY(snapshot.disk->io_available);
        QVERIFY(snapshot.disk->io_util_percent > 0.0);
        QVERIFY(snapshot.cpu.usage_percent >= 0.0);
        QVERIFY(snapshot.cpu.usage_percent <= 100.0);
    }
};

QTEST_APPLESS_MAIN(SystemMetricsTest)

#include "test_system_metrics.moc"
