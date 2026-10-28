#include <QTest>

#include "core/plugin/plugin_host.h"

using namespace tm_linux;

class PluginHostTest : public QObject {
    Q_OBJECT

private slots:
    void loadsValidPlugin()
    {
        PluginHost host;
        QVERIFY(host.loadFile(TEST_PLUGIN_PATH));
        QCOMPARE(host.plugins().size(), std::size_t{1});

        const LoadedPlugin& plugin = host.plugins().front();
        QVERIFY(plugin.loaded);
        QCOMPARE(QString::fromStdString(plugin.pluginId), QStringLiteral("test.plugin"));
        QCOMPARE(QString::fromStdString(plugin.name), QStringLiteral("Test Plugin"));
        QCOMPARE(plugin.items.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(plugin.items[0].itemId), QStringLiteral("test.item"));

        host.refreshItemValues();
        const auto items = host.allItems();
        QCOMPARE(items.size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(items[0].value), QStringLiteral("42"));
    }

    void rejectsLibraryWithoutFactory()
    {
        PluginHost host;
        QVERIFY(!host.loadFile(EMPTY_PLUGIN_PATH));
        QVERIFY(!host.lastError().empty());
        QCOMPARE(host.plugins().size(), std::size_t{1});
        QVERIFY(!host.plugins().front().loaded);
    }
};

QTEST_APPLESS_MAIN(PluginHostTest)

#include "test_plugin_host.moc"
