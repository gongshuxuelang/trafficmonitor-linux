#include <QApplication>
#include <QTest>

#include "ui/floating_window.h"

using namespace tm_linux;

namespace {

PluginItemView gpuItem()
{
    PluginItemView item;
    item.itemId = "trafficmonitor.gpu.usage";
    item.displayName = "GPU";
    item.labelText = "GPU";
    item.sampleText = "5%";
    item.value = "5%";
    return item;
}

} // namespace

class FloatingWindowTest : public QObject {
    Q_OBJECT

private slots:
    void hasOverlayWindowFlags()
    {
        FloatingWindow window;
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(window.testAttribute(Qt::WA_TranslucentBackground));
    }

    void rendersSnapshotAndPluginItems()
    {
        FloatingWindow window;
        AppSettings settings = AppSettings::defaults();
        settings.display_items = {
            DisplayItemSetting{"net.up", true},
            DisplayItemSetting{"cpu.usage", true},
            DisplayItemSetting{"trafficmonitor.gpu.usage", true},
        };
        window.setSettings(settings);

        Snapshot snapshot;
        NetworkInfo network;
        network.interface = "wlp0s20f3";
        network.tx_bytes_per_sec = 1024.0 * 1024.0;
        network.rx_bytes_per_sec = 2.0 * 1024.0 * 1024.0;
        snapshot.network = network;
        snapshot.cpu.usage_percent = 12.0;
        window.setPluginItems({gpuItem()});
        window.setSnapshot(snapshot);

        const QStringList lines = window.renderedLines();
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines[0].contains(QStringLiteral("1.0 MB/s")));
        QVERIFY(lines[1].contains(QStringLiteral("12%")));
        QVERIFY(lines[2].contains(QStringLiteral("5%")));
    }

    void hidesInvisibleItems()
    {
        FloatingWindow window;
        AppSettings settings = AppSettings::defaults();
        settings.display_items = {
            DisplayItemSetting{"net.up", false},
            DisplayItemSetting{"cpu.usage", true},
        };
        window.setSettings(settings);
        window.setSnapshot(Snapshot{});
        QCOMPARE(window.renderedLines().size(), 1);
    }
};

QTEST_MAIN(FloatingWindowTest)

#include "test_floating_window.moc"
