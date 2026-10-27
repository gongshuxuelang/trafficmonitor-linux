#include <QTest>

#include "tm/collectors.h"

using namespace tm_linux;

namespace {

constexpr std::string_view kProcStat = R"(cpu  100 20 30 400 50 10 5 7 0 0
cpu0 50 10 15 200 25 5 2 3 0 0
cpu1 50 10 15 200 25 5 3 4 0 0
intr 12345
)";

} // namespace

class CpuCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void parseProcStatReadsAggregateLine()
    {
        const auto times = parseProcStat(kProcStat);

        QVERIFY(times.has_value());
        QCOMPARE(times->user, std::uint64_t{100});
        QCOMPARE(times->nice, std::uint64_t{20});
        QCOMPARE(times->system, std::uint64_t{30});
        QCOMPARE(times->idle, std::uint64_t{400});
        QCOMPARE(times->iowait, std::uint64_t{50});
        QCOMPARE(times->irq, std::uint64_t{10});
        QCOMPARE(times->softirq, std::uint64_t{5});
        QCOMPARE(times->steal, std::uint64_t{7});
    }

    void parseProcStatRejectsMissingAggregateLine()
    {
        QVERIFY(!parseProcStat("cpu0 1 2 3 4\n").has_value());
        QVERIFY(!parseProcStat("").has_value());
    }

    void firstSampleHasNoUsage()
    {
        const auto current = parseProcStat(kProcStat);
        QVERIFY(current.has_value());

        QVERIFY(!computeCpuUsagePercent(*current, *current).has_value());
    }

    void computesUsageFromDelta()
    {
        CpuTimes previous{};
        previous.user = 500;
        previous.idle = 400;
        previous.iowait = 100;

        CpuTimes current{};
        current.user = 800;
        current.idle = 500;
        current.iowait = 200;

        // total delta = 300 + 100 + 100 = 500; idle delta = 200; usage = 60%.
        const auto usage = computeCpuUsagePercent(previous, current);
        QVERIFY(usage.has_value());
        QCOMPARE(usage.value(), 60.0);
    }

    void zeroTotalDeltaHasNoUsage()
    {
        CpuTimes previous{};
        previous.user = 100;
        previous.idle = 100;

        QVERIFY(!computeCpuUsagePercent(previous, previous).has_value());
    }
};

QTEST_APPLESS_MAIN(CpuCollectorTest)

#include "test_cpu_collector.moc"
