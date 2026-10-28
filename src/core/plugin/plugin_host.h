#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/metrics.h"
#include "core/plugin/tm_plugin_api.h"

namespace tm_linux {

/**
 * @brief 插件条目的只读视图。
 */
struct PluginItemView {
    std::string itemId;
    std::string displayName;
    std::string labelText;
    std::string sampleText;
    std::string value;
    bool customDraw{};
    const TMPluginItemV1* api{};
};

/**
 * @brief 已扫描插件的元数据、加载状态与条目列表。
 */
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

/**
 * @brief 动态加载 .so 插件，维护宿主 API 并向插件分发指标更新。
 */
class PluginHost {
public:
    /**
     * @brief 创建插件宿主并初始化提供给插件的宿主 API。
     * @param plugin_dir 插件扫描目录。
     */
    explicit PluginHost(std::filesystem::path plugin_dir = {});

    /**
     * @brief 调用各插件的 shutdown 并释放动态库句柄。
     */
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    /**
     * @brief 加载单个插件，校验 ABI、必要回调与条目后登记。
     * @param path 插件 .so 文件路径。
     * @return 加载成功返回 true；失败时仍会记录一条未加载的插件信息。
     */
    bool loadFile(const std::filesystem::path& path);

    /**
     * @brief 扫描插件目录并按文件名顺序加载全部 .so 文件。
     */
    void loadAll();

    /**
     * @brief 将最新快照转换为插件 ABI 结构并分发给所有已加载插件。
     * @param snapshot 最新指标快照。
     */
    void update(const Snapshot& snapshot);

    /**
     * @brief 刷新所有插件条目的显示值，取不到值时置为 "--"。
     */
    void refreshItemValues();

    /**
     * @brief 获取已扫描的插件列表。
     * @return 插件记录列表的引用。
     */
    const std::vector<LoadedPlugin>& plugins() const;

    /**
     * @brief 汇总所有已加载插件的条目。
     * @return 条目视图列表。
     */
    std::vector<PluginItemView> allItems() const;

    /**
     * @brief 获取最近一次错误信息。
     * @return 错误文本的引用。
     */
    const std::string& lastError() const;

    /**
     * @brief 设置插件配置目录并确保目录存在。
     * @param config_dir 配置目录路径。
     */
    void setConfigDir(std::filesystem::path config_dir);

    /**
     * @brief 设置提供给插件的屏幕 DPI。
     * @param dpi DPI 数值。
     */
    void setDpi(std::uint32_t dpi);

private:
    /**
     * @brief 记录错误信息。
     * @param message 错误文本。
     */
    void setError(std::string message);

    /**
     * @brief 卸载全部插件并清空记录。
     */
    void unloadAll();

    std::filesystem::path plugin_dir_;
    std::filesystem::path config_dir_;
    std::vector<LoadedPlugin> plugins_;
    std::string last_error_;
    std::uint32_t dpi_{96};
    TMPluginHostV1 host_api_{};
};

} // namespace tm_linux
