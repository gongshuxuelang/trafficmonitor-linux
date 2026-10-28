#include "core/app_settings.h"

namespace tm_linux {

namespace {

/**
 * @brief 从设置仓库读取整数并限制在指定范围内。
 * @param repository 设置仓库。
 * @param key 设置键。
 * @param fallback 缺失或类型不符时的默认值。
 * @param minimum 允许的最小值。
 * @param maximum 允许的最大值。
 * @return 限制在 [minimum, maximum] 内的整数。
 */
int clampedInt(SettingsRepository& repository, const QString& key, int fallback, int minimum,
               int maximum)
{
    const auto value = repository.getValue(key);
    if (!value.has_value() || !value->isDouble()) {
        return fallback;
    }
    return std::clamp(value->toInt(), minimum, maximum);
}

/**
 * @brief 从设置仓库读取布尔值。
 * @param repository 设置仓库。
 * @param key 设置键。
 * @param fallback 缺失或类型不符时的默认值。
 * @return 读取到的布尔值或默认值。
 */
bool boolValue(SettingsRepository& repository, const QString& key, bool fallback)
{
    const auto value = repository.getValue(key);
    return value.has_value() && value->isBool() ? value->toBool() : fallback;
}

/**
 * @brief 从设置仓库读取非空字符串。
 * @param repository 设置仓库。
 * @param key 设置键。
 * @param fallback 缺失、类型不符或内容为空时的默认值。
 * @return 读取到的字符串或默认值。
 */
std::string stringValue(SettingsRepository& repository, const QString& key,
                        const std::string& fallback)
{
    const auto value = repository.getValue(key);
    if (!value.has_value() || !value->isString()) {
        return fallback;
    }
    const std::string result = value->toString().toStdString();
    return result.empty() ? fallback : result;
}

/**
 * @brief 将存储字符串解析为单位模式。
 * @param value "auto"、"kb" 或 "mb"。
 * @return 对应的单位模式，未知值按 Auto 处理。
 */
UnitMode parseUnitMode(const std::string& value)
{
    if (value == "kb") {
        return UnitMode::Kb;
    }
    if (value == "mb") {
        return UnitMode::Mb;
    }
    return UnitMode::Auto;
}

/**
 * @brief 将单位模式转换为存储字符串。
 * @param mode 单位模式。
 * @return "auto"、"kb" 或 "mb"。
 */
std::string unitModeName(UnitMode mode)
{
    switch (mode) {
    case UnitMode::Kb:
        return "kb";
    case UnitMode::Mb:
        return "mb";
    case UnitMode::Auto:
        return "auto";
    }
    return "auto";
}

/**
 * @brief 将存储字符串解析为速率单位基准。
 * @param value "bit" 或其他值。
 * @return Bit 或 Byte。
 */
UnitBase parseUnitBase(const std::string& value)
{
    return value == "bit" ? UnitBase::Bit : UnitBase::Byte;
}

/**
 * @brief 将速率单位基准转换为存储字符串。
 * @param base 速率单位基准。
 * @return "bit" 或 "byte"。
 */
std::string unitBaseName(UnitBase base)
{
    return base == UnitBase::Bit ? "bit" : "byte";
}

} // namespace

AppSettings AppSettings::defaults()
{
    AppSettings settings;
    settings.display_items = {
        DisplayItemSetting{"net.up", true},
        DisplayItemSetting{"net.down", true},
        DisplayItemSetting{"cpu.usage", true},
        DisplayItemSetting{"mem.usage", true},
        DisplayItemSetting{"disk.io", true},
        DisplayItemSetting{"cpu.temp", true},
        DisplayItemSetting{"cpu.freq", true},
        DisplayItemSetting{"trafficmonitor.gpu.usage", true},
        DisplayItemSetting{"trafficmonitor.gpu.memory", true},
        DisplayItemSetting{"trafficmonitor.gpu.temp", true},
    };
    return settings;
}

SettingsModel::SettingsModel(SettingsRepository& repository)
    : repository_(repository)
{
}

AppSettings SettingsModel::load()
{
    AppSettings settings = AppSettings::defaults();

    settings.refresh_interval_ms = clampedInt(
        repository_, QStringLiteral("general.refresh_interval_ms"), settings.refresh_interval_ms,
        200, 5000);
    settings.opacity = clampedInt(repository_, QStringLiteral("window.opacity"), settings.opacity,
                                  20, 100);
    settings.always_on_top =
        boolValue(repository_, QStringLiteral("window.always_on_top"), settings.always_on_top);
    settings.locked = boolValue(repository_, QStringLiteral("window.locked"), settings.locked);
    settings.click_through =
        boolValue(repository_, QStringLiteral("window.click_through"), settings.click_through);
    settings.network_interface = stringValue(
        repository_, QStringLiteral("network.interface"), settings.network_interface);
    settings.disk_mount_point =
        stringValue(repository_, QStringLiteral("disk.mount_point"), settings.disk_mount_point);
    settings.gpu_enabled =
        boolValue(repository_, QStringLiteral("gpu.enabled"), settings.gpu_enabled);
    settings.unit_mode = parseUnitMode(
        stringValue(repository_, QStringLiteral("display.unit_mode"), unitModeName(settings.unit_mode)));
    settings.unit_base = parseUnitBase(
        stringValue(repository_, QStringLiteral("display.unit_base"), unitBaseName(settings.unit_base)));

    const auto items_value = repository_.getValue(QStringLiteral("display.items"));
    if (items_value.has_value() && items_value->isArray()) {
        std::vector<DisplayItemSetting> items;
        std::set<std::string> seen;
        for (const QJsonValue& entry : items_value->toArray()) {
            std::string id;
            bool visible = true;
            if (entry.isString()) {
                id = entry.toString().toStdString();
            } else if (entry.isObject()) {
                const QJsonObject object = entry.toObject();
                id = object.value(QStringLiteral("id")).toString().toStdString();
                visible = object.value(QStringLiteral("visible")).toBool(true);
            }
            if (!id.empty() && seen.insert(id).second) {
                items.push_back(DisplayItemSetting{id, visible});
            }
        }
        if (!items.empty()) {
            settings.display_items = std::move(items);
        }
    }

    const auto geometry = repository_.getJson(QStringLiteral("window.geometry"));
    if (geometry.has_value()) {
        settings.geometry.x = std::clamp(geometry->value(QStringLiteral("x")).toInt(settings.geometry.x),
                                         -10000, 10000);
        settings.geometry.y = std::clamp(geometry->value(QStringLiteral("y")).toInt(settings.geometry.y),
                                         -10000, 10000);
        settings.geometry.width = std::clamp(
            geometry->value(QStringLiteral("width")).toInt(settings.geometry.width), 120, 2000);
        settings.geometry.height = std::clamp(
            geometry->value(QStringLiteral("height")).toInt(settings.geometry.height), 60, 2000);
    }

    return settings;
}

bool SettingsModel::save(const AppSettings& settings, QString* error)
{
    auto set = [this, error](const QString& key, const QJsonValue& value) {
        return repository_.setValue(key, value, error);
    };

    if (!set(QStringLiteral("general.refresh_interval_ms"),
             std::clamp(settings.refresh_interval_ms, 200, 5000))) {
        return false;
    }
    if (!set(QStringLiteral("window.opacity"), std::clamp(settings.opacity, 20, 100))) {
        return false;
    }
    if (!set(QStringLiteral("window.always_on_top"), settings.always_on_top)) {
        return false;
    }
    if (!set(QStringLiteral("window.locked"), settings.locked)) {
        return false;
    }
    if (!set(QStringLiteral("window.click_through"), settings.click_through)) {
        return false;
    }
    if (!set(QStringLiteral("network.interface"),
             QString::fromStdString(settings.network_interface))) {
        return false;
    }
    if (!set(QStringLiteral("disk.mount_point"),
             QString::fromStdString(settings.disk_mount_point))) {
        return false;
    }
    if (!set(QStringLiteral("gpu.enabled"), settings.gpu_enabled)) {
        return false;
    }
    if (!set(QStringLiteral("display.unit_mode"),
             QString::fromStdString(unitModeName(settings.unit_mode)))) {
        return false;
    }
    if (!set(QStringLiteral("display.unit_base"),
             QString::fromStdString(unitBaseName(settings.unit_base)))) {
        return false;
    }

    QJsonArray items;
    for (const auto& item : settings.display_items) {
        items.append(QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(item.id)},
            {QStringLiteral("visible"), item.visible},
        });
    }
    if (!set(QStringLiteral("display.items"), items)) {
        return false;
    }

    QJsonObject geometry{
        {QStringLiteral("x"), settings.geometry.x},
        {QStringLiteral("y"), settings.geometry.y},
        {QStringLiteral("width"), std::clamp(settings.geometry.width, 120, 2000)},
        {QStringLiteral("height"), std::clamp(settings.geometry.height, 60, 2000)},
    };
    if (!repository_.setJson(QStringLiteral("window.geometry"), geometry, error)) {
        return false;
    }

    return true;
}

} // namespace tm_linux
