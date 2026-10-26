#include <QTest>

#include "tm/collectors.h"

using namespace tm_linux;
using namespace std::chrono_literals;

namespace {

constexpr std::string_view kNetDev = R"(Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 1000 10 0 0 0 0 0 0 2000 20 0 0 0 0 0 0
wlp0s20f3: 123456 100 0 0 0 0 0 0 654321 200 0 0 0 0 0 0
badline
)";

std::vector<NetworkSample> twoSamples(std::uint64_t rx, std::uint64_t tx)
{
    return {
        NetworkSample{"wlp0s20f3", rx, tx},
    };
}

} // namespace

class NetworkCollectorTest : public QObject {
    Q_OBJECT

private slots:
    void parseNetDevParsesInterfaces()
    {
        const auto samples = parseNetDev(kNetDev);

        QCOMPARE(samples.size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(samples[0].interface), QStringLiteral("lo"));
        QCOMPARE(samples[0].rx_bytes, std::uint64_t{1000});
        QCOMPARE(samples[0].tx_bytes, std::uint64_t{2000});
        QCOMPARE(QString::fromStdString(samples[1].interface), QStringLiteral("wlp0s20f3"));
        QCOMPARE(samples[1].rx_bytes, std::uint64_t{123456});
        QCOMPARE(samples[1].tx_bytes, std::uint64_t{654321});
    }

    void parseNetDevIgnoresMalformedLinesButKeepsValidOnes()
    {
        const auto samples = parseNetDev(
            "Inter-|   Receive | Transmit\n"
            "badline\n"
            "eth0: 10 1 0 0 0 0 0 0 20 2 0 0 0 0 0 0\n"
            " broken: no numbers here\n");

        QCOMPARE(samples.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(samples[0].interface), QStringLiteral("eth0"));
        QCOMPARE(samples[0].rx_bytes, std::uint64_t{10});
        QCOMPARE(samples[0].tx_bytes, std::uint64_t{20});
    }

    void firstSampleHasNoRate()
    {
        NetworkRateCalculator calc;
        const auto rates = calc.update(twoSamples(1000, 2000), std::chrono::steady_clock::time_point{});

        QCOMPARE(rates.size(), std::size_t{1});
        QVERIFY(!rates.at("wlp0s20f3").rx_bytes_per_sec.has_value());
        QVERIFY(!rates.at("wlp0s20f3").tx_bytes_per_sec.has_value());
    }

    void secondSampleComputesRate()
    {
        NetworkRateCalculator calc;
        const auto t0 = std::chrono::steady_clock::time_point{};
        calc.update(twoSamples(1000, 2000), t0);

        const auto rates = calc.update(twoSamples(5000, 6000), t0 + 2s);

        QCOMPARE(rates.at("wlp0s20f3").rx_bytes_per_sec.value(), 2000.0);
        QCOMPARE(rates.at("wlp0s20f3").tx_bytes_per_sec.value(), 2000.0);
    }

    void counterResetProducesZeroRate()
    {
        NetworkRateCalculator calc;
        const auto t0 = std::chrono::steady_clock::time_point{};
        calc.update(twoSamples(5000, 6000), t0);

        const auto rates = calc.update(twoSamples(1000, 7000), t0 + 1s);

        QCOMPARE(rates.at("wlp0s20f3").rx_bytes_per_sec.value(), 0.0);
        QCOMPARE(rates.at("wlp0s20f3").tx_bytes_per_sec.value(), 1000.0);
    }

    void zeroElapsedTimeProducesNoRate()
    {
        NetworkRateCalculator calc;
        const auto t0 = std::chrono::steady_clock::time_point{};
        calc.update(twoSamples(1000, 2000), t0);

        const auto rates = calc.update(twoSamples(5000, 6000), t0);

        QVERIFY(!rates.at("wlp0s20f3").rx_bytes_per_sec.has_value());
        QVERIFY(!rates.at("wlp0s20f3").tx_bytes_per_sec.has_value());
    }
};

QTEST_APPLESS_MAIN(NetworkCollectorTest)

#include "test_network_collector.moc"
