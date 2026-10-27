#pragma once

#include <QString>

#include <string>
#include <vector>

#include "tm/formatting.h"
#include "tm/storage.h"

namespace tm_linux {

struct DisplayItemSetting {
    std::string id;
    bool visible{true};
};

struct WindowGeometry {
    int x{};
    int y{};
    int width{300};
    int height{140};
};

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

    static AppSettings defaults();
};

class SettingsModel {
public:
    explicit SettingsModel(SettingsRepository& repository);

    AppSettings load();
    bool save(const AppSettings& settings, QString* error = nullptr);

private:
    SettingsRepository& repository_;
};

} // namespace tm_linux
