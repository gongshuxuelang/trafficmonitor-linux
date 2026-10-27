#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "tm/app_settings.h"
#include "tm/storage.h"

using namespace tm_linux;

namespace {

std::unique_ptr<Database> openDatabase(const QTemporaryDir& dir)
{
    auto database = std::make_unique<Database>(dir.filePath("settings.db").toStdString());
    if (!database->open() || !database->initializeSchema()) {
        return nullptr;
    }
    return database;
}

} // namespace

class AppSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreSane()
    {
        const AppSettings settings = AppSettings::defaults();
        QCOMPARE(settings.refresh_interval_ms, 1000);
        QCOMPARE(settings.opacity, 100);
        QVERIFY(settings.always_on_top);
        QVERIFY(!settings.display_items.empty());
        QCOMPARE(settings.display_items.front().id, std::string("net.up"));
    }

    void loadDefaultsWhenDatabaseIsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository repository(database->handle());
        SettingsModel model(repository);
        const AppSettings settings = model.load();

        QCOMPARE(settings.refresh_interval_ms, 1000);
        QCOMPARE(settings.network_interface, std::string("auto"));
        QCOMPARE(settings.disk_mount_point, std::string("/"));
    }

    void saveAndLoadRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository repository(database->handle());
        SettingsModel model(repository);

        AppSettings settings = AppSettings::defaults();
        settings.refresh_interval_ms = 2000;
        settings.opacity = 85;
        settings.always_on_top = false;
        settings.network_interface = "wlp0s20f3";
        settings.disk_mount_point = "/home";
        settings.gpu_enabled = false;
        settings.unit_mode = UnitMode::Mb;
        settings.unit_base = UnitBase::Bit;
        settings.geometry = WindowGeometry{10, 20, 320, 160};
        settings.display_items = {
            DisplayItemSetting{"net.up", true},
            DisplayItemSetting{"net.down", false},
            DisplayItemSetting{"cpu.usage", true},
        };

        QVERIFY(model.save(settings));
        const AppSettings loaded = model.load();

        QCOMPARE(loaded.refresh_interval_ms, 2000);
        QCOMPARE(loaded.opacity, 85);
        QCOMPARE(loaded.always_on_top, false);
        QCOMPARE(loaded.network_interface, std::string("wlp0s20f3"));
        QCOMPARE(loaded.disk_mount_point, std::string("/home"));
        QCOMPARE(loaded.gpu_enabled, false);
        QCOMPARE(loaded.unit_mode, UnitMode::Mb);
        QCOMPARE(loaded.unit_base, UnitBase::Bit);
        QCOMPARE(loaded.geometry.x, 10);
        QCOMPARE(loaded.geometry.y, 20);
        QCOMPARE(loaded.geometry.width, 320);
        QCOMPARE(loaded.geometry.height, 160);
        QCOMPARE(loaded.display_items.size(), std::size_t{3});
        QCOMPARE(loaded.display_items[1].id, std::string("net.down"));
        QCOMPARE(loaded.display_items[1].visible, false);
    }

    void clampsInvalidValues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository repository(database->handle());
        QVERIFY(repository.setValue(QStringLiteral("general.refresh_interval_ms"), 10));
        QVERIFY(repository.setValue(QStringLiteral("window.opacity"), 500));

        SettingsModel model(repository);
        const AppSettings settings = model.load();

        QCOMPARE(settings.refresh_interval_ms, 200);
        QCOMPARE(settings.opacity, 100);
    }
};

QTEST_GUILESS_MAIN(AppSettingsTest)

#include "test_app_settings.moc"
