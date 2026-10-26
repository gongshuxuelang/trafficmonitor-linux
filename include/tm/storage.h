#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace tm_linux {

class Database {
public:
    explicit Database(std::filesystem::path path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(QString* error = nullptr);
    bool initializeSchema(QString* error = nullptr);
    QSqlDatabase& handle();
    const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
    QString connection_name_;
    QSqlDatabase database_;
};

class SettingsRepository {
public:
    explicit SettingsRepository(QSqlDatabase& database);

    bool setJson(const QString& key, const QJsonObject& value, QString* error = nullptr);
    std::optional<QJsonObject> getJson(const QString& key) const;

    bool setValue(const QString& key, const QJsonValue& value, QString* error = nullptr);
    std::optional<QJsonValue> getValue(const QString& key) const;

    bool remove(const QString& key, QString* error = nullptr);

    bool exportToJson(const std::filesystem::path& path, QString* error = nullptr) const;
    bool importFromJson(const std::filesystem::path& path, QString* error = nullptr);

private:
    QSqlDatabase& database_;
};

class HistoryRepository {
public:
    explicit HistoryRepository(QSqlDatabase& database);

    bool appendSample(qint64 ts_utc, const QString& day_local, const QString& interface_name,
                      std::uint64_t up_bytes, std::uint64_t down_bytes,
                      QString* error = nullptr);

    struct Totals {
        std::uint64_t up_bytes{};
        std::uint64_t down_bytes{};
    };

    std::optional<Totals> dailyTotals(const QString& day_local,
                                      const QString& interface_name) const;

    bool cleanupBefore(const QString& day_local, QString* error = nullptr);

private:
    QSqlDatabase& database_;
};

} // namespace tm_linux
