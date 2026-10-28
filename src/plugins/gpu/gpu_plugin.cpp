#include "core/plugin/tm_plugin_api.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <mutex>

namespace {

constexpr int kNvmlSuccess = 0;
constexpr int kNvmlTemperatureGpu = 0;

using NvmlReturn = int;
using NvmlDevice = void*;

struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

using NvmlInit = NvmlReturn (*)();
using NvmlShutdown = NvmlReturn (*)();
using NvmlDeviceGetCount = NvmlReturn (*)(unsigned int*);
using NvmlDeviceGetHandleByIndex = NvmlReturn (*)(unsigned int, NvmlDevice*);
using NvmlDeviceGetUtilizationRates = NvmlReturn (*)(NvmlDevice, NvmlUtilization*);
using NvmlDeviceGetMemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory*);
using NvmlDeviceGetTemperature = NvmlReturn (*)(NvmlDevice, int, unsigned int*);

struct ItemState {
    std::array<char, 96> text{};
};

ItemState g_usage;
ItemState g_memory;
ItemState g_temperature;

std::mutex g_mutex;
void* g_nvml_handle = nullptr;
NvmlInit g_nvml_init = nullptr;
NvmlShutdown g_nvml_shutdown = nullptr;
NvmlDeviceGetCount g_nvml_device_count = nullptr;
NvmlDeviceGetHandleByIndex g_nvml_handle_by_index = nullptr;
NvmlDeviceGetUtilizationRates g_nvml_utilization = nullptr;
NvmlDeviceGetMemoryInfo g_nvml_memory = nullptr;
NvmlDeviceGetTemperature g_nvml_temperature = nullptr;
bool g_available = false;
std::chrono::steady_clock::time_point g_last_attempt{};

/**
 * @brief 将文本写入条目的显示缓冲区。
 * @param state 条目状态。
 * @param text 文本，超出缓冲区时会被截断。
 */
void setText(ItemState& state, const char* text)
{
    std::snprintf(state.text.data(), state.text.size(), "%s", text);
}

/**
 * @brief 从动态库中解析函数符号。
 * @tparam Function 函数指针类型。
 * @param handle 动态库句柄。
 * @param name 符号名。
 * @return 解析到的函数指针；失败时返回 nullptr。
 */
template <typename Function>
Function resolve(void* handle, const char* name)
{
    return reinterpret_cast<Function>(::dlsym(handle, name));
}

/**
 * @brief 确保 NVML 已加载并完成初始化。
 *
 * 加载失败后 5 秒内不会重复尝试，避免频繁 dlopen。
 *
 * @return NVML 可用返回 true。
 */
bool ensureNvmlLocked()
{
    if (g_available) {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (g_last_attempt.time_since_epoch().count() != 0
        && std::chrono::duration<double>(now - g_last_attempt).count() < 5.0) {
        return false;
    }
    g_last_attempt = now;

    g_nvml_handle = ::dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
    if (g_nvml_handle == nullptr) {
        return false;
    }

    g_nvml_init = resolve<NvmlInit>(g_nvml_handle, "nvmlInit_v2");
    g_nvml_shutdown = resolve<NvmlShutdown>(g_nvml_handle, "nvmlShutdown");
    g_nvml_device_count = resolve<NvmlDeviceGetCount>(g_nvml_handle, "nvmlDeviceGetCount_v2");
    g_nvml_handle_by_index =
        resolve<NvmlDeviceGetHandleByIndex>(g_nvml_handle, "nvmlDeviceGetHandleByIndex_v2");
    g_nvml_utilization =
        resolve<NvmlDeviceGetUtilizationRates>(g_nvml_handle, "nvmlDeviceGetUtilizationRates");
    g_nvml_memory = resolve<NvmlDeviceGetMemoryInfo>(g_nvml_handle, "nvmlDeviceGetMemoryInfo");
    g_nvml_temperature =
        resolve<NvmlDeviceGetTemperature>(g_nvml_handle, "nvmlDeviceGetTemperature");

    if (g_nvml_init == nullptr || g_nvml_shutdown == nullptr || g_nvml_device_count == nullptr
        || g_nvml_handle_by_index == nullptr || g_nvml_utilization == nullptr
        || g_nvml_memory == nullptr || g_nvml_temperature == nullptr
        || g_nvml_init() != kNvmlSuccess) {
        if (g_nvml_handle != nullptr) {
            ::dlclose(g_nvml_handle);
            g_nvml_handle = nullptr;
        }
        return false;
    }

    g_available = true;
    return true;
}

/**
 * @brief 读取第一块 GPU 的使用率、显存与温度并更新显示文本。
 *
 * NVML 不可用或查询失败时，对应条目显示 "--"。
 */
void refreshValues()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!ensureNvmlLocked()) {
        setText(g_usage, "--");
        setText(g_memory, "--");
        setText(g_temperature, "--");
        return;
    }

    unsigned int device_count = 0;
    if (g_nvml_device_count(&device_count) != kNvmlSuccess || device_count == 0) {
        setText(g_usage, "--");
        setText(g_memory, "--");
        setText(g_temperature, "--");
        return;
    }

    NvmlDevice device = nullptr;
    if (g_nvml_handle_by_index(0, &device) != kNvmlSuccess || device == nullptr) {
        setText(g_usage, "--");
        setText(g_memory, "--");
        setText(g_temperature, "--");
        return;
    }

    NvmlUtilization utilization{};
    if (g_nvml_utilization(device, &utilization) == kNvmlSuccess) {
        std::snprintf(g_usage.text.data(), g_usage.text.size(), "%u%%", utilization.gpu);
    } else {
        setText(g_usage, "--");
    }

    NvmlMemory memory{};
    if (g_nvml_memory(device, &memory) == kNvmlSuccess) {
        std::snprintf(g_memory.text.data(), g_memory.text.size(), "%llu / %llu MiB",
                      memory.used / (1024ull * 1024ull), memory.total / (1024ull * 1024ull));
    } else {
        setText(g_memory, "--");
    }

    unsigned int temperature = 0;
    if (g_nvml_temperature(device, kNvmlTemperatureGpu, &temperature) == kNvmlSuccess) {
        std::snprintf(g_temperature.text.data(), g_temperature.text.size(), "%u°C", temperature);
    } else {
        setText(g_temperature, "--");
    }
}

/**
 * @brief 线程安全地获取条目文本视图。
 * @param state 条目状态。
 * @return 指向内部缓冲区的文本视图。
 */
TMStringView valueView(const ItemState& state)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return TMStringView{state.text.data(), std::strlen(state.text.data())};
}

/**
 * @brief 插件 ABI 的取值回调。
 * @param user_data 指向 ItemState 的用户数据。
 * @return 当前显示文本。
 */
TMStringView itemValue(void* user_data)
{
    return valueView(*static_cast<const ItemState*>(user_data));
}

const TMPluginItemV1 kItems[]{
    {
        sizeof(TMPluginItemV1),
        TM_PLUGIN_API_VERSION,
        "trafficmonitor.gpu.usage",
        "GPU 使用率",
        "GPU",
        "5%",
        0,
        &g_usage,
        itemValue,
        nullptr,
        nullptr,
        nullptr,
    },
    {
        sizeof(TMPluginItemV1),
        TM_PLUGIN_API_VERSION,
        "trafficmonitor.gpu.memory",
        "GPU 显存",
        "VRAM",
        "1024 / 4096 MiB",
        0,
        &g_memory,
        itemValue,
        nullptr,
        nullptr,
        nullptr,
    },
    {
        sizeof(TMPluginItemV1),
        TM_PLUGIN_API_VERSION,
        "trafficmonitor.gpu.temp",
        "GPU 温度",
        "GPU",
        "45°C",
        0,
        &g_temperature,
        itemValue,
        nullptr,
        nullptr,
        nullptr,
    },
};

/**
 * @brief 插件初始化回调：立即刷新一次 GPU 数值。
 * @return 固定返回 TM_OK。
 */
TMResult initializePlugin(void*, const TMPluginHostV1*)
{
    refreshValues();
    return TM_OK;
}

/**
 * @brief 插件关闭回调：关闭 NVML 并释放动态库句柄。
 */
void shutdownPlugin(void*)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_available && g_nvml_shutdown != nullptr) {
        g_nvml_shutdown();
    }
    if (g_nvml_handle != nullptr) {
        ::dlclose(g_nvml_handle);
        g_nvml_handle = nullptr;
    }
    g_available = false;
    g_nvml_init = nullptr;
    g_nvml_shutdown = nullptr;
    g_nvml_device_count = nullptr;
    g_nvml_handle_by_index = nullptr;
    g_nvml_utilization = nullptr;
    g_nvml_memory = nullptr;
    g_nvml_temperature = nullptr;
}

/**
 * @brief 插件更新回调：收到指标更新后刷新 GPU 数值。
 */
void updatePlugin(void*, const TMMetricSnapshot*)
{
    refreshValues();
}

/**
 * @brief 返回插件条目数量。
 * @return 条目数。
 */
uint32_t itemCount(void*)
{
    return sizeof(kItems) / sizeof(kItems[0]);
}

/**
 * @brief 按索引返回插件条目。
 * @param index 条目索引。
 * @return 条目指针；越界时返回 nullptr。
 */
const TMPluginItemV1* getItem(void*, uint32_t index)
{
    if (index >= sizeof(kItems) / sizeof(kItems[0])) {
        return nullptr;
    }
    return &kItems[index];
}

/**
 * @brief 返回插件提示文本。
 * @return 文本视图。
 */
TMStringView tooltip(void*)
{
    static const char text[] = "NVIDIA GPU metrics";
    return TMStringView{text, sizeof(text) - 1};
}

const TMPluginV1 kPlugin{
    sizeof(TMPluginV1),
    TM_PLUGIN_API_VERSION,
    "trafficmonitor.gpu",
    "TrafficMonitor GPU Monitor",
    "0.1.0",
    "TrafficMonitor for Linux",
    "NVIDIA GPU utilization, VRAM and temperature",
    "",
    "",
    nullptr,
    initializePlugin,
    shutdownPlugin,
    updatePlugin,
    itemCount,
    getItem,
    nullptr,
    nullptr,
    tooltip,
    nullptr,
};

} // namespace

/**
 * @brief 插件入口：返回插件实例。
 *
 * host_api_version 参数当前未使用。
 *
 * @return 插件描述结构指针。
 */
extern "C" TM_PLUGIN_EXPORT const TMPluginV1* tm_plugin_get_instance(uint32_t)
{
    return &kPlugin;
}
