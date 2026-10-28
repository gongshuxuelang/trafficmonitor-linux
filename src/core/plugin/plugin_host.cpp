#include "core/plugin/plugin_host.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace tm_linux {

namespace {

using PluginFactory = const TMPluginV1* (*)(uint32_t);

/**
 * @brief 取出并清空动态加载器的错误信息。
 * @return 错误文本；无错误时返回通用描述。
 */
std::string takeDlError()
{
    const char* error = ::dlerror();
    return error != nullptr ? std::string(error) : std::string("unknown dynamic loader error");
}

/**
 * @brief 计算默认的插件配置目录。
 *
 * 优先使用 XDG_CONFIG_HOME，其次 HOME/.config，最后回退到 /tmp。
 *
 * @return 配置目录路径。
 */
std::string defaultConfigDir()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "trafficmonitor-linux" / "plugins";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "trafficmonitor-linux" / "plugins";
    }
    return "/tmp/trafficmonitor-linux/plugins";
}

/**
 * @brief 根据数据是否有效返回对应的标志位。
 * @param valid 数据是否有效。
 * @param flag 标志位。
 * @return 有效时返回 flag，否则返回 0。
 */
uint64_t validFlag(bool valid, uint32_t flag)
{
    return valid ? flag : 0;
}

} // namespace

PluginHost::PluginHost(std::filesystem::path plugin_dir)
    : plugin_dir_(std::move(plugin_dir))
    , config_dir_(defaultConfigDir())
{
    std::error_code error;
    std::filesystem::create_directories(config_dir_, error);

    host_api_.struct_size = sizeof(TMPluginHostV1);
    host_api_.api_version = TM_PLUGIN_API_VERSION;
    host_api_.host = this;
    host_api_.get_monitor_value = [](void*, uint32_t) -> uint64_t {
        return 0;
    };
    host_api_.get_config_dir = [](void* host) -> const char* {
        return static_cast<PluginHost*>(host)->config_dir_.c_str();
    };
    host_api_.get_dpi = [](void* host, uint32_t) -> uint32_t {
        return static_cast<PluginHost*>(host)->dpi_;
    };
    host_api_.log = [](void*, uint32_t, const char* message) {
        std::fprintf(stderr, "[trafficmonitor plugin] %s\n", message != nullptr ? message : "");
    };
    host_api_.request_repaint = [](void*) {
    };
}

PluginHost::~PluginHost()
{
    unloadAll();
}

bool PluginHost::loadFile(const std::filesystem::path& path)
{
    last_error_.clear();

    LoadedPlugin record;
    record.path = path;

    auto fail = [this, &record](std::string message) {
        record.loaded = false;
        record.error = std::move(message);
        last_error_ = record.error;
        if (record.handle != nullptr) {
            ::dlclose(record.handle);
            record.handle = nullptr;
        }
        plugins_.push_back(std::move(record));
        return false;
    };

    void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        return fail(takeDlError());
    }
    record.handle = handle;

    const auto factory = reinterpret_cast<PluginFactory>(::dlsym(handle, "tm_plugin_get_instance"));
    if (factory == nullptr) {
        return fail("missing tm_plugin_get_instance: " + takeDlError());
    }

    const TMPluginV1* plugin = factory(TM_PLUGIN_API_VERSION);
    if (plugin == nullptr) {
        return fail("tm_plugin_get_instance returned null");
    }
    record.api = plugin;

    if (plugin->struct_size < sizeof(TMPluginV1)) {
        return fail("plugin API struct is smaller than the host ABI");
    }
    if ((plugin->api_version >> 16) != TM_PLUGIN_API_MAJOR) {
        return fail("unsupported plugin API major version");
    }
    if (plugin->plugin_id == nullptr || *plugin->plugin_id == '\0') {
        return fail("plugin_id is empty");
    }
    if (plugin->initialize == nullptr || plugin->update == nullptr
        || plugin->get_item_count == nullptr || plugin->get_item == nullptr) {
        return fail("plugin is missing required callbacks");
    }

    for (const auto& loaded : plugins_) {
        if (loaded.loaded && loaded.pluginId == plugin->plugin_id) {
            return fail("duplicate plugin_id: " + std::string(plugin->plugin_id));
        }
    }

    const TMResult initialized = plugin->initialize(plugin->user_data, &host_api_);
    if (initialized != TM_OK) {
        if (plugin->shutdown != nullptr) {
            plugin->shutdown(plugin->user_data);
        }
        return fail("plugin initialize failed with code "
                    + std::to_string(static_cast<int>(initialized)));
    }

    record.pluginId = plugin->plugin_id;
    record.name = plugin->name != nullptr ? plugin->name : "";
    record.version = plugin->version != nullptr ? plugin->version : "";
    record.author = plugin->author != nullptr ? plugin->author : "";
    record.description = plugin->description != nullptr ? plugin->description : "";

    const uint32_t item_count = plugin->get_item_count(plugin->user_data);
    if (item_count > 256) {
        if (plugin->shutdown != nullptr) {
            plugin->shutdown(plugin->user_data);
        }
        return fail("plugin exposes more than 256 items");
    }

    std::vector<std::string> seen_item_ids;
    for (const auto& loaded : plugins_) {
        for (const auto& item : loaded.items) {
            seen_item_ids.push_back(item.itemId);
        }
    }

    for (uint32_t index = 0; index < item_count; ++index) {
        const TMPluginItemV1* item = plugin->get_item(plugin->user_data, index);
        if (item == nullptr) {
            if (plugin->shutdown != nullptr) {
                plugin->shutdown(plugin->user_data);
            }
            return fail("plugin returned a null item at index " + std::to_string(index));
        }
        if (item->struct_size < sizeof(TMPluginItemV1)) {
            if (plugin->shutdown != nullptr) {
                plugin->shutdown(plugin->user_data);
            }
            return fail("plugin item struct is smaller than the host ABI");
        }
        if (item->item_id == nullptr || *item->item_id == '\0') {
            if (plugin->shutdown != nullptr) {
                plugin->shutdown(plugin->user_data);
            }
            return fail("plugin item id is empty");
        }
        if (item->get_value_text == nullptr) {
            if (plugin->shutdown != nullptr) {
                plugin->shutdown(plugin->user_data);
            }
            return fail("plugin item has no get_value_text callback");
        }
        if (std::find(seen_item_ids.begin(), seen_item_ids.end(), item->item_id)
            != seen_item_ids.end()) {
            if (plugin->shutdown != nullptr) {
                plugin->shutdown(plugin->user_data);
            }
            return fail("duplicate item_id: " + std::string(item->item_id));
        }
        seen_item_ids.emplace_back(item->item_id);

        PluginItemView view;
        view.itemId = item->item_id;
        view.displayName = item->display_name != nullptr ? item->display_name : item->item_id;
        view.labelText = item->label_text != nullptr ? item->label_text : "";
        view.sampleText = item->sample_text != nullptr ? item->sample_text : "";
        view.customDraw = (item->flags & TM_ITEM_CUSTOM_DRAW) != 0;
        view.api = item;
        record.items.push_back(std::move(view));
    }

    record.loaded = true;
    plugins_.push_back(std::move(record));
    return true;
}

void PluginHost::loadAll()
{
    if (plugin_dir_.empty()) {
        return;
    }

    std::error_code error;
    std::vector<std::filesystem::path> candidates;
    std::filesystem::directory_iterator iterator(plugin_dir_, error);
    if (error) {
        setError("cannot scan plugin directory: " + error.message());
        return;
    }

    for (const auto& entry : iterator) {
        if (!entry.is_regular_file(error)) {
            continue;
        }
        if (entry.path().extension() == ".so") {
            candidates.push_back(entry.path());
        }
    }
    std::sort(candidates.begin(), candidates.end());

    for (const auto& candidate : candidates) {
        loadFile(candidate);
    }
}

void PluginHost::update(const Snapshot& snapshot)
{
    TMMetricSnapshot api_snapshot{};
    api_snapshot.struct_size = sizeof(TMMetricSnapshot);
    api_snapshot.api_version = TM_PLUGIN_API_VERSION;
    api_snapshot.timestamp_unix_ms = static_cast<uint64_t>(snapshot.timestamp_unix_ms);
    api_snapshot.cpu_usage_percent = snapshot.cpu.usage_percent;
    api_snapshot.cpu_freq_mhz = snapshot.cpu.frequency_mhz;
    api_snapshot.today_up_bytes = snapshot.today_up_bytes;
    api_snapshot.today_down_bytes = snapshot.today_down_bytes;

    if (snapshot.network.has_value()) {
        api_snapshot.up_bytes_per_sec =
            static_cast<uint64_t>(std::max(0.0, snapshot.network->tx_bytes_per_sec));
        api_snapshot.down_bytes_per_sec =
            static_cast<uint64_t>(std::max(0.0, snapshot.network->rx_bytes_per_sec));
        api_snapshot.valid_flags |= TM_VALID_UP | TM_VALID_DOWN;
    }
    if (snapshot.memory.has_value()) {
        api_snapshot.memory_usage_percent = snapshot.memory->used_percent;
        api_snapshot.memory_used_bytes = snapshot.memory->used_bytes;
        api_snapshot.memory_total_bytes = snapshot.memory->total_bytes;
        api_snapshot.valid_flags |= TM_VALID_MEMORY;
    }
    if (snapshot.disk.has_value()) {
        api_snapshot.disk_space_used_percent = snapshot.disk->used_percent;
        api_snapshot.valid_flags |= TM_VALID_DISK_SPACE;
        if (snapshot.disk->io_available) {
            api_snapshot.disk_io_util_percent = snapshot.disk->io_util_percent;
            api_snapshot.valid_flags |= TM_VALID_DISK_IO;
        }
    }
    if (snapshot.temperature.cpu_c.has_value()) {
        api_snapshot.cpu_temp_c = *snapshot.temperature.cpu_c;
        api_snapshot.valid_flags |= TM_VALID_CPU_TEMP;
    }
    if (snapshot.temperature.gpu_c.has_value()) {
        api_snapshot.gpu_temp_c = *snapshot.temperature.gpu_c;
        api_snapshot.valid_flags |= TM_VALID_GPU_TEMP;
    }
    if (snapshot.temperature.hdd_c.has_value()) {
        api_snapshot.hdd_temp_c = *snapshot.temperature.hdd_c;
        api_snapshot.valid_flags |= TM_VALID_HDD_TEMP;
    }
    if (snapshot.temperature.mainboard_c.has_value()) {
        api_snapshot.mainboard_temp_c = *snapshot.temperature.mainboard_c;
        api_snapshot.valid_flags |= TM_VALID_MAINBOARD_TEMP;
    }
    api_snapshot.valid_flags |= TM_VALID_CPU | TM_VALID_CPU_FREQ | TM_VALID_TODAY_UP
        | TM_VALID_TODAY_DOWN;

    for (auto& plugin : plugins_) {
        if (plugin.loaded && plugin.api != nullptr && plugin.api->update != nullptr) {
            plugin.api->update(plugin.api->user_data, &api_snapshot);
        }
    }
}

void PluginHost::refreshItemValues()
{
    for (auto& plugin : plugins_) {
        if (!plugin.loaded) {
            continue;
        }
        for (auto& item : plugin.items) {
            if (item.api == nullptr || item.api->get_value_text == nullptr) {
                item.value = "--";
                continue;
            }

            const TMStringView view = item.api->get_value_text(item.api->user_data);
            if (view.data != nullptr && view.size > 0) {
                item.value.assign(view.data, view.size);
            } else {
                item.value = "--";
            }
        }
    }
}

const std::vector<LoadedPlugin>& PluginHost::plugins() const
{
    return plugins_;
}

std::vector<PluginItemView> PluginHost::allItems() const
{
    std::vector<PluginItemView> result;
    for (const auto& plugin : plugins_) {
        if (!plugin.loaded) {
            continue;
        }
        result.insert(result.end(), plugin.items.begin(), plugin.items.end());
    }
    return result;
}

const std::string& PluginHost::lastError() const
{
    return last_error_;
}

void PluginHost::setConfigDir(std::filesystem::path config_dir)
{
    config_dir_ = std::move(config_dir);
    std::error_code error;
    std::filesystem::create_directories(config_dir_, error);
}

void PluginHost::setDpi(std::uint32_t dpi)
{
    dpi_ = dpi;
}

void PluginHost::setError(std::string message)
{
    last_error_ = std::move(message);
}

void PluginHost::unloadAll()
{
    for (auto& plugin : plugins_) {
        if (plugin.loaded && plugin.api != nullptr) {
            if (plugin.api->shutdown != nullptr) {
                plugin.api->shutdown(plugin.api->user_data);
            }
        }
        if (plugin.handle != nullptr) {
            ::dlclose(plugin.handle);
            plugin.handle = nullptr;
        }
        plugin.loaded = false;
    }
    plugins_.clear();
}

} // namespace tm_linux
