#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace tm_linux {

/**
 * @brief 管理 SQLite 连接、表结构初始化与连接生命周期。
 */
class Database {
public:
    /**
     * @brief 创建数据库对象（不会立即打开连接）。
     * @param path 数据库文件路径。
     */
    explicit Database(std::filesystem::path path);

    /**
     * @brief 关闭连接并移除 Qt 连接名。
     */
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief 打开数据库连接。
     * @param error 可选的错误信息输出。
     * @return 打开成功返回 true。
     */
    bool open(QString* error = nullptr);

    /**
     * @brief 创建表和索引，并设置 WAL、busy_timeout 等 PRAGMA。
     * @param error 可选的错误信息输出。
     * @return 初始化成功返回 true。
     */
    bool initializeSchema(QString* error = nullptr);

    /**
     * @brief 获取底层数据库连接。
     * @return QSqlDatabase 引用。
     */
    QSqlDatabase& handle();

    /**
     * @brief 获取数据库文件路径。
     * @return 路径引用。
     */
    const std::filesystem::path& path() const;

private:
    std::filesystem::path path_;
    QString connection_name_;
    QSqlDatabase database_;
};

/**
 * @brief 以键值或 JSON 形式读写 settings 表。
 */
class SettingsRepository {
public:
    /**
     * @brief 绑定已打开的数据库连接。
     * @param database 数据库连接引用，生命周期须长于本对象。
     */
    explicit SettingsRepository(QSqlDatabase& database);

    /**
     * @brief 写入或更新一个 JSON 对象。
     * @param key 设置键。
     * @param value JSON 对象。
     * @param error 可选的错误信息输出。
     * @return 写入成功返回 true。
     */
    bool setJson(const QString& key, const QJsonObject& value, QString* error = nullptr);

    /**
     * @brief 读取一个 JSON 对象。
     * @param key 设置键。
     * @return JSON 对象；键不存在或内容非法时返回 std::nullopt。
     */
    std::optional<QJsonObject> getJson(const QString& key) const;

    /**
     * @brief 写入一个标量或任意 JSON 值（内部包装为 {"value": ...}）。
     * @param key 设置键。
     * @param value JSON 值。
     * @param error 可选的错误信息输出。
     * @return 写入成功返回 true。
     */
    bool setValue(const QString& key, const QJsonValue& value, QString* error = nullptr);

    /**
     * @brief 读取一个标量或任意 JSON 值。
     * @param key 设置键。
     * @return JSON 值；键不存在或缺少 value 字段时返回 std::nullopt。
     */
    std::optional<QJsonValue> getValue(const QString& key) const;

    /**
     * @brief 删除一个设置键。
     * @param key 设置键。
     * @param error 可选的错误信息输出。
     * @return 删除成功返回 true。
     */
    bool remove(const QString& key, QString* error = nullptr);

    /**
     * @brief 将全部设置导出为 JSON 文件。
     * @param path 输出文件路径。
     * @param error 可选的错误信息输出。
     * @return 导出成功返回 true。
     */
    bool exportToJson(const std::filesystem::path& path, QString* error = nullptr) const;

    /**
     * @brief 从 JSON 文件导入设置，导入过程在事务中执行。
     * @param path 输入文件路径。
     * @param error 可选的错误信息输出。
     * @return 导入成功返回 true；文件非法或版本不支持时返回 false。
     */
    bool importFromJson(const std::filesystem::path& path, QString* error = nullptr);

private:
    QSqlDatabase& database_;
};

/**
 * @brief 读写 traffic_history 表中的分钟级流量历史。
 */
class HistoryRepository {
public:
    /**
     * @brief 绑定已打开的数据库连接。
     * @param database 数据库连接引用，生命周期须长于本对象。
     */
    explicit HistoryRepository(QSqlDatabase& database);

    /**
     * @brief 写入一条采样记录；相同时间戳与接口的记录会被覆盖。
     * @param ts_utc UTC Unix 时间戳（秒）。
     * @param day_local 本地日期（yyyy-MM-dd）。
     * @param interface_name 接口名，或汇总用的 "__total__"。
     * @param up_bytes 本周期上传字节增量。
     * @param down_bytes 本周期下载字节增量。
     * @param error 可选的错误信息输出。
     * @return 写入成功返回 true。
     */
    bool appendSample(qint64 ts_utc, const QString& day_local, const QString& interface_name,
                      std::uint64_t up_bytes, std::uint64_t down_bytes,
                      QString* error = nullptr);

    /**
     * @brief 某天某接口的流量汇总。
     */
    struct Totals {
        std::uint64_t up_bytes{};
        std::uint64_t down_bytes{};
    };

    /**
     * @brief 汇总指定日期与接口的流量。
     * @param day_local 本地日期（yyyy-MM-dd）。
     * @param interface_name 接口名或 "__total__"。
     * @return 汇总值；当天没有记录时返回 std::nullopt。
     */
    std::optional<Totals> dailyTotals(const QString& day_local,
                                      const QString& interface_name) const;

    /**
     * @brief 删除早于指定日期的全部历史记录。
     * @param day_local 保留的起始日期（yyyy-MM-dd）。
     * @param error 可选的错误信息输出。
     * @return 删除成功返回 true。
     */
    bool cleanupBefore(const QString& day_local, QString* error = nullptr);

private:
    QSqlDatabase& database_;
};

} // namespace tm_linux
