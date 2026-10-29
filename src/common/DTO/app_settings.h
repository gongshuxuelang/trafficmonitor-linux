#ifndef TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_APP_SETTINGS_H_
#define TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_APP_SETTINGS_H_

#include "pch.h"

#include "common/formatting.h"
#include "core/storage/storage.h"

namespace tm_linux {

/**
 * @brief 单个显示项的配置。
 */
struct DisplayItemSetting {
    std::string id;             /**< 显示项标识，如 "net.up"。 */
    bool visible{true};         /**< 是否在悬浮窗中显示。 */
};

/**
 * @brief 悬浮窗的位置与尺寸。
 */
struct WindowGeometry {
    int x{};
    int y{};
    int width{300};
    int height{140};
};

/**
 * @brief 应用全部可持久化设置。
 */
struct AppSettings {
    int refresh_interval_ms{1000};
    int opacity{100};
    bool always_on_top{true};
    bool locked{false};
    bool click_through{false};
    std::string network_interface{"auto"};
    std::string disk_mount_point{"/"};
    bool gpu_enabled{true};
    UnitMode unit_mode{UnitMode::Auto};
    UnitBase unit_base{UnitBase::Byte};
    std::vector<DisplayItemSetting> display_items;
    WindowGeometry geometry;

    /**
     * @brief 生成默认设置。
     * @return 包含全部内置显示项及默认窗口参数的配置。
     */
    static AppSettings defaults();
};

/**
 * @brief 从设置仓库加载并保存 AppSettings。
 */
class SettingsModel {
public:
    /**
     * @brief 绑定设置仓库。
     * @param repository 设置仓库引用，生命周期须长于 SettingsModel。
     */
    explicit SettingsModel(SettingsRepository& repository);

    /**
     * @brief 从存储加载设置，缺失或非法值回退到默认值。
     * @return 合并后的设置。
     */
    AppSettings load();

    /**
     * @brief 将设置写入存储，数值会被限制在合法范围内。
     * @param settings 待保存的设置。
     * @param error 可选的错误信息输出。
     * @return 保存成功返回 true。
     */
    bool save(const AppSettings& settings, QString* error = nullptr);

private:
    SettingsRepository& repository_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_APP_SETTINGS_H_
