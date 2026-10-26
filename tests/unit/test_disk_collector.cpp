#include <QTest>

#include <chrono>

#include "tm/collectors.h"

using namespace tm_linux;
using namespace std::chrono_literals;

namespace {

constexpr std::string_view kDiskStats = R"( 259       0 nvme0n1 100 0 2000 50 200 0 4000 60 0 1234 100 0 0 0 0
   7       0 loop0 10 0 20 5 0 0 0 0 0 100 0 0 0 0 0 0
)";

} // namespace

class DiskCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void parseDiskStatsReadsIoTicks()
    {
        const auto stats = parseDiskStats(kDiskStats);

        QCOMPARE(stats.size(), std::size_t{2});
        QCOMPARE(stats.at("nvme0n1").io_ticks_ms, std::uint64_t{1234});
        QCOMPARE(stats.at("loop0").io_ticks_ms, std::uint64_t{100});
    }

    void physicalDiskFilterExcludesVirtualDevices()
    {
        QVERIFY(isPhysicalDiskName("nvme0n1"));
        QVERIFY(isPhysicalDiskName("sda"));
        QVERIFY(!isPhysicalDiskName("loop0"));
        QVERIFY(!isPhysicalDiskName("ram0"));
        QVERIFY(!isPhysicalDiskName("dm-0"));
    }

    void firstSampleHasNoUtilization()
    {
        DiskIoRateCalculator calc;
        const auto rates = calc.update(parseDiskStats(kDiskStats), std::chrono::steady_clock::time_point{});

        QVERIFY(rates.empty());
    }

    void computesUtilizationFromIoTicks()
    {
        DiskIoRateCalculator calc;
        const auto t0 = std::chrono::steady_clock::time_point{};
        calc.update(parseDiskStats(kDiskStats), t0);

        auto later = parseDiskStats(kDiskStats);
        later["nvme0n1"].io_ticks_ms += 500;
        const auto rates = calc.update(later, t0 + 1s);

        QCOMPARE(rates.at("nvme0n1"), 50.0);
    }

    void counterResetProducesZeroUtilization()
    {
        DiskIoRateCalculator calc;
        const auto t0 = std::chrono::steady_clock::time_point{};
        calc.update(parseDiskStats(kDiskStats), t0);

        auto later = parseDiskStats(kDiskStats);
        later["nvme0n1"].io_ticks_ms = 10;
        const auto rates = calc.update(later, t0 + 1s);

        QCOMPARE(rates.at("nvme0n1"), 0.0);
    }

    void capacityUsagePercent()
    {
        QCOMPARE(computeDiskUsagePercent(1000, 400), 60.0);
        QCOMPARE(computeDiskUsagePercent(0, 0), 0.0);
    }
};

QTEST_APPLESS_MAIN(DiskCollectorTest)

#include "test_disk_collector.moc"
