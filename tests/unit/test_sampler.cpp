#include <QTest>

#include <atomic>

#include "tm/sampler.h"

using namespace tm_linux;

class SamplerTest : public QObject {
    Q_OBJECT

private slots:
    void producesSnapshotsOnWorkerThread()
    {
        SystemMetricsCollector collector;
        Sampler sampler(collector);
        std::atomic<int> callback_count{0};

        sampler.setSampleCallback([&callback_count](const Snapshot&) {
            callback_count.fetch_add(1);
        });
        sampler.start(50);

        QTRY_VERIFY_WITH_TIMEOUT(sampler.latest() != nullptr, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(callback_count.load() >= 2, 2000);

        const auto snapshot = sampler.latest();
        QVERIFY(snapshot);
        QVERIFY(snapshot->memory.has_value());
        QVERIFY(snapshot->cpu.core_count > 0);

        sampler.stop();
    }
};

QTEST_APPLESS_MAIN(SamplerTest)

#include "test_sampler.moc"
