#include <QStringList>
#include <QTest>

#include "core/plugin/plugin_host.h"

using namespace tm_linux;

class GpuPluginTest : public QObject {
    Q_OBJECT

private slots:
    void loadsGpuPluginAndExposesItems()
    {
        PluginHost host;
        QVERIFY(host.loadFile(GPU_PLUGIN_PATH));
        QCOMPARE(host.plugins().size(), std::size_t{1});

        const LoadedPlugin& plugin = host.plugins().front();
        QVERIFY(plugin.loaded);
        QCOMPARE(QString::fromStdString(plugin.pluginId), QStringLiteral("trafficmonitor.gpu"));
        QVERIFY(plugin.items.size() >= std::size_t{3});

        QStringList ids;
        for (const auto& item : plugin.items) {
            ids.push_back(QString::fromStdString(item.itemId));
        }
        QVERIFY(ids.contains(QStringLiteral("trafficmonitor.gpu.usage")));
        QVERIFY(ids.contains(QStringLiteral("trafficmonitor.gpu.memory")));
        QVERIFY(ids.contains(QStringLiteral("trafficmonitor.gpu.temp")));

        host.refreshItemValues();
        const auto items = host.allItems();
        QVERIFY(!items.empty());
        for (const auto& item : items) {
            QVERIFY(!item.value.empty());
        }
    }
};

QTEST_APPLESS_MAIN(GpuPluginTest)

#include "test_gpu_plugin.moc"
