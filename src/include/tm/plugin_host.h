#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "tm/metrics.h"
#include "tm_plugin_api.h"

namespace tm_linux {

struct PluginItemView {
    std::string itemId;
    std::string displayName;
    std::string labelText;
    std::string sampleText;
    std::string value;
    bool customDraw{};
    const TMPluginItemV1* api{};
};

struct LoadedPlugin {
    std::filesystem::path path;
    std::string pluginId;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    bool loaded{};
    std::string error;
    void* handle{};
    const TMPluginV1* api{};
    std::vector<PluginItemView> items;
};

class PluginHost {
public:
    explicit PluginHost(std::filesystem::path plugin_dir = {});
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    bool loadFile(const std::filesystem::path& path);
    void loadAll();
    void update(const Snapshot& snapshot);
    void refreshItemValues();

    const std::vector<LoadedPlugin>& plugins() const;
    std::vector<PluginItemView> allItems() const;

    const std::string& lastError() const;
    void setConfigDir(std::filesystem::path config_dir);
    void setDpi(std::uint32_t dpi);

private:
    void setError(std::string message);
    void unloadAll();

    std::filesystem::path plugin_dir_;
    std::filesystem::path config_dir_;
    std::vector<LoadedPlugin> plugins_;
    std::string last_error_;
    std::uint32_t dpi_{96};
    TMPluginHostV1 host_api_{};
};

} // namespace tm_linux
