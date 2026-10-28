#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "core/storage/storage.h"

using namespace tm_linux;

namespace {

std::unique_ptr<Database> openDatabase(const QTemporaryDir& dir)
{
    auto database = std::make_unique<Database>(dir.filePath("test.db").toStdString());
    if (!database->open() || !database->initializeSchema()) {
        return nullptr;
    }
    return database;
}

} // namespace

class StorageTest : public QObject {
    Q_OBJECT

private slots:
    void settingsJsonRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository settings(database->handle());
        const QJsonObject value{{"refresh_interval_ms", 1000}, {"unit", "auto"}};

        QVERIFY(settings.setJson("general", value));
        const auto loaded = settings.getJson("general");
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->value("refresh_interval_ms").toInt(), 1000);
        QCOMPARE(loaded->value("unit").toString(), QStringLiteral("auto"));
        QVERIFY(!settings.getJson("missing").has_value());
    }

    void settingsOverwriteAndPrimitives()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository settings(database->handle());
        QVERIFY(settings.setValue("window.opacity", 90));
        QVERIFY(settings.setValue("window.opacity", 80));

        const auto loaded = settings.getValue("window.opacity");
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->toInt(), 80);
    }

    void historySumsDailyTotals()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        HistoryRepository history(database->handle());
        QVERIFY(history.appendSample(60, "2026-10-26", "wlp0s20f3", 100, 200));
        QVERIFY(history.appendSample(120, "2026-10-26", "wlp0s20f3", 50, 60));

        const auto totals = history.dailyTotals("2026-10-26", "wlp0s20f3");
        QVERIFY(totals.has_value());
        QCOMPARE(totals->up_bytes, std::uint64_t{150});
        QCOMPARE(totals->down_bytes, std::uint64_t{260});
        QVERIFY(!history.dailyTotals("2026-10-25", "wlp0s20f3").has_value());
    }

    void historyUpsertsSameBucket()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        HistoryRepository history(database->handle());
        QVERIFY(history.appendSample(60, "2026-10-26", "wlp0s20f3", 100, 200));
        QVERIFY(history.appendSample(60, "2026-10-26", "wlp0s20f3", 150, 250));

        const auto totals = history.dailyTotals("2026-10-26", "wlp0s20f3");
        QVERIFY(totals.has_value());
        QCOMPARE(totals->up_bytes, std::uint64_t{150});
        QCOMPARE(totals->down_bytes, std::uint64_t{250});
    }

    void historyCleanupRemovesOlderDays()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        HistoryRepository history(database->handle());
        QVERIFY(history.appendSample(60, "2026-10-24", "wlp0s20f3", 1, 1));
        QVERIFY(history.appendSample(120, "2026-10-25", "wlp0s20f3", 2, 2));
        QVERIFY(history.appendSample(180, "2026-10-26", "wlp0s20f3", 3, 3));

        QVERIFY(history.cleanupBefore("2026-10-25"));
        QVERIFY(!history.dailyTotals("2026-10-24", "wlp0s20f3").has_value());
        QVERIFY(history.dailyTotals("2026-10-25", "wlp0s20f3").has_value());
        QVERIFY(history.dailyTotals("2026-10-26", "wlp0s20f3").has_value());
    }

    void configExportImportRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        SettingsRepository repository(database->handle());
        QVERIFY(repository.setValue(QStringLiteral("window.opacity"), 90));
        QVERIFY(repository.setJson(QStringLiteral("general"),
                                   QJsonObject{{QStringLiteral("refresh"), 1000}}));

        const QString export_path = dir.filePath(QStringLiteral("config.json"));
        QVERIFY(repository.exportToJson(export_path.toStdString()));

        Database second(dir.filePath(QStringLiteral("second.db")).toStdString());
        QVERIFY(second.open());
        QVERIFY(second.initializeSchema());
        SettingsRepository second_repository(second.handle());
        QVERIFY(second_repository.importFromJson(export_path.toStdString()));

        const auto opacity = second_repository.getValue(QStringLiteral("window.opacity"));
        QVERIFY(opacity.has_value());
        QCOMPARE(opacity->toInt(), 90);

        const auto general = second_repository.getJson(QStringLiteral("general"));
        QVERIFY(general.has_value());
        QCOMPARE(general->value(QStringLiteral("refresh")).toInt(), 1000);
    }

    void schemaVersionIsOne()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto database = openDatabase(dir);
        QVERIFY(database);

        QSqlQuery query(database->handle());
        QVERIFY(query.exec("SELECT value FROM app_meta WHERE key = 'schema_version'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("1"));
    }
};

QTEST_GUILESS_MAIN(StorageTest)

#include "test_storage.moc"
