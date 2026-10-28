#include "core/storage/storage.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace tm_linux {

namespace {

/**
 * @brief 写入错误信息。
 * @param error 错误输出指针，可为 nullptr。
 * @param message 错误文本。
 */
void setError(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

/**
 * @brief 从查询对象提取 SQL 错误并写入错误输出。
 * @param error 错误输出指针，可为 nullptr。
 * @param query 执行失败的查询。
 */
void setSqlError(QString* error, const QSqlQuery& query)
{
    setError(error, query.lastError().text());
}

/**
 * @brief 获取当前 Unix 毫秒时间戳。
 * @return 毫秒时间戳。
 */
qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

Database::Database(std::filesystem::path path)
    : path_(std::move(path))
    , connection_name_(QStringLiteral("tm_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    if (database_.isValid()) {
        database_.close();
    }
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_name_);
}

bool Database::open(QString* error)
{
    if (!database_.isValid()) {
        database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_);
        database_.setDatabaseName(QString::fromStdString(path_.string()));
    }

    if (!database_.open()) {
        setError(error, database_.lastError().text());
        return false;
    }
    return true;
}

bool Database::initializeSchema(QString* error)
{
    QSqlQuery query(database_);

    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
        "PRAGMA foreign_keys = ON",
        "PRAGMA busy_timeout = 5000",
    };
    for (const char* pragma : pragmas) {
        if (!query.exec(QString::fromLatin1(pragma))) {
            setSqlError(error, query);
            return false;
        }
    }

    const char* statements[] = {
        "CREATE TABLE IF NOT EXISTS app_meta ("
        " key TEXT PRIMARY KEY,"
        " value TEXT NOT NULL"
        ")",

        "INSERT OR IGNORE INTO app_meta(key, value) VALUES ('schema_version', '1')",
        "INSERT OR IGNORE INTO app_meta(key, value) VALUES ('created_by', 'trafficmonitor-linux')",

        "CREATE TABLE IF NOT EXISTS settings ("
        " key TEXT PRIMARY KEY,"
        " value_json TEXT NOT NULL,"
        " updated_at INTEGER NOT NULL"
        ")",

        "CREATE TABLE IF NOT EXISTS traffic_history ("
        " ts_utc INTEGER NOT NULL,"
        " day_local TEXT NOT NULL,"
        " interface TEXT NOT NULL,"
        " up_bytes INTEGER NOT NULL,"
        " down_bytes INTEGER NOT NULL,"
        " PRIMARY KEY (ts_utc, interface)"
        ") WITHOUT ROWID",

        "CREATE INDEX IF NOT EXISTS idx_traffic_history_day"
        " ON traffic_history(day_local, interface)",

        "CREATE TABLE IF NOT EXISTS plugin_state ("
        " plugin_id TEXT PRIMARY KEY,"
        " enabled INTEGER NOT NULL DEFAULT 1,"
        " config_json TEXT NOT NULL DEFAULT '{}',"
        " last_error TEXT,"
        " updated_at INTEGER NOT NULL"
        ")",
    };

    for (const char* statement : statements) {
        if (!query.exec(QString::fromLatin1(statement))) {
            setSqlError(error, query);
            return false;
        }
    }

    return true;
}

QSqlDatabase& Database::handle()
{
    return database_;
}

const std::filesystem::path& Database::path() const
{
    return path_;
}

SettingsRepository::SettingsRepository(QSqlDatabase& database)
    : database_(database)
{
}

bool SettingsRepository::setJson(const QString& key, const QJsonObject& value, QString* error)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value_json, updated_at) VALUES(?, ?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value_json = excluded.value_json, "
        "updated_at = excluded.updated_at"));
    query.addBindValue(key);
    query.addBindValue(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
    query.addBindValue(nowMs());

    if (!query.exec()) {
        setSqlError(error, query);
        return false;
    }
    return true;
}

std::optional<QJsonObject> SettingsRepository::getJson(const QString& key) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT value_json FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    const QJsonDocument document =
        QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

bool SettingsRepository::setValue(const QString& key, const QJsonValue& value, QString* error)
{
    return setJson(key, QJsonObject{{QStringLiteral("value"), value}}, error);
}

std::optional<QJsonValue> SettingsRepository::getValue(const QString& key) const
{
    const auto object = getJson(key);
    if (!object.has_value() || !object->contains(QStringLiteral("value"))) {
        return std::nullopt;
    }
    return object->value(QStringLiteral("value"));
}

bool SettingsRepository::remove(const QString& key, QString* error)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        setSqlError(error, query);
        return false;
    }
    return true;
}

bool SettingsRepository::exportToJson(const std::filesystem::path& path, QString* error) const
{
    QJsonObject settings;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT key, value_json FROM settings ORDER BY key"))) {
        setSqlError(error, query);
        return false;
    }
    while (query.next()) {
        const QJsonDocument document =
            QJsonDocument::fromJson(query.value(1).toString().toUtf8());
        if (!document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        if (object.size() == 1 && object.contains(QStringLiteral("value"))) {
            settings.insert(query.value(0).toString(), object.value(QStringLiteral("value")));
        } else {
            settings.insert(query.value(0).toString(), object);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), 1);
    root.insert(QStringLiteral("exported_at"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("settings"), settings);

    QSaveFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool SettingsRepository::importFromJson(const std::filesystem::path& path, QString* error)
{
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        setError(error, QStringLiteral("config file is not a JSON object"));
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schema_version")).toInt() != 1) {
        setError(error, QStringLiteral("unsupported config schema version"));
        return false;
    }

    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    if (!database_.transaction()) {
        setError(error, database_.lastError().text());
        return false;
    }
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        const QJsonValue value = it.value();
        if (value.isObject() && !value.toObject().contains(QStringLiteral("value"))) {
            if (!setJson(it.key(), value.toObject(), error)) {
                database_.rollback();
                return false;
            }
        } else {
            const QJsonValue scalar =
                value.isObject() ? value.toObject().value(QStringLiteral("value")) : value;
            if (!setValue(it.key(), scalar, error)) {
                database_.rollback();
                return false;
            }
        }
    }
    if (!database_.commit()) {
        setError(error, database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

HistoryRepository::HistoryRepository(QSqlDatabase& database)
    : database_(database)
{
}

bool HistoryRepository::appendSample(qint64 ts_utc, const QString& day_local,
                                     const QString& interface_name, std::uint64_t up_bytes,
                                     std::uint64_t down_bytes, QString* error)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO traffic_history(ts_utc, day_local, interface, up_bytes, down_bytes) "
        "VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(ts_utc, interface) DO UPDATE SET day_local = excluded.day_local, "
        "up_bytes = excluded.up_bytes, down_bytes = excluded.down_bytes"));
    query.addBindValue(ts_utc);
    query.addBindValue(day_local);
    query.addBindValue(interface_name);
    query.addBindValue(static_cast<qint64>(up_bytes));
    query.addBindValue(static_cast<qint64>(down_bytes));

    if (!query.exec()) {
        setSqlError(error, query);
        return false;
    }
    return true;
}

std::optional<HistoryRepository::Totals> HistoryRepository::dailyTotals(
    const QString& day_local, const QString& interface_name) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*), COALESCE(SUM(up_bytes), 0), COALESCE(SUM(down_bytes), 0) "
        "FROM traffic_history WHERE day_local = ? AND interface = ?"));
    query.addBindValue(day_local);
    query.addBindValue(interface_name);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    if (query.value(0).toLongLong() == 0) {
        return std::nullopt;
    }

    Totals totals;
    totals.up_bytes = static_cast<std::uint64_t>(query.value(1).toLongLong());
    totals.down_bytes = static_cast<std::uint64_t>(query.value(2).toLongLong());
    return totals;
}

bool HistoryRepository::cleanupBefore(const QString& day_local, QString* error)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM traffic_history WHERE day_local < ?"));
    query.addBindValue(day_local);
    if (!query.exec()) {
        setSqlError(error, query);
        return false;
    }
    return true;
}

} // namespace tm_linux
