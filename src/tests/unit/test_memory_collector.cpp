#include <QTest>

#include "tm/collectors.h"

using namespace tm_linux;

namespace {

constexpr std::string_view kMemInfo = R"(MemTotal:       16384000 kB
MemFree:         1048576 kB
MemAvailable:    8388608 kB
Buffers:          100000 kB
Cached:          2000000 kB
SwapTotal:       4096000 kB
SwapFree:        3072000 kB
)";

} // namespace

class MemoryCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void parseMemInfoUsesMemAvailable()
    {
        const auto memory = parseMemInfo(kMemInfo);

        QVERIFY(memory.has_value());
        QCOMPARE(memory->total_bytes, std::uint64_t{16384000} * 1024);
        QCOMPARE(memory->available_bytes, std::uint64_t{8388608} * 1024);
        QCOMPARE(memory->used_bytes, (std::uint64_t{16384000} - 8388608) * 1024);
        const double expected_percent =
            100.0 * static_cast<double>(std::uint64_t{16384000} - 8388608) / 16384000.0;
        QCOMPARE(memory->used_percent, expected_percent);
        QCOMPARE(memory->swap_total_bytes, std::uint64_t{4096000} * 1024);
        QCOMPARE(memory->swap_free_bytes, std::uint64_t{3072000} * 1024);
    }

    void parseMemInfoFallsBackWhenMemAvailableMissing()
    {
        const auto memory = parseMemInfo(
            "MemTotal: 1000 kB\n"
            "MemFree: 200 kB\n"
            "Buffers: 100 kB\n"
            "Cached: 300 kB\n");

        QVERIFY(memory.has_value());
        QCOMPARE(memory->used_bytes, std::uint64_t{400} * 1024);
        QCOMPARE(memory->used_percent, 40.0);
    }

    void parseMemInfoRejectsMissingTotal()
    {
        QVERIFY(!parseMemInfo("MemFree: 100 kB\n").has_value());
        QVERIFY(!parseMemInfo("").has_value());
    }
};

QTEST_APPLESS_MAIN(MemoryCollectorTest)

#include "test_memory_collector.moc"
